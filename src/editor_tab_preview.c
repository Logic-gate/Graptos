#include "editor_tab_private.h"

#include <stdarg.h>
#include <string.h>

#define PREVIEW_MAX_BYTES (2u * 1024u * 1024u)

typedef struct {
    GtkTextBuffer *buffer;
    GtkTextIter iter;
} PreviewWriter;

static gboolean str_has_prefix(const char *text, const char *prefix) {
    return text && prefix && g_str_has_prefix(text, prefix);
}

static gboolean syntax_name_is(EditorTab *tab, const char *name) {
    const char *syntax = NULL;
    if (tab && tab->active_syntax) syntax = tab->active_syntax->name;
    return syntax && name && g_ascii_strcasecmp(syntax, name) == 0;
}

static gboolean path_has_suffix(EditorTab *tab, const char *suffix) {
    return tab && tab->file_path && suffix && g_str_has_suffix(tab->file_path, suffix);
}

static gboolean preview_is_markdown(EditorTab *tab) {
    return syntax_name_is(tab, "Markdown") || path_has_suffix(tab, ".md") ||
           path_has_suffix(tab, ".markdown");
}

gboolean editor_tab_is_latex(EditorTab *tab) {
    return syntax_name_is(tab, "LaTeX") || syntax_name_is(tab, "Tex") ||
           path_has_suffix(tab, ".tex") || path_has_suffix(tab, ".latex");
}

gboolean preview_is_supported(EditorTab *tab) {
    return preview_is_markdown(tab) || editor_tab_is_latex(tab);
}

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

void preview_ensure_tags(EditorTab *tab) {
    if (!tab || !tab->preview_buffer) return;
    GtkTextBuffer *buffer = tab->preview_buffer;
    ensure_tag(buffer, "preview-h1", "weight", PANGO_WEIGHT_BOLD,
               "scale", 1.65, "pixels-above-lines", 12, NULL);
    ensure_tag(buffer, "preview-h2", "weight", PANGO_WEIGHT_BOLD,
               "scale", 1.35, "pixels-above-lines", 10, NULL);
    ensure_tag(buffer, "preview-h3", "weight", PANGO_WEIGHT_BOLD,
               "scale", 1.15, "pixels-above-lines", 8, NULL);
    ensure_tag(buffer, "preview-bold", "weight", PANGO_WEIGHT_BOLD, NULL);
    ensure_tag(buffer, "preview-italic", "style", PANGO_STYLE_ITALIC, NULL);
    ensure_tag(buffer, "preview-code", "family", "monospace",
               "background", "#2A2E3D", NULL);
    ensure_tag(buffer, "preview-quote", "style", PANGO_STYLE_ITALIC,
               "foreground", "#8FA1B3", NULL);
    ensure_tag(buffer, "preview-math", "family", "monospace",
               "foreground", "#C586C0", NULL);
    ensure_tag(buffer, "preview-command", "weight", PANGO_WEIGHT_BOLD,
               "foreground", "#4EC9B0", NULL);
}

static void writer_init(PreviewWriter *writer, GtkTextBuffer *buffer) {
    writer->buffer = buffer;
    gtk_text_buffer_get_end_iter(buffer, &writer->iter);
}

static void writer_insert(PreviewWriter *writer, const char *text) {
    gtk_text_buffer_insert(writer->buffer, &writer->iter, text ? text : "", -1);
}

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

static void render_markdown_inline(PreviewWriter *writer, const char *line) {
    const char *p = line ? line : "";
    while (*p != '\0') {
        if (str_has_prefix(p, "**")) {
            const char *end = strstr(p + 2, "**");
            if (end) {
                char *inner = g_strndup(p + 2, (gsize)(end - p - 2));
                writer_insert_tag(writer, inner, "preview-bold");
                g_free(inner);
                p = end + 2;
                continue;
            }
        }
        if (*p == '`') {
            const char *end = strchr(p + 1, '`');
            if (end) {
                char *inner = g_strndup(p + 1, (gsize)(end - p - 1));
                writer_insert_tag(writer, inner, "preview-code");
                g_free(inner);
                p = end + 1;
                continue;
            }
        }
        if (*p == '*') {
            const char *end = strchr(p + 1, '*');
            if (end) {
                char *inner = g_strndup(p + 1, (gsize)(end - p - 1));
                writer_insert_tag(writer, inner, "preview-italic");
                g_free(inner);
                p = end + 1;
                continue;
            }
        }
        const char *next = g_utf8_next_char(p);
        char *one = g_strndup(p, (gsize)(next - p));
        writer_insert(writer, one);
        g_free(one);
        p = next;
    }
}

static void render_markdown_line(PreviewWriter *writer,
                                 const char *line,
                                 gboolean *in_code) {
    if (str_has_prefix(line, "```") || str_has_prefix(line, "~~~")) {
        *in_code = !*in_code;
        return;
    }
    if (*in_code) {
        writer_insert_tag(writer, line, "preview-code");
        writer_insert(writer, "\n");
        return;
    }
    if (str_has_prefix(line, "### ")) {
        char *text = line_without_prefix(line, 3u);
        writer_insert_tag(writer, text, "preview-h3");
        writer_insert(writer, "\n");
        g_free(text);
    } else if (str_has_prefix(line, "## ")) {
        char *text = line_without_prefix(line, 2u);
        writer_insert_tag(writer, text, "preview-h2");
        writer_insert(writer, "\n");
        g_free(text);
    } else if (str_has_prefix(line, "# ")) {
        char *text = line_without_prefix(line, 1u);
        writer_insert_tag(writer, text, "preview-h1");
        writer_insert(writer, "\n");
        g_free(text);
    } else if (str_has_prefix(line, ">")) {
        char *text = line_without_prefix(line, 1u);
        writer_insert_tag(writer, text, "preview-quote");
        writer_insert(writer, "\n");
        g_free(text);
    } else if (str_has_prefix(line, "- ") || str_has_prefix(line, "* ")) {
        writer_insert(writer, "• ");
        render_markdown_inline(writer, line + 2);
        writer_insert(writer, "\n");
    } else {
        render_markdown_inline(writer, line);
        writer_insert(writer, "\n");
    }
}

void preview_render_markdown(EditorTab *tab, const char *text) {
    PreviewWriter writer;
    writer_init(&writer, tab->preview_buffer);
    gboolean in_code = FALSE;
    char **lines = g_strsplit(text ? text : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        render_markdown_line(&writer, lines[i], &in_code);
    }
    g_strfreev(lines);
}

static char *latex_arg(const char *line, const char *command) {
    const char *start = strstr(line ? line : "", command);
    if (!start) return NULL;
    start = strchr(start, '{');
    if (!start) return NULL;
    const char *end = strchr(start + 1, '}');
    if (!end || end <= start + 1) return NULL;
    return g_strndup(start + 1, (gsize)(end - start - 1));
}

static gboolean latex_is_math_line(const char *line) {
    return str_has_prefix(line, "$$") || str_has_prefix(line, "\\[") ||
           str_has_prefix(line, "\\(") || strchr(line ? line : "", '$');
}

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

void preview_render_latex(EditorTab *tab, const char *text) {
    PreviewWriter writer;
    writer_init(&writer, tab->preview_buffer);
    char **lines = g_strsplit(text ? text : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        render_latex_line(&writer, lines[i]);
    }
    g_strfreev(lines);
}

void editor_tab_update_preview(EditorTab *tab) {
    if (!tab || !tab->preview_buffer || !tab->win) return;
    if (!tab->win->preview_enabled || !preview_is_supported(tab)) {
        if (tab->preview_scrolled) gtk_widget_set_visible(tab->preview_scrolled, FALSE);
        return;
    }

    char *text = buffer_text(tab);
    if (!text) return;
    if (strlen(text) > PREVIEW_MAX_BYTES) {
        gtk_text_buffer_set_text(tab->preview_buffer,
                                 "Preview disabled for files over 2 MB.",
                                 -1);
        g_free(text);
        return;
    }

    tab->preview_updating = TRUE;
    gtk_text_buffer_set_text(tab->preview_buffer, "", 0);
    preview_ensure_tags(tab);
    if (preview_is_markdown(tab)) preview_render_markdown(tab, text);
    else if (editor_tab_is_latex(tab)) preview_render_latex(tab, text);
    tab->preview_updating = FALSE;

    if (tab->preview_scrolled) gtk_widget_set_visible(tab->preview_scrolled, TRUE);
    g_free(text);
}
