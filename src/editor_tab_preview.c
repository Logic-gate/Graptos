/**
 * @file src/editor_tab_preview.c
 * @brief Graptoς editor tab preview module.
 * @details Editor tabs hold the real editing surface. We split the implementation by
 *          lifecycle, input, rendering, preview, and transient UI because each part has
 *          different timing and cleanup pressure.
 */

#include "editor_tab_private.h"
#include "config.h"

#include <stdarg.h>
#include <string.h>

/**
 * @brief Preview max bytes macro.
 */
#define PREVIEW_MAX_BYTES (2u * 1024u * 1024u)

/**
 * @brief Editor tab preview type definition.
 */
typedef struct {
    GtkTextBuffer *buffer; /**< Buffer. */
    GtkTextIter iter; /**< Iter. */
} PreviewWriter;

/**
 * @brief Editor tab preview type definition.
 */
typedef struct {
    GtkWidget *scrolled; /**< Scrolled. */
    gdouble value; /**< Value. */
} PreviewScrollRestore;

/**
 * @brief Preview restore scroll cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean preview_restore_scroll_cb(gpointer user_data) {
    PreviewScrollRestore *restore = user_data;
    if (!restore || !restore->scrolled) {
        g_free(restore);
        return G_SOURCE_REMOVE;
    }
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(restore->scrolled));
    if (adj) {
        gdouble max = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
        if (max < 0.0) max = 0.0;
        gtk_adjustment_set_value(adj, CLAMP(restore->value, 0.0, max));
    }
    g_object_unref(restore->scrolled);
    g_free(restore);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Preview restore scroll later.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param scrolled The scrolled supplied by the caller.
 * @param value The value being parsed, stored, or applied.
 */
static void preview_restore_scroll_later(GtkWidget *scrolled, gdouble value) {
    if (!scrolled) return;
    PreviewScrollRestore *restore = g_new0(PreviewScrollRestore, 1);
    restore->scrolled = g_object_ref(scrolled);
    restore->value = value;
    g_idle_add_full(G_PRIORITY_LOW, preview_restore_scroll_cb, restore, NULL);
}

/**
 * @brief Str has prefix.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean str_has_prefix(const char *text, const char *prefix) {
    return text && prefix && g_str_has_prefix(text, prefix);
}

/**
 * @brief Syntax name is.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param name The name supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean syntax_name_is(EditorTab *tab, const char *name) {
    const char *syntax = NULL;
    if (tab && tab->active_syntax) syntax = tab->active_syntax->name;
    return syntax && name && g_ascii_strcasecmp(syntax, name) == 0;
}

/**
 * @brief Path has suffix.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param suffix The suffix supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean path_has_suffix(EditorTab *tab, const char *suffix) {
    if (!tab || !tab->file_path || !suffix) return FALSE;
    g_autofree char *path = g_ascii_strdown(tab->file_path, -1);
    g_autofree char *needle = g_ascii_strdown(suffix, -1);
    return path && needle && g_str_has_suffix(path, needle);
}

/**
 * @brief Preview is markdown.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean preview_is_markdown(EditorTab *tab) {
    return syntax_name_is(tab, "Markdown") || path_has_suffix(tab, ".md") ||
           path_has_suffix(tab, ".markdown");
}

/**
 * @brief Handle a detached preview window close request.
 * @details Native window close is treated as turning preview off. The preview
 *          widget is reattached hidden before the transient window is destroyed
 *          so View -> Preview can show it again without rebuilding tab layout.
 * @param window The detached preview window.
 * @param user_data The editor tab that owns the preview.
 * @return TRUE because the callback handles the close request.
 */
static gboolean preview_window_close_request(GtkWindow *window, gpointer user_data) {
    (void)window;
    EditorTab *tab = user_data;
    EditorWindow *win = tab ? tab->win : NULL;
    if (!win) return TRUE;

    /*
     * Native close means the user is done with preview, not just done with
     * detached placement. Keep the preview widgets owned by their tabs so the
     * normal View -> Preview toggle can bring them back later.
     */
    win->preview_enabled = FALSE;
    for (guint i = 0u; i < app_window_tab_count(win); i++) {
        EditorTab *other = app_window_tab_at(win, i);
        if (!other) continue;
        if (other->preview_detached) editor_tab_preview_reattach(other);
        editor_tab_set_preview_visible(other, FALSE);
    }
    graptos_config_save(win);
    app_window_update_ui(win);
    return TRUE;
}

/**
 * @brief Return whether the preview should be visible for the tab.
 * @details Both attached and detached preview placement use the same policy:
 *          the global preview toggle must be on and the current document must
 *          be a previewable type.
 * @param tab The editor tab whose preview state is being queried.
 * @return TRUE when the preview should be shown.
 */
static gboolean preview_should_be_visible(EditorTab *tab) {
    return tab && tab->win && tab->win->preview_enabled && preview_is_supported(tab);
}

/**
 * @brief Set the preview container visibility in its current parent.
 * @details Detached previews are top-level windows, while attached previews
 *          are children of the tab splitter. The rendering code should not
 *          care which parent currently owns the widget.
 * @param tab The editor tab whose preview visibility should be updated.
 * @param visible TRUE to show the preview, FALSE to hide it.
 */
static void preview_set_container_visible(EditorTab *tab, gboolean visible) {
    if (!tab) return;
    if (tab->preview_detached && tab->preview_window) {
        gtk_widget_set_visible(tab->preview_window, visible);
    } else if (tab->preview_box) {
        gtk_widget_set_visible(tab->preview_box, visible);
    }
}

/**
 * @brief Detach the rendered preview into a separate window.
 * @details Moving the existing preview container preserves scroll position,
 *          text tags, and the live GtkTextBuffer. We hold a temporary
 *          reference while changing parents because GTK drops a child reference
 *          when it is removed from a container.
 * @param tab The editor tab whose preview should move into a window.
 */
void editor_tab_preview_detach(EditorTab *tab) {
    if (!tab || !tab->preview_box || tab->preview_detached) return;

    g_object_ref(tab->preview_box);
    if (tab->preview_paned &&
        gtk_paned_get_end_child(GTK_PANED(tab->preview_paned)) == tab->preview_box) {
        gtk_paned_set_end_child(GTK_PANED(tab->preview_paned), NULL);
    } else {
        GtkWidget *parent = gtk_widget_get_parent(tab->preview_box);
        if (parent) gtk_widget_unparent(tab->preview_box);
    }

    tab->preview_window = gtk_window_new();
    gtk_widget_add_css_class(tab->preview_window, "graptos-window");
    gtk_widget_add_css_class(tab->preview_window, "graptos-preview-window");
    gtk_window_set_title(GTK_WINDOW(tab->preview_window), "Graptoς Preview");
    gtk_window_set_default_size(GTK_WINDOW(tab->preview_window), 520, 720);
    if (tab->win && tab->win->window) {
        gtk_window_set_transient_for(GTK_WINDOW(tab->preview_window),
                                     GTK_WINDOW(tab->win->window));
    }
    g_signal_connect(tab->preview_window, "close-request",
                     G_CALLBACK(preview_window_close_request), tab);
    gtk_window_set_child(GTK_WINDOW(tab->preview_window), tab->preview_box);
    tab->preview_detached = TRUE;
    if (tab->preview_detach_button) {
        gtk_button_set_icon_name(GTK_BUTTON(tab->preview_detach_button),
                                 "view-restore-symbolic");
        gtk_widget_set_tooltip_text(tab->preview_detach_button,
                                    "Reattach preview");
    }
    gtk_widget_set_visible(tab->preview_box, TRUE);
    if (preview_should_be_visible(tab)) gtk_window_present(GTK_WINDOW(tab->preview_window));
    else gtk_widget_set_visible(tab->preview_window, FALSE);
    g_object_unref(tab->preview_box);
}

/**
 * @brief Reattach the rendered preview to the editor splitter.
 * @details The preview window is only a temporary parent. Reattaching returns
 *          the same preview widgets to the tab without rebuilding the rendered
 *          markdown contents.
 * @param tab The editor tab whose preview should return to the tab layout.
 */
void editor_tab_preview_reattach(EditorTab *tab) {
    if (!tab || !tab->preview_box || !tab->preview_paned) return;

    g_object_ref(tab->preview_box);
    if (tab->preview_window) {
        g_signal_handlers_disconnect_by_data(tab->preview_window, tab);
        if (gtk_window_get_child(GTK_WINDOW(tab->preview_window)) == tab->preview_box) {
            gtk_window_set_child(GTK_WINDOW(tab->preview_window), NULL);
        }
        gtk_window_destroy(GTK_WINDOW(tab->preview_window));
        tab->preview_window = NULL;
    } else {
        GtkWidget *parent = gtk_widget_get_parent(tab->preview_box);
        if (parent && parent != tab->preview_paned) gtk_widget_unparent(tab->preview_box);
    }

    if (gtk_paned_get_end_child(GTK_PANED(tab->preview_paned)) != tab->preview_box) {
        gtk_paned_set_end_child(GTK_PANED(tab->preview_paned), tab->preview_box);
    }
    tab->preview_detached = FALSE;
    if (tab->preview_detach_button) {
        gtk_button_set_icon_name(GTK_BUTTON(tab->preview_detach_button),
                                 "window-new-symbolic");
        gtk_widget_set_tooltip_text(tab->preview_detach_button,
                                    "Detach preview");
    }
    gtk_widget_set_visible(tab->preview_box, preview_should_be_visible(tab));
    g_object_unref(tab->preview_box);
}

/**
 * @brief Close any detached preview window owned by the tab.
 * @details Teardown must make the temporary window forget the tab before GTK
 *          destroys widgets asynchronously. That avoids close callbacks using
 *          freed editor-tab memory while the application exits.
 * @param tab The editor tab whose detached preview should be closed.
 */
void editor_tab_preview_close_detached(EditorTab *tab) {
    if (!tab || !tab->preview_window) return;
    g_signal_handlers_disconnect_by_data(tab->preview_window, tab);
    if (gtk_window_get_child(GTK_WINDOW(tab->preview_window)) == tab->preview_box) {
        gtk_window_set_child(GTK_WINDOW(tab->preview_window), NULL);
    }
    gtk_window_destroy(GTK_WINDOW(tab->preview_window));
    tab->preview_window = NULL;
    tab->preview_detached = FALSE;
}

/**
 * @brief Toggle preview attach state from an idle callback.
 * @details The clicked icon button is inside the preview container. Moving that
 *          container while GTK is still resolving the click can leave GTK
 *          asking for coordinates from a widget whose native parent just
 *          changed. Deferring the actual move by one idle tick keeps the event
 *          path stable.
 * @param user_data The editor tab that owns the preview widgets.
 * @return G_SOURCE_REMOVE after the deferred move has completed.
 */
gboolean preview_reparent_idle_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab || tab->disposing) return G_SOURCE_REMOVE;
    tab->preview_reparent_idle = 0u;
    if (tab->preview_detached) editor_tab_preview_reattach(tab);
    else editor_tab_preview_detach(tab);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Editor tab is latex.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_is_latex(EditorTab *tab) {
    return syntax_name_is(tab, "LaTeX") || syntax_name_is(tab, "Tex") ||
           path_has_suffix(tab, ".tex") || path_has_suffix(tab, ".latex");
}

/**
 * @brief Preview is supported.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean preview_is_supported(EditorTab *tab) {
    return preview_is_markdown(tab) || editor_tab_is_latex(tab);
}

/**
 * @brief Ensure tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param name The name supplied by the caller.
 * @param first_property The first property supplied by the caller.
 */
static void ensure_tag(GtkTextBuffer *buffer,
                       const char *name,
                       const char *first_property,
                       ...) {
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    if (gtk_text_tag_table_lookup(table, name)) return;

    GtkTextTag *tag = gtk_text_tag_new(name);
    va_list args;
    va_start(args, first_property);
    g_object_set_valist(G_OBJECT(tag), first_property, args);
    va_end(args);
    gtk_text_tag_table_add(table, tag);
    g_object_unref(tag);
}

/**
 * @brief Preview ensure tags.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void preview_ensure_tags(EditorTab *tab) {
    if (!tab || !tab->preview_buffer) return;
    GtkTextBuffer *buffer = tab->preview_buffer;
    ensure_tag(buffer, "preview-h1", "weight", PANGO_WEIGHT_BOLD,
               "scale", 1.65, "pixels-above-lines", 12, NULL);
    ensure_tag(buffer, "preview-h2", "weight", PANGO_WEIGHT_BOLD,
               "scale", 1.35, "pixels-above-lines", 10, NULL);
    ensure_tag(buffer, "preview-h3", "weight", PANGO_WEIGHT_BOLD,
               "scale", 1.15, "pixels-above-lines", 8, NULL);
    ensure_tag(buffer, "preview-h4", "weight", PANGO_WEIGHT_BOLD,
               "scale", 1.08, "pixels-above-lines", 6, NULL);
    ensure_tag(buffer, "preview-h5", "weight", PANGO_WEIGHT_BOLD,
               "scale", 1.02, "pixels-above-lines", 5, NULL);
    ensure_tag(buffer, "preview-h6", "weight", PANGO_WEIGHT_BOLD,
               "scale", 0.98, "pixels-above-lines", 4, NULL);
    ensure_tag(buffer, "preview-bold", "weight", PANGO_WEIGHT_BOLD, NULL);
    ensure_tag(buffer, "preview-italic", "style", PANGO_STYLE_ITALIC, NULL);
    ensure_tag(buffer, "preview-strike", "strikethrough", TRUE, NULL);
    ensure_tag(buffer, "preview-code", "family", "monospace",
               "background", "#2A2E3D", NULL);
    ensure_tag(buffer, "preview-quote", "style", PANGO_STYLE_ITALIC,
               "foreground", "#8FA1B3", NULL);
    ensure_tag(buffer, "preview-link", "underline", PANGO_UNDERLINE_SINGLE,
               "foreground", "#61AFEF", NULL);
    ensure_tag(buffer, "preview-table", "family", "monospace", NULL);
    ensure_tag(buffer, "preview-task", "family", "monospace",
               "weight", PANGO_WEIGHT_BOLD, NULL);
    ensure_tag(buffer, "preview-rule", "foreground", "#8FA1B3", NULL);
    ensure_tag(buffer, "preview-math", "family", "monospace",
               "foreground", "#C586C0", NULL);
    ensure_tag(buffer, "preview-command", "weight", PANGO_WEIGHT_BOLD,
               "foreground", "#4EC9B0", NULL);
}

/**
 * @brief Writer init.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param writer The writer supplied by the caller.
 * @param buffer The text buffer used for the operation.
 */
static void writer_init(PreviewWriter *writer, GtkTextBuffer *buffer) {
    writer->buffer = buffer;
    gtk_text_buffer_get_end_iter(buffer, &writer->iter);
}

/**
 * @brief Writer insert.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param writer The writer supplied by the caller.
 * @param text The text fragment supplied by the caller.
 */
static void writer_insert(PreviewWriter *writer, const char *text) {
    gtk_text_buffer_insert(writer->buffer, &writer->iter, text ? text : "", -1);
}

/**
 * @brief Writer insert tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param writer The writer supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param tag The tag supplied by the caller.
 */
static void writer_insert_tag(PreviewWriter *writer,
                              const char *text,
                              const char *tag) {
    if (!writer || !writer->buffer || !tag) return;
    gint start_offset = gtk_text_iter_get_offset(&writer->iter);
    gtk_text_buffer_insert(writer->buffer, &writer->iter, text ? text : "", -1);

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_iter_at_offset(writer->buffer, &start, start_offset);
    gtk_text_buffer_get_iter_at_offset(writer->buffer, &end,
                                       gtk_text_iter_get_offset(&writer->iter));
    gtk_text_buffer_apply_tag_by_name(writer->buffer, tag, &start, &end);
}

/**
 * @brief Line without prefix.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param count The count supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *line_without_prefix(const char *line, guint count) {
    const char *p = line ? line : "";
    while (*p == ' ' || *p == '\t') p++;
    while (count > 0u && *p != '\0') {
        p++;
        count--;
    }
    while (*p == ' ' || *p == '\t') p++;
    return g_strdup(p);
}

/**
 * @brief Return the first non-space character in a markdown line.
 * @details CommonMark allows several block markers to be indented by up to
 *          three spaces. The preview keeps that rule local instead of trimming
 *          every line globally, because code blocks and nested lists depend on
 *          leading whitespace.
 * @param line The markdown line being inspected.
 * @return Pointer into line at the first non-space character.
 */
static const char *markdown_skip_spaces(const char *line) {
    const char *p = line ? line : "";
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/**
 * @brief Count leading spaces in a markdown line.
 * @details Indented code and nested list hints need the original indentation
 *          count. Tabs are treated as four spaces for preview purposes.
 * @param line The markdown line being inspected.
 * @return The display indentation width.
 */
static guint markdown_leading_spaces(const char *line) {
    guint count = 0u;
    for (const char *p = line ? line : ""; *p == ' ' || *p == '\t'; p++) {
        count += (*p == '\t') ? 4u : 1u;
    }
    return count;
}

/**
 * @brief Return text after a target markdown indentation width.
 * @details Indented code can be created by spaces or tabs. This helper removes
 *          display indentation rather than blindly advancing bytes, so a tab at
 *          the start of a line does not make the preview skip real text.
 * @param line The markdown line being inspected.
 * @param target The display indentation width to consume.
 * @return Pointer into line after the requested indentation.
 */
static const char *markdown_after_indent(const char *line, guint target) {
    const char *p = line ? line : "";
    guint count = 0u;
    while (*p == ' ' || *p == '\t') {
        guint step = (*p == '\t') ? 4u : 1u;
        if (count + step > target) break;
        count += step;
        p++;
        if (count >= target) break;
    }
    return p;
}

/**
 * @brief Check whether a markdown line is blank.
 * @details Blank lines separate block constructs. Keeping this helper explicit
 *          avoids treating whitespace-only lines as paragraphs.
 * @param line The markdown line being inspected.
 * @return TRUE when the line has no visible characters.
 */
static gboolean markdown_is_blank(const char *line) {
    const char *p = markdown_skip_spaces(line);
    return *p == '\0';
}

/**
 * @brief Find a closing inline delimiter.
 * @details Inline rendering is intentionally local and conservative. Escaped
 *          delimiters are skipped so a literal marker stays visible.
 * @param start Pointer after the opening delimiter.
 * @param delimiter The delimiter to find.
 * @return Pointer to the closing delimiter, or NULL.
 */
static const char *markdown_find_closing(const char *start, const char *delimiter) {
    gsize len = strlen(delimiter ? delimiter : "");
    if (!start || len == 0u) return NULL;
    for (const char *p = start; *p != '\0'; p = g_utf8_next_char(p)) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
            continue;
        }
        if (strncmp(p, delimiter, len) == 0) return p;
    }
    return NULL;
}

/**
 * @brief Render one UTF-8 character from an inline markdown stream.
 * @details The preview writer works with GTK offsets, so inline parsing copies
 *          one complete UTF-8 character when no markdown delimiter is active.
 * @param writer The preview writer receiving output.
 * @param p Pointer to the current UTF-8 character.
 * @return Pointer to the next UTF-8 character.
 */
static const char *markdown_render_one_char(PreviewWriter *writer, const char *p) {
    const char *next = g_utf8_next_char(p);
    char *one = g_strndup(p, (gsize)(next - p));
    writer_insert(writer, one);
    g_free(one);
    return next;
}

/**
 * @brief Render markdown inline spans.
 * @details This covers the CommonMark/GFM inline forms that can be represented
 *          in a GtkTextBuffer: code spans, emphasis, strong emphasis,
 *          strikethrough, links, images, autolinks, and backslash escapes.
 *          HTML is kept as visible text because the preview does not execute or
 *          embed raw HTML.
 * @param writer The writer supplied by the caller.
 * @param line The markdown line being rendered.
 */
static void render_markdown_inline(PreviewWriter *writer, const char *line) {
    const char *p = line ? line : "";
    while (*p != '\0') {
        if (*p == '\\' && p[1] != '\0') {
            p = markdown_render_one_char(writer, p + 1);
            continue;
        }
        if (*p == '`') {
            const char *end = markdown_find_closing(p + 1, "`");
            if (end) {
                char *inner = g_strndup(p + 1, (gsize)(end - p - 1));
                writer_insert_tag(writer, inner, "preview-code");
                g_free(inner);
                p = end + 1;
                continue;
            }
        }
        if (str_has_prefix(p, "![") || *p == '[') {
            gboolean image = str_has_prefix(p, "![");
            const char *label_start = p + (image ? 2 : 1);
            const char *label_end = strchr(label_start, ']');
            if (label_end && label_end[1] == '(') {
                const char *url_end = strchr(label_end + 2, ')');
                if (url_end) {
                    char *label = g_strndup(label_start,
                                            (gsize)(label_end - label_start));
                    char *url = g_strndup(label_end + 2,
                                          (gsize)(url_end - label_end - 2));
                    if (image) writer_insert(writer, "🖼 ");
                    writer_insert_tag(writer, label, "preview-link");
                    if (url[0] != '\0') {
                        writer_insert(writer, " <");
                        writer_insert_tag(writer, url, "preview-link");
                        writer_insert(writer, ">");
                    }
                    g_free(label);
                    g_free(url);
                    p = url_end + 1;
                    continue;
                }
            }
        }
        if (*p == '<' &&
            (g_str_has_prefix(p + 1, "http://") ||
             g_str_has_prefix(p + 1, "https://") ||
             g_str_has_prefix(p + 1, "mailto:"))) {
            const char *end = strchr(p + 1, '>');
            if (end) {
                char *url = g_strndup(p + 1, (gsize)(end - p - 1));
                writer_insert_tag(writer, url, "preview-link");
                g_free(url);
                p = end + 1;
                continue;
            }
        }
        if (str_has_prefix(p, "***") || str_has_prefix(p, "___")) {
            const char *delimiter = str_has_prefix(p, "***") ? "***" : "___";
            const char *end = markdown_find_closing(p + 3, delimiter);
            if (end) {
                char *inner = g_strndup(p + 3, (gsize)(end - p - 3));
                writer_insert_tag(writer, inner, "preview-bold");
                g_free(inner);
                p = end + 3;
                continue;
            }
        }
        if (str_has_prefix(p, "**") || str_has_prefix(p, "__")) {
            const char *delimiter = str_has_prefix(p, "**") ? "**" : "__";
            const char *end = markdown_find_closing(p + 2, delimiter);
            if (end) {
                char *inner = g_strndup(p + 2, (gsize)(end - p - 2));
                writer_insert_tag(writer, inner, "preview-bold");
                g_free(inner);
                p = end + 2;
                continue;
            }
        }
        if (str_has_prefix(p, "~~")) {
            const char *end = markdown_find_closing(p + 2, "~~");
            if (end) {
                char *inner = g_strndup(p + 2, (gsize)(end - p - 2));
                writer_insert_tag(writer, inner, "preview-strike");
                g_free(inner);
                p = end + 2;
                continue;
            }
        }
        if (*p == '*' || *p == '_') {
            char delimiter[2] = {*p, '\0'};
            const char *end = markdown_find_closing(p + 1, delimiter);
            if (end) {
                char *inner = g_strndup(p + 1, (gsize)(end - p - 1));
                writer_insert_tag(writer, inner, "preview-italic");
                g_free(inner);
                p = end + 1;
                continue;
            }
        }
        p = markdown_render_one_char(writer, p);
    }
}

/**
 * @brief Check for a fenced code block marker.
 * @details CommonMark accepts backtick and tilde fences. Graptoς stores the
 *          fence character so only the matching fence closes the block.
 * @param line The markdown line being inspected.
 * @param marker_out Receives '`' or '~' when a fence is found.
 * @return TRUE when line begins a fence after up to three spaces.
 */
static gboolean markdown_is_fence(const char *line, char *marker_out) {
    const char *p = line ? line : "";
    guint spaces = markdown_leading_spaces(line);
    if (spaces > 3u) return FALSE;
    p = markdown_skip_spaces(p);
    if ((str_has_prefix(p, "```") || str_has_prefix(p, "~~~"))) {
        if (marker_out) *marker_out = *p;
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Check for a thematic break.
 * @details The preview renders CommonMark-style horizontal rules as a visible
 *          separator line.
 * @param line The markdown line being inspected.
 * @return TRUE when the line is a thematic break.
 */
static gboolean markdown_is_thematic_break(const char *line) {
    const char *p = markdown_skip_spaces(line);
    char marker = *p;
    if (marker != '-' && marker != '*' && marker != '_') return FALSE;
    guint count = 0u;
    for (; *p != '\0'; p++) {
        if (*p == marker) count++;
        else if (*p != ' ' && *p != '\t') return FALSE;
    }
    return count >= 3u;
}

/**
 * @brief Check for a setext heading underline.
 * @details Setext headings use the following line as structure, so the block
 *          parser needs to look ahead before rendering the paragraph line.
 * @param line The candidate underline.
 * @return 1 for h1, 2 for h2, or 0 when not a setext underline.
 */
static guint markdown_setext_level(const char *line) {
    const char *p = markdown_skip_spaces(line);
    char marker = *p;
    if (marker != '=' && marker != '-') return 0u;
    guint count = 0u;
    for (; *p != '\0'; p++) {
        if (*p == marker) count++;
        else if (*p != ' ' && *p != '\t') return 0u;
    }
    if (count == 0u) return 0u;
    return marker == '=' ? 1u : 2u;
}

/**
 * @brief Check for a GFM table separator row.
 * @details Tables are a GFM extension. The GTK preview renders them as
 *          monospaced text with separators instead of calculating cell layout.
 * @param line The markdown line being inspected.
 * @return TRUE when the line looks like a table divider.
 */
static gboolean markdown_is_table_separator(const char *line) {
    const char *p = markdown_skip_spaces(line);
    gboolean saw_dash = FALSE;
    gboolean saw_pipe = FALSE;
    for (; *p != '\0'; p++) {
        if (*p == '|') saw_pipe = TRUE;
        else if (*p == '-') saw_dash = TRUE;
        else if (*p != ':' && *p != ' ' && *p != '\t') return FALSE;
    }
    return saw_dash && saw_pipe;
}

/**
 * @brief Render one table row.
 * @details The table renderer keeps the source order and uses a visible cell
 *          separator so tables remain readable without introducing a complex
 *          layout widget per preview refresh.
 * @param writer The preview writer receiving output.
 * @param line The markdown table row.
 * @param tag The text tag to apply to the rendered row.
 */
static void render_markdown_table_row(PreviewWriter *writer,
                                      const char *line,
                                      const char *tag) {
    char **cells = g_strsplit(line ? line : "", "|", -1);
    GString *row = g_string_new(NULL);
    for (guint i = 0u; cells && cells[i]; i++) {
        g_strstrip(cells[i]);
        if (cells[i][0] == '\0' && (i == 0u || !cells[i + 1])) continue;
        if (row->len > 0u) g_string_append(row, "  │  ");
        g_string_append(row, cells[i]);
    }
    writer_insert_tag(writer, row->str, tag);
    writer_insert(writer, "\n");
    g_string_free(row, TRUE);
    g_strfreev(cells);
}

/**
 * @brief Render a markdown heading.
 * @details Heading tags are selected dynamically so h4-h6 can share the same
 *          block parser as h1-h3.
 * @param writer The preview writer receiving output.
 * @param text The heading text.
 * @param level The CommonMark heading level.
 */
static void render_markdown_heading(PreviewWriter *writer,
                                    const char *text,
                                    guint level) {
    char tag[16];
    g_snprintf(tag, sizeof(tag), "preview-h%u", CLAMP(level, 1u, 6u));
    writer_insert_tag(writer, text ? text : "", tag);
    writer_insert(writer, "\n");
}

/**
 * @brief Render a list item or task item.
 * @details List indentation is represented visually, while task markers are
 *          normalized to checkbox glyphs so `[ ]` and `[x]` stand out in the
 *          preview.
 * @param writer The preview writer receiving output.
 * @param marker The visible list marker.
 * @param item The item text after the markdown marker.
 * @param indent The display indentation width.
 */
static void render_markdown_list_item(PreviewWriter *writer,
                                      const char *marker,
                                      const char *item,
                                      guint indent) {
    for (guint i = 0u; i < indent / 2u; i++) writer_insert(writer, "  ");
    if (g_str_has_prefix(item, "[ ] ")) {
        writer_insert_tag(writer, "☐ ", "preview-task");
        render_markdown_inline(writer, item + 4);
    } else if (g_str_has_prefix(item, "[x] ") || g_str_has_prefix(item, "[X] ")) {
        writer_insert_tag(writer, "☑ ", "preview-task");
        render_markdown_inline(writer, item + 4);
    } else {
        writer_insert(writer, marker);
        writer_insert(writer, " ");
        render_markdown_inline(writer, item);
    }
    writer_insert(writer, "\n");
}

/**
 * @brief Render one markdown block line.
 * @details The block pass handles CommonMark/GFM structures that can be
 *          recognized one line at a time. It deliberately leaves raw HTML as
 *          visible text because the preview is a safe GTK text renderer.
 * @param writer The preview writer receiving output.
 * @param lines All markdown lines in the document.
 * @param index The current line index, updated when lookahead consumes lines.
 * @param in_code TRUE when inside a fenced code block.
 * @param fence_marker The active fenced-code marker character.
 */
static void render_markdown_block(PreviewWriter *writer,
                                  char **lines,
                                  guint *index,
                                  gboolean *in_code,
                                  char *fence_marker) {
    const char *line = lines[*index] ? lines[*index] : "";
    char marker = '\0';
    if (*in_code) {
        if (markdown_is_fence(line, &marker) && marker == *fence_marker) {
            *in_code = FALSE;
            *fence_marker = '\0';
            return;
        }
        writer_insert_tag(writer, line, "preview-code");
        writer_insert(writer, "\n");
        return;
    }
    if (markdown_is_fence(line, &marker)) {
        *in_code = TRUE;
        *fence_marker = marker;
        return;
    }
    if (markdown_is_blank(line)) {
        writer_insert(writer, "\n");
        return;
    }
    if (markdown_leading_spaces(line) >= 4u) {
        writer_insert_tag(writer, markdown_after_indent(line, 4u), "preview-code");
        writer_insert(writer, "\n");
        return;
    }

    const char *trimmed = markdown_skip_spaces(line);
    guint setext = lines[*index + 1u] ? markdown_setext_level(lines[*index + 1u]) : 0u;
    if (setext > 0u && !markdown_is_blank(trimmed)) {
        render_markdown_heading(writer, trimmed, setext);
        (*index)++;
        return;
    }
    if (lines[*index + 1u] && markdown_is_table_separator(lines[*index + 1u])) {
        render_markdown_table_row(writer, line, "preview-bold");
        render_markdown_table_row(writer, lines[*index + 1u], "preview-table");
        (*index)++;
        while (lines[*index + 1u] && strchr(lines[*index + 1u], '|') &&
               !markdown_is_blank(lines[*index + 1u])) {
            (*index)++;
            render_markdown_table_row(writer, lines[*index], "preview-table");
        }
        return;
    }
    if (markdown_is_thematic_break(line)) {
        writer_insert_tag(writer, "────────────────────────", "preview-rule");
        writer_insert(writer, "\n");
        return;
    }
    if (*trimmed == '#') {
        guint level = 0u;
        while (trimmed[level] == '#') level++;
        if (level >= 1u && level <= 6u &&
            (trimmed[level] == ' ' || trimmed[level] == '\t')) {
            char *text = line_without_prefix(trimmed, level);
            render_markdown_heading(writer, text, level);
            g_free(text);
            return;
        }
    }
    if (*trimmed == '>') {
        char *text = line_without_prefix(trimmed, 1u);
        writer_insert_tag(writer, "❝ ", "preview-quote");
        writer_insert_tag(writer, text, "preview-quote");
        writer_insert(writer, "\n");
        g_free(text);
        return;
    }
    if ((trimmed[0] == '-' || trimmed[0] == '*' || trimmed[0] == '+') &&
        (trimmed[1] == ' ' || trimmed[1] == '\t')) {
        render_markdown_list_item(writer, "•", trimmed + 2,
                                  markdown_leading_spaces(line));
        return;
    }
    if (g_ascii_isdigit(trimmed[0])) {
        const char *p = trimmed;
        while (g_ascii_isdigit(*p)) p++;
        if ((*p == '.' || *p == ')') && (p[1] == ' ' || p[1] == '\t')) {
            char *number = g_strndup(trimmed, (gsize)(p - trimmed + 1));
            render_markdown_list_item(writer, number, p + 2,
                                      markdown_leading_spaces(line));
            g_free(number);
            return;
        }
    }

    render_markdown_inline(writer, line);
    if (g_str_has_suffix(line, "  ") || g_str_has_suffix(line, "\\")) {
        writer_insert(writer, "\n");
    }
    writer_insert(writer, "\n");
}

/**
 * @brief Preview render markdown.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param text The text fragment supplied by the caller.
 */
void preview_render_markdown(EditorTab *tab, const char *text) {
    PreviewWriter writer;
    writer_init(&writer, tab->preview_buffer);
    gboolean in_code = FALSE;
    char fence_marker = '\0';
    char **lines = g_strsplit(text ? text : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        render_markdown_block(&writer, lines, &i, &in_code, &fence_marker);
    }
    g_strfreev(lines);
}

/**
 * @brief Latex arg.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param command The command supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *latex_arg(const char *line, const char *command) {
    const char *start = strstr(line ? line : "", command);
    if (!start) return NULL;
    start = strchr(start, '{');
    if (!start) return NULL;
    const char *end = strchr(start + 1, '}');
    if (!end || end <= start + 1) return NULL;
    return g_strndup(start + 1, (gsize)(end - start - 1));
}

/**
 * @brief Latex is math line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean latex_is_math_line(const char *line) {
    return str_has_prefix(line, "$$") || str_has_prefix(line, "\\[") ||
           str_has_prefix(line, "\\(") || strchr(line ? line : "", '$');
}

/**
 * @brief Render latex line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param writer The writer supplied by the caller.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 */
static void render_latex_line(PreviewWriter *writer, const char *line) {
    char *text = NULL;
    if ((text = latex_arg(line, "\\section"))) {
        writer_insert_tag(writer, text, "preview-h1");
    } else if ((text = latex_arg(line, "\\subsection"))) {
        writer_insert_tag(writer, text, "preview-h2");
    } else if ((text = latex_arg(line, "\\subsubsection"))) {
        writer_insert_tag(writer, text, "preview-h3");
    } else if ((text = latex_arg(line, "\\item"))) {
        writer_insert(writer, "• ");
        writer_insert(writer, text);
    } else if (str_has_prefix(line, "\\item")) {
        writer_insert(writer, "• ");
        writer_insert(writer, line + 5);
    } else if (latex_is_math_line(line)) {
        writer_insert_tag(writer, line, "preview-math");
    } else if (str_has_prefix(line, "\\")) {
        writer_insert_tag(writer, line, "preview-command");
    } else {
        writer_insert(writer, line);
    }
    writer_insert(writer, "\n");
    g_free(text);
}

/**
 * @brief Preview render latex.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param text The text fragment supplied by the caller.
 */
void preview_render_latex(EditorTab *tab, const char *text) {
    PreviewWriter writer;
    writer_init(&writer, tab->preview_buffer);
    char **lines = g_strsplit(text ? text : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        render_latex_line(&writer, lines[i]);
    }
    g_strfreev(lines);
}

/**
 * @brief Editor tab update preview.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_update_preview(EditorTab *tab) {
    if (!tab || !tab->preview_buffer || !tab->win) return;
    if (!tab->win->preview_enabled || !preview_is_supported(tab)) {
        preview_set_container_visible(tab, FALSE);
        return;
    }

    GtkAdjustment *adj = tab->preview_scrolled
        ? gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(tab->preview_scrolled))
        : NULL;
    gdouble scroll_value = adj ? gtk_adjustment_get_value(adj) : 0.0;

    char *text = buffer_text(tab);
    if (!text) return;
    if (strlen(text) > PREVIEW_MAX_BYTES) {
        gtk_text_buffer_set_text(tab->preview_buffer,
                                 "Preview disabled for files over 2 MB.",
                                 -1);
        preview_set_container_visible(tab, TRUE);
        preview_restore_scroll_later(tab->preview_scrolled, scroll_value);
        g_free(text);
        return;
    }

    tab->preview_updating = TRUE;
    gtk_text_buffer_set_text(tab->preview_buffer, "", 0);
    preview_ensure_tags(tab);
    if (preview_is_markdown(tab)) preview_render_markdown(tab, text);
    else if (editor_tab_is_latex(tab)) preview_render_latex(tab, text);
    tab->preview_updating = FALSE;

    preview_set_container_visible(tab, TRUE);
    preview_restore_scroll_later(tab->preview_scrolled, scroll_value);
    g_free(text);
}
