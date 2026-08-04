/**
 * @file src/editor_tab_sourceview.c
 * @brief Graptoς editor tab sourceview module.
 * @details GtkSourceView is the seam between our syntax/theme rules and GTK rendering. We
 *          keep that seam narrow so highlight fixes do not leak into unrelated editor
 *          behavior.
 */

#include "editor_tab_private.h"

/**
 * @brief Background custom highlight chunk size.
 */
#define CUSTOM_HIGHLIGHT_BACKGROUND_LINES 160u

/**
 * @brief Background custom highlight delay.
 */
#define CUSTOM_HIGHLIGHT_BACKGROUND_DELAY_MS 20u

/**
 * @brief Source language id for syntax.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param syntax The syntax definition used by the editor path.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *source_language_id_for_syntax(SyntaxDef *syntax) {
    if (!syntax || !syntax->name) return NULL;
    if (g_ascii_strcasecmp(syntax->name, "C") == 0) return "c";
    if (g_ascii_strcasecmp(syntax->name, "C++") == 0) return "cpp";
    if (g_ascii_strcasecmp(syntax->name, "Python") == 0) return "python3";
    if (g_ascii_strcasecmp(syntax->name, "JavaScript") == 0) return "javascript";
    if (g_ascii_strcasecmp(syntax->name, "TypeScript") == 0) return "typescript";
    if (g_ascii_strcasecmp(syntax->name, "Rust") == 0) return "rust";
    if (g_ascii_strcasecmp(syntax->name, "Go") == 0) return "go";
    if (g_ascii_strcasecmp(syntax->name, "Java") == 0) return "java";
    if (g_ascii_strcasecmp(syntax->name, "PHP") == 0) return "php";
    if (g_ascii_strcasecmp(syntax->name, "Shell") == 0) return "sh";
    if (g_ascii_strcasecmp(syntax->name, "Bash") == 0) return "sh";
    if (g_ascii_strcasecmp(syntax->name, "Markdown") == 0) return "markdown";
    if (g_ascii_strcasecmp(syntax->name, "LaTeX") == 0) return "latex";
    if (g_ascii_strcasecmp(syntax->name, "HTML") == 0) return "html";
    if (g_ascii_strcasecmp(syntax->name, "CSS") == 0) return "css";
    if (g_ascii_strcasecmp(syntax->name, "JSON") == 0) return "json";
    if (g_ascii_strcasecmp(syntax->name, "YAML") == 0) return "yaml";
    if (g_ascii_strcasecmp(syntax->name, "TOML") == 0) return "toml";
    if (g_ascii_strcasecmp(syntax->name, "SQL") == 0) return "sql";
    if (g_ascii_strcasecmp(syntax->name, "Diff") == 0) return "diff";
    return NULL;
}

/**
 * @brief Source language for tab.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkSourceLanguage *source_language_for_tab(EditorTab *tab) {
    GtkSourceLanguageManager *manager = gtk_source_language_manager_get_default();
    if (!manager || !tab) return NULL;

    GtkSourceLanguage *language = NULL;
    const char *id = source_language_id_for_syntax(tab->active_syntax);
    if (tab->active_syntax) {
        return id ? gtk_source_language_manager_get_language(manager, id) : NULL;
    }
    if (tab->file_path && tab->file_path[0] != '\0') {
        language = gtk_source_language_manager_guess_language(manager,
                                                              tab->file_path,
                                                              NULL);
    }
    if (!language) {
        if (id) language = gtk_source_language_manager_get_language(manager, id);
    }
    return language;
}

/**
 * @brief Return the effective custom highlight mode.
 * @param tab The editor tab whose window config is being inspected.
 * @return Stable mode text.
 */
static const char *custom_highlight_mode(EditorTab *tab) {
    const char *mode = tab && tab->win ? tab->win->custom_highlight_mode : NULL;
    if (g_strcmp0(mode, "viewport") == 0 ||
        g_strcmp0(mode, "background") == 0 ||
        g_strcmp0(mode, "full") == 0) {
        return mode;
    }
    return "auto";
}

/**
 * @brief Return whether the current custom highlighter should use a viewport.
 * @param tab The editor tab whose buffer is being inspected.
 * @return TRUE when full-buffer regex highlighting should be avoided.
 */
static gboolean custom_highlight_uses_viewport(EditorTab *tab) {
    const char *mode = custom_highlight_mode(tab);
    if (g_strcmp0(mode, "viewport") == 0 ||
        g_strcmp0(mode, "background") == 0) {
        return TRUE;
    }
    if (g_strcmp0(mode, "full") == 0) return FALSE;
    guint max_chars = tab && tab->win && tab->win->full_highlight_max_chars > 0u
        ? tab->win->full_highlight_max_chars
        : GRAPTOS_FULL_HIGHLIGHT_MAX_CHARS;
    return tab && tab->buffer &&
           (guint)gtk_text_buffer_get_char_count(tab->buffer) > max_chars;
}

/**
 * @brief Return whether background custom highlighting is enabled.
 * @param tab The editor tab whose window config is being inspected.
 * @return TRUE when offscreen ranges may be highlighted slowly.
 */
static gboolean custom_highlight_uses_background(EditorTab *tab) {
    return g_strcmp0(custom_highlight_mode(tab), "background") == 0;
}

/**
 * @brief Return whether full custom tag clearing should be avoided.
 * @details Only custom YAML syntaxes need the large-file clear avoidance. When
 *          a tab switches to a GtkSourceView language, old Graptoς regex tags
 *          must still be cleared so stale custom colours do not remain.
 * @param tab The editor tab whose syntax target is being inspected.
 * @return TRUE when clearing should stay range-bound.
 */
static gboolean custom_highlight_avoid_full_clear(EditorTab *tab) {
    return tab &&
           editor_tab_highlighting_allowed(tab) &&
           source_language_for_tab(tab) == NULL &&
           custom_highlight_uses_viewport(tab);
}

/**
 * @brief Compute visible custom highlight line range.
 * @param tab The editor tab whose text view is visible.
 * @param start_out First line to highlight.
 * @param end_out Last line to highlight.
 * @return TRUE when a range was computed.
 */
static gboolean custom_highlight_visible_range(EditorTab *tab,
                                               guint *start_out,
                                               guint *end_out) {
    if (!tab || !tab->buffer || !start_out || !end_out) return FALSE;
    gint line_count = gtk_text_buffer_get_line_count(tab->buffer);
    if (line_count <= 0) return FALSE;

    guint top_line = 0u;
    guint bottom_line = (guint)MIN(line_count - 1, 0);
    if (tab->text_view && gtk_widget_get_mapped(tab->text_view)) {
        GtkTextView *view = GTK_TEXT_VIEW(tab->text_view);
        GdkRectangle visible;
        GtkTextIter top;
        GtkTextIter bottom;
        gint ignored = 0;
        gtk_text_view_get_visible_rect(view, &visible);
        gtk_text_view_get_line_at_y(view, &top, visible.y, &ignored);
        gtk_text_view_get_line_at_y(view, &bottom, visible.y + visible.height, &ignored);
        top_line = (guint)gtk_text_iter_get_line(&top);
        bottom_line = (guint)gtk_text_iter_get_line(&bottom);
    } else {
        GtkTextIter cursor;
        GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, insert);
        top_line = (guint)gtk_text_iter_get_line(&cursor);
        bottom_line = top_line;
    }

    guint context = GRAPTOS_HIGHLIGHT_CONTEXT_LINES;
    guint start = top_line > context ? top_line - context : 0u;
    guint end = bottom_line + context;
    if (end >= (guint)line_count) end = (guint)line_count - 1u;
    if (end < start) end = start;
    *start_out = start;
    *end_out = end;
    return TRUE;
}

/**
 * @brief Clear old viewport-only custom highlight ranges.
 * @param tab The editor tab whose previous range should be trimmed.
 * @param start New highlighted start line.
 * @param end New highlighted end line.
 */
static void custom_highlight_clear_old_viewport(EditorTab *tab,
                                                guint start,
                                                guint end) {
    if (!tab || !tab->custom_highlight_range_valid || !tab->win) return;
    if (tab->custom_highlight_end_line < start) {
        syntax_clear_range(tab->buffer,
                           tab->win->syntaxes,
                           tab->custom_highlight_start_line,
                           tab->custom_highlight_end_line);
        return;
    }
    if (tab->custom_highlight_start_line > end) {
        syntax_clear_range(tab->buffer,
                           tab->win->syntaxes,
                           tab->custom_highlight_start_line,
                           tab->custom_highlight_end_line);
        return;
    }
    if (tab->custom_highlight_start_line < start) {
        syntax_clear_range(tab->buffer,
                           tab->win->syntaxes,
                           tab->custom_highlight_start_line,
                           start - 1u);
    }
    if (tab->custom_highlight_end_line > end) {
        syntax_clear_range(tab->buffer,
                           tab->win->syntaxes,
                           end + 1u,
                           tab->custom_highlight_end_line);
    }
}

static gboolean custom_highlight_background_cb(gpointer user_data);

/**
 * @brief Schedule one background custom highlight chunk.
 * @param tab The editor tab whose offscreen lines should be highlighted.
 */
static void custom_highlight_schedule_background(EditorTab *tab) {
    if (!tab || !custom_highlight_uses_background(tab)) return;
    if (tab->custom_highlight_background_timeout) return;
    tab->custom_highlight_background_timeout =
        g_timeout_add_full(G_PRIORITY_LOW,
                           CUSTOM_HIGHLIGHT_BACKGROUND_DELAY_MS,
                           custom_highlight_background_cb,
                           tab,
                           NULL);
}

/**
 * @brief Apply one background custom highlight chunk.
 * @param user_data Editor tab.
 * @return G_SOURCE_REMOVE after one chunk.
 */
static gboolean custom_highlight_background_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab || !tab->buffer || !tab->win || !editor_tab_custom_highlight_needed(tab)) {
        return G_SOURCE_REMOVE;
    }
    tab->custom_highlight_background_timeout = 0u;
    gint line_count = gtk_text_buffer_get_line_count(tab->buffer);
    if (line_count <= 0 || tab->custom_highlight_background_line >= (guint)line_count) {
        return G_SOURCE_REMOVE;
    }
    guint start = tab->custom_highlight_background_line;
    guint end = start + CUSTOM_HIGHLIGHT_BACKGROUND_LINES - 1u;
    if (end >= (guint)line_count) end = (guint)line_count - 1u;
    syntax_apply_range(tab->buffer,
                       tab->win->syntaxes,
                       tab->active_syntax,
                       start,
                       end);
    tab->custom_highlight_background_line = end + 1u;
    if (tab->custom_highlight_background_line < (guint)line_count) {
        custom_highlight_schedule_background(tab);
    }
    return G_SOURCE_REMOVE;
}

/**
 * @brief Colour is dark.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param colour The colour supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean colour_is_dark(const char *colour) {
    GdkRGBA rgba;
    if (!colour || !gdk_rgba_parse(&rgba, colour)) return TRUE;
    double luminance = (0.2126 * rgba.red) +
                       (0.7152 * rgba.green) +
                       (0.0722 * rgba.blue);
    return luminance < 0.50;
}

/**
 * @brief Source style scheme for window.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkSourceStyleScheme *source_style_scheme_for_window(EditorWindow *win) {
    GtkSourceStyleSchemeManager *manager =
        gtk_source_style_scheme_manager_get_default();
    if (!manager) return NULL;

    gboolean dark = colour_is_dark(win ? win->editor_bg_color : NULL);
    const char *preferred[] = {
        dark ? "Adwaita-dark" : "Adwaita",
        dark ? "classic" : "classic",
        NULL
    };

    for (guint i = 0u; preferred[i]; i++) {
        GtkSourceStyleScheme *scheme =
            gtk_source_style_scheme_manager_get_scheme(manager, preferred[i]);
        if (scheme) return scheme;
    }
    return NULL;
}


/**
 * @brief Source parent scheme id for window.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *source_parent_scheme_id_for_window(EditorWindow *win) {
    GtkSourceStyleSchemeManager *manager =
        gtk_source_style_scheme_manager_get_default();
    gboolean dark = colour_is_dark(win ? win->editor_bg_color : NULL);
    const char *preferred[] = {
        dark ? "Adwaita-dark" : "Adwaita",
        dark ? "oblivion" : "classic",
        "classic",
        NULL
    };
    if (!manager) return preferred[0];
    for (guint i = 0u; preferred[i]; i++) {
        if (gtk_source_style_scheme_manager_get_scheme(manager, preferred[i])) {
            return preferred[i];
        }
    }
    return preferred[0];
}

/**
 * @brief String contains ci.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @param needle The needle supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean string_contains_ci(const char *text, const char *needle) {
    if (!text || !needle || needle[0] == '\0') return FALSE;
    char *lower_text = g_ascii_strdown(text, -1);
    char *lower_needle = g_ascii_strdown(needle, -1);
    gboolean found = lower_text && lower_needle && strstr(lower_text, lower_needle) != NULL;
    g_free(lower_text);
    g_free(lower_needle);
    return found;
}
/**
 * @brief Gtksource styles for yaml rule.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param rule_name The rule name supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */

/**
 * @brief Style names add.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param names The names supplied by the caller.
 * @param name The name supplied by the caller.
 */
static void style_names_add(GPtrArray *names, const char *name) {
    if (!names || !name || name[0] == '\0') return;
    for (guint i = 0u; i < names->len; i++) {
        const char *existing = g_ptr_array_index(names, i);
        if (g_strcmp0(existing, name) == 0) return;
    }
    g_ptr_array_add(names, g_strdup(name));
}

/**
 * @brief Map a Graptoς YAML rule name to GtkSourceView style IDs.
 * @details YAML rule names are Graptoς concepts, while GtkSourceView exposes
 *          stable style IDs such as def:keyword or markdown:header. This
 *          mapping lets existing YAML files keep their semantic names without
 *          reintroducing a second regex-based highlighter.
 * @param rule_name The syntax rule name from a Graptoς YAML file.
 * @return A newly allocated string array of GtkSourceView style IDs.
 *
 * YAML rule names are Graptoς concepts, while GtkSourceView exposes stable style
 * IDs such as def:keyword or markdown:header.  This mapping lets existing YAML
 * files keep their semantic names without reintroducing a second regex-based
 * highlighter.
 */
static GPtrArray *gtksource_styles_for_yaml_rule(const char *rule_name) {
    GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
    if (!names || !rule_name) return names;

    if (string_contains_ci(rule_name, "heading") ||
        string_contains_ci(rule_name, "header")) {
        style_names_add(names, "def:heading");
        style_names_add(names, "markdown:header");
        style_names_add(names, "markdown:atx-header");
        style_names_add(names, "markdown:setext-header");
    }
    if (string_contains_ci(rule_name, "codeblock") ||
        string_contains_ci(rule_name, "code-block") ||
        string_contains_ci(rule_name, "fence")) {
        style_names_add(names, "def:preformatted-section");
        style_names_add(names, "markdown:code-block");
    }
    if (string_contains_ci(rule_name, "inline-code") ||
        string_contains_ci(rule_name, "code-span")) {
        style_names_add(names, "def:inline-code");
        style_names_add(names, "markdown:code-span");
    }
    if (string_contains_ci(rule_name, "link") ||
        string_contains_ci(rule_name, "url")) {
        style_names_add(names, "def:link-text");
        style_names_add(names, "def:link-destination");
        style_names_add(names, "markdown:link-text");
        style_names_add(names, "markdown:url");
    }
    if (string_contains_ci(rule_name, "bold") ||
        string_contains_ci(rule_name, "strong")) {
        style_names_add(names, "def:strong-emphasis");
        style_names_add(names, "markdown:strong-emphasis");
    }
    if (string_contains_ci(rule_name, "emphasis") ||
        string_contains_ci(rule_name, "italic")) {
        style_names_add(names, "def:emphasis");
        style_names_add(names, "markdown:emphasis");
    }
    if (string_contains_ci(rule_name, "list")) {
        style_names_add(names, "def:list-marker");
        style_names_add(names, "markdown:list-marker");
    }
    if (string_contains_ci(rule_name, "quote")) {
        style_names_add(names, "def:shebang");
        style_names_add(names, "markdown:blockquote-marker");
    }
    if (string_contains_ci(rule_name, "comment") ||
        string_contains_ci(rule_name, "doc")) {
        style_names_add(names, "def:comment");
    }
    if (string_contains_ci(rule_name, "string") ||
        string_contains_ci(rule_name, "include-path")) {
        style_names_add(names, "def:string");
    }
    if (string_contains_ci(rule_name, "character")) {
        style_names_add(names, "def:character");
    }
    if (string_contains_ci(rule_name, "escape")) {
        style_names_add(names, "def:special-char");
    }
    if (string_contains_ci(rule_name, "preprocessor") ||
        string_contains_ci(rule_name, "macro")) {
        style_names_add(names, "def:preprocessor");
    }
    if (string_contains_ci(rule_name, "keyword") ||
        string_contains_ci(rule_name, "control") ||
        string_contains_ci(rule_name, "storage") ||
        string_contains_ci(rule_name, "attribute")) {
        style_names_add(names, "def:keyword");
    }
    if (string_contains_ci(rule_name, "type") ||
        string_contains_ci(rule_name, "aggregate")) {
        style_names_add(names, "def:type");
    }
    if (string_contains_ci(rule_name, "function")) {
        style_names_add(names, "def:function");
    }
    if (string_contains_ci(rule_name, "constant") ||
        string_contains_ci(rule_name, "boolean") ||
        string_contains_ci(rule_name, "null")) {
        style_names_add(names, "def:constant");
    }
    if (string_contains_ci(rule_name, "number")) {
        style_names_add(names, "def:number");
    }
    if (string_contains_ci(rule_name, "operator") ||
        string_contains_ci(rule_name, "punctuation") ||
        string_contains_ci(rule_name, "bracket")) {
        style_names_add(names, "def:operator");
    }
    if (string_contains_ci(rule_name, "todo") ||
        string_contains_ci(rule_name, "note")) {
        style_names_add(names, "def:note");
    }
    if (string_contains_ci(rule_name, "error")) {
        style_names_add(names, "def:error");
    }
    return names;
}

/**
 * @brief Append xml escaped.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param out Output storage filled when the lookup succeeds.
 * @param text The text fragment supplied by the caller.
 */
static void append_xml_escaped(GString *out, const char *text) {
    char *escaped = g_markup_escape_text(text ? text : "", -1);
/**
 * @brief Yaml override style dir.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
    g_string_append(out, escaped ? escaped : "");
    g_free(escaped);
}

/**
 * @brief Append style from rule.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param xml The xml supplied by the caller.
 * @param style_name The style name supplied by the caller.
 * @param rule The rule supplied by the caller.
 */
static void append_style_from_rule(GString *xml, const char *style_name, SyntaxRule *rule) {
    if (!xml || !style_name || !rule || !rule->color || rule->color[0] == '\0') return;
    g_string_append(xml, "  <style name=\"");
    append_xml_escaped(xml, style_name);
    g_string_append(xml, "\" foreground=\"");
    append_xml_escaped(xml, rule->color);
    g_string_append(xml, "\"");
    if (rule->bold) g_string_append(xml, " bold=\"true\"");
    if (rule->italic) g_string_append(xml, " italic=\"true\"");
    if (rule->underline) g_string_append(xml, " underline=\"single\"");
    g_string_append(xml, "/>\n");
}

/**
 * @brief Return the cache directory for generated YAML style overrides.
 * @details GtkSourceView loads style schemes from files, not arbitrary in-memory
 *          CSS. Graptoς writes override schemes under the user cache directory
 *          so config and YAML colors can be represented without modifying
 *          system language data.
 * @return An owned directory path, or NULL when no cache base is available.
 *
 * GtkSourceView loads style schemes from files, not arbitrary in-memory CSS.
 * Generate Graptoς override schemes under the user cache directory so config and
 * YAML colors can be represented without modifying system language data.
 */
static char *yaml_override_style_dir(void) {
    const char *base = g_get_user_cache_dir();
    if (!base || base[0] == '\0') return NULL;
    return g_build_filename(base, "graptos", "gtksourceview", "styles", NULL);
}

/**
 * @brief Style slug from syntax name.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param name The name supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *style_slug_from_syntax_name(const char *name) {
    char *lower = g_ascii_strdown(name && name[0] ? name : "plain", -1);
    if (!lower) return g_strdup("plain");
    for (char *p = lower; *p; p++) {
        if (!g_ascii_isalnum(*p)) *p = '-';
    }
    return lower;
}

/**
 * @brief Effective colour.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param value The value being parsed, stored, or applied.
 * @param fallback The fallback supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *effective_colour(const char *value, const char *fallback) {
    return (value && value[0] == '#') ? value : fallback;
}

/**
 * @brief Source yaml override scheme for tab.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkSourceStyleScheme *source_yaml_override_scheme_for_tab(EditorTab *tab) {
    if (!tab || !tab->win) return NULL;

    g_autofree char *dir = yaml_override_style_dir();
    if (!dir) return source_style_scheme_for_window(tab->win);
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        return source_style_scheme_for_window(tab->win);
    }

    gboolean dark = colour_is_dark(tab->win->editor_bg_color);
    const char *syntax_name = (tab->active_syntax && tab->active_syntax->name) ?
        tab->active_syntax->name : "plain";
    g_autofree char *slug = style_slug_from_syntax_name(syntax_name);
    g_autofree char *id = g_strdup_printf("graptos-config-%s-%s-%s",
                                          dark ? "dark" : "light",
                                          tab->win->use_yaml_style_overrides ? "yaml" : "plain",
                                          slug ? slug : "plain");
    g_autofree char *filename = g_strdup_printf("%s.xml", id);
    g_autofree char *path = g_build_filename(dir, filename, NULL);

    const char *fg = effective_colour(tab->win->editor_fg_color,
                                      dark ? "#d4d4d4" : "#202124");
    const char *bg = effective_colour(tab->win->editor_bg_color,
                                      dark ? "#181a1f" : "#ffffff");
    const char *gutter_bg = effective_colour(tab->win->editor_gutter_bg_color, bg);
    const char *gutter_fg = effective_colour(tab->win->editor_gutter_fg_color,
                                             dark ? "#8b949e" : "#6b7280");
    const char *current_line = effective_colour(tab->win->editor_current_line_bg_color,
                                                dark ? "#20232b" : "#f3f4f6");
    const char *selection_bg = effective_colour(tab->win->editor_selection_bg_color,
                                                dark ? "#3a405c" : "#cfe3ff");
    const char *selection_fg = effective_colour(tab->win->editor_selection_fg_color,
                                                dark ? "#ffffff" : "#111827");
    const char *cursor = effective_colour(tab->win->editor_cursor_color, fg);

    g_autoptr(GString) xml = g_string_new(NULL);
    g_string_append(xml, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    const char *parent_id = source_parent_scheme_id_for_window(tab->win);
    g_string_append_printf(xml,
        "<style-scheme id=\"%s\" _name=\"Graptoς Config\" version=\"1.0\" parent-scheme=\"%s\">\n",
        id, parent_id ? parent_id : "classic");
    g_string_append(xml, "  <author>Graptoς</author>\n");
    g_string_append(xml, "  <description>GtkSourceView highlighting using Graptoς config colours and optional YAML overrides.</description>\n");
    g_string_append_printf(xml,
        "  <style name=\"text\" foreground=\"%s\" background=\"%s\"/>\n",
        fg, bg);
    g_string_append_printf(xml,
        "  <style name=\"current-line\" background=\"%s\"/>\n", current_line);
    g_string_append_printf(xml,
        "  <style name=\"line-numbers\" foreground=\"%s\" background=\"%s\"/>\n",
        gutter_fg, gutter_bg);
    g_string_append_printf(xml,
        "  <style name=\"selection\" foreground=\"%s\" background=\"%s\"/>\n",
        selection_fg, selection_bg);
    g_string_append_printf(xml,
        "  <style name=\"cursor\" foreground=\"%s\"/>\n", cursor);
    g_string_append_printf(xml,
        "  <style name=\"bracket-match\" foreground=\"%s\" background=\"%s\" bold=\"true\"/>\n",
        fg, current_line);
    g_string_append_printf(xml,
        "  <style name=\"right-margin\" foreground=\"%s\" background=\"%s\"/>\n",
        gutter_fg, bg);

    if (tab->win->use_yaml_style_overrides && tab->active_syntax &&
        tab->active_syntax->rules) {
        g_autoptr(GHashTable) seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
        for (guint i = 0u; i < tab->active_syntax->rules->len; i++) {
            SyntaxRule *rule = g_ptr_array_index(tab->active_syntax->rules, i);
            if (!rule || !rule->color || rule->color[0] == '\0') continue;
            g_autoptr(GPtrArray) style_names = gtksource_styles_for_yaml_rule(rule->name);
            for (guint j = 0u; style_names && j < style_names->len; j++) {
                const char *style_name = g_ptr_array_index(style_names, j);
                if (!style_name || g_hash_table_contains(seen, style_name)) continue;
                g_hash_table_add(seen, g_strdup(style_name));
                append_style_from_rule(xml, style_name, rule);
            }
        }
    }
    g_string_append(xml, "</style-scheme>\n");

    g_autoptr(GError) error = NULL;
    if (!g_file_set_contents(path, xml->str, (gssize)xml->len, &error)) {
        return source_style_scheme_for_window(tab->win);
    }

    GtkSourceStyleSchemeManager *manager = gtk_source_style_scheme_manager_get_default();
/**
 * @brief Tab update highlight engine.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
    GtkSourceStyleScheme *scheme = NULL;
    if (manager) {
        static gboolean search_path_added = FALSE;
        if (!search_path_added) {
            gtk_source_style_scheme_manager_append_search_path(manager, dir);
            search_path_added = TRUE;
        }
        /*
         * The scheme file is regenerated when config or YAML overrides change.
         * Force a rescan so GtkSourceView does not keep using a cached copy from
         * before the user changed colors.
         */
        gtk_source_style_scheme_manager_force_rescan(manager);
        scheme = gtk_source_style_scheme_manager_get_scheme(manager, id);
    }
    return scheme ? scheme : source_style_scheme_for_window(tab->win);
}

/**
 * @brief Clear graptos transient tags.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void clear_graptos_transient_tags(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    if (!custom_highlight_avoid_full_clear(tab)) {
        syntax_clear(tab->buffer, tab->win ? tab->win->syntaxes : NULL);
    }
    if (tab->selection_matches_active) clear_selection_matches(tab);
    if (tab->diagnostics_active &&
        (!tab->active_syntax ||
         !tab->active_syntax->lsp_command ||
         tab->active_syntax->lsp_command[0] == '\0')) {
        clear_syntax_diagnostics(tab);
    }
    tab->custom_highlight_active = FALSE;
    tab->custom_highlight_range_valid = FALSE;
    graptos_source_cancel(&tab->custom_highlight_background_timeout);
}

/**
 * @brief Return whether custom YAML highlighting owns this tab.
 * @details GtkSourceView-backed syntaxes are excluded because GTK already owns
 *          their incremental highlighter. This function only covers Graptoς
 *          regex syntax fallback.
 * @param tab The editor tab whose highlighting engine is being inspected.
 * @return TRUE when Graptoς regex highlighting should run.
 */
gboolean editor_tab_custom_highlight_needed(EditorTab *tab) {
    return tab &&
           tab->source_buffer &&
           editor_tab_highlighting_allowed(tab) &&
           gtk_source_buffer_get_language(tab->source_buffer) == NULL;
}

/**
 * @brief Apply custom YAML highlighting without resetting the source engine.
 * @details Large custom files highlight only the visible range plus context.
 *          Small files keep the full-buffer behavior unless config selects a
 *          viewport mode.
 * @param tab The editor tab whose visible range should be highlighted.
 */
void editor_tab_apply_custom_highlight(EditorTab *tab) {
    if (!tab || !tab->buffer || !tab->win || !editor_tab_custom_highlight_needed(tab)) {
        return;
    }

    guint max_chars = tab->win->full_highlight_max_chars > 0u
        ? tab->win->full_highlight_max_chars
        : GRAPTOS_FULL_HIGHLIGHT_MAX_CHARS;
    if (!custom_highlight_uses_viewport(tab)) {
        if ((guint)gtk_text_buffer_get_char_count(tab->buffer) <= max_chars) {
            syntax_apply(tab->buffer,
                         tab->win->syntaxes,
                         tab->active_syntax);
            tab->custom_highlight_active = TRUE;
        }
        return;
    }

    guint start = 0u;
    guint end = 0u;
    if (!custom_highlight_visible_range(tab, &start, &end)) return;
    if (!custom_highlight_uses_background(tab)) {
        custom_highlight_clear_old_viewport(tab, start, end);
    }
    syntax_apply_range(tab->buffer, tab->win->syntaxes, tab->active_syntax, start, end);
    tab->custom_highlight_start_line = start;
    tab->custom_highlight_end_line = end;
    tab->custom_highlight_range_valid = TRUE;
    tab->custom_highlight_active = TRUE;

    if (custom_highlight_uses_background(tab)) {
        gint line_count = gtk_text_buffer_get_line_count(tab->buffer);
        if (line_count > 0) {
            if (tab->custom_highlight_background_line < end + 1u) {
                tab->custom_highlight_background_line = end + 1u;
            }
            if (tab->custom_highlight_background_line < (guint)line_count) {
                custom_highlight_schedule_background(tab);
            }
        }
    } else {
        graptos_source_cancel(&tab->custom_highlight_background_timeout);
    }
}

/**
 * @brief Refresh the GtkSourceView language and style engine for a tab.
 * @details Resetting language and scheme as one operation avoids mixed style
 *          state where old text keeps one scheme while newly typed text uses
 *          another. The transient Graptoς tags are cleared before the new
 *          engine is applied so the buffer redraw starts from a clean base.
 * @param tab The editor tab whose source buffer should be refreshed.
 *
 * Reset language and scheme as one operation.  GtkSourceView otherwise keeps
 * parts of the old style state until later invalidation, which made old text
 * use one scheme while newly typed text used another.
 */
void editor_tab_update_highlight_engine(EditorTab *tab) {
    if (!tab || !tab->source_buffer) return;

    graptos_source_cancel(&tab->highlight_timeout);
    clear_graptos_transient_tags(tab);

    gtk_source_buffer_set_highlight_syntax(tab->source_buffer, FALSE);
    gtk_source_buffer_set_language(tab->source_buffer, NULL);
    gtk_source_buffer_set_style_scheme(tab->source_buffer,
        source_yaml_override_scheme_for_tab(tab));
    gtk_source_buffer_set_language(tab->source_buffer, source_language_for_tab(tab));
    gtk_source_buffer_set_highlight_syntax(tab->source_buffer, TRUE);
    if (editor_tab_custom_highlight_needed(tab)) {
        tab->custom_highlight_background_line = 0u;
        editor_tab_apply_custom_highlight(tab);
    }

    if (tab->text_view) gtk_widget_queue_draw(tab->text_view);
    if (tab->minimap_view) gtk_widget_queue_draw(tab->minimap_view);
}
