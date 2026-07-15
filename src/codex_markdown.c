/**
 * @file src/codex_markdown.c
 * @brief Codex response markdown renderer.
 */

#include "codex_markdown.h"
#include <stdarg.h>
#include <string.h>

/**
 * @brief Codex markdown type definition.
 */
typedef struct { GtkTextBuffer *buffer; GtkTextIter iter; } Writer;

/**
 * @brief Tag ensure.
 */
static void tag_ensure(GtkTextBuffer *buffer, const char *name,
                       const char *first, ...) {
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    if (gtk_text_tag_table_lookup(table, name)) return;
    GtkTextTag *tag = gtk_text_tag_new(name);
    va_list args;
    va_start(args, first);
    g_object_set_valist(G_OBJECT(tag), first, args);
    va_end(args);
    gtk_text_tag_table_add(table, tag);
    g_object_unref(tag);
}

/**
 * @brief Tags ensure.
 */
static void tags_ensure(GtkTextBuffer *buffer) {
    tag_ensure(buffer, "ai-h1", "weight", PANGO_WEIGHT_BOLD, "scale", 1.55,
               "foreground", "#89b4fa", "pixels-above-lines", 10, NULL);
    tag_ensure(buffer, "ai-h2", "weight", PANGO_WEIGHT_BOLD, "scale", 1.30,
               "foreground", "#a6e3a1", "pixels-above-lines", 8, NULL);
    tag_ensure(buffer, "ai-h3", "weight", PANGO_WEIGHT_BOLD, "scale", 1.12,
               "foreground", "#f9e2af", NULL);
    tag_ensure(buffer, "ai-bold", "weight", PANGO_WEIGHT_BOLD, NULL);
    tag_ensure(buffer, "ai-italic", "style", PANGO_STYLE_ITALIC, NULL);
    tag_ensure(buffer, "ai-code", "family", "monospace", "foreground", "#f1f1f1",
               "background", "#282c34", NULL);
    tag_ensure(buffer, "ai-quote", "style", PANGO_STYLE_ITALIC,
               "foreground", "#b7c5d3", "left-margin", 12, NULL);
    tag_ensure(buffer, "ai-link", "foreground", "#7dcfff",
               "underline", PANGO_UNDERLINE_SINGLE, NULL);
}

/**
 * @brief Put.
 */
static void put(Writer *writer, const char *text) {
    gtk_text_buffer_insert(writer->buffer, &writer->iter, text ? text : "", -1);
}

/**
 * @brief Put tag.
 */
static void put_tag(Writer *writer, const char *text, const char *tag) {
    gint offset = gtk_text_iter_get_offset(&writer->iter);
    put(writer, text);
    GtkTextIter start;
    gtk_text_buffer_get_iter_at_offset(writer->buffer, &start, offset);
    gtk_text_buffer_apply_tag_by_name(writer->buffer, tag, &start, &writer->iter);
}

/**
 * @brief Inline render.
 */
static void inline_render(Writer *writer, const char *line) {
    const char *p = line ? line : "";
    while (*p) {
        const char *end = NULL;
        if (g_str_has_prefix(p, "**") && (end = strstr(p + 2, "**"))) {
            char *s = g_strndup(p + 2, (gsize)(end - p - 2));
            put_tag(writer, s, "ai-bold"); g_free(s); p = end + 2;
        } else if (*p == '`' && (end = strchr(p + 1, '`'))) {
            char *s = g_strndup(p + 1, (gsize)(end - p - 1));
            put_tag(writer, s, "ai-code"); g_free(s); p = end + 1;
        } else if (*p == '*' && (end = strchr(p + 1, '*'))) {
            char *s = g_strndup(p + 1, (gsize)(end - p - 1));
            put_tag(writer, s, "ai-italic"); g_free(s); p = end + 1;
        } else if (*p == '[' && (end = strstr(p + 1, "]("))) {
            const char *close = strchr(end + 2, ')');
            if (close) {
                char *s = g_strndup(p + 1, (gsize)(end - p - 1));
                put_tag(writer, s, "ai-link"); g_free(s); p = close + 1;
            } else { put(writer, "["); p++; }
        } else {
            const char *next = g_utf8_next_char(p);
            char *s = g_strndup(p, (gsize)(next - p));
            put(writer, s); g_free(s); p = next;
        }
    }
}

/**
 * @brief Line render.
 */
static void line_render(Writer *writer, const char *line, gboolean *code) {
    if (g_str_has_prefix(line, "```") || g_str_has_prefix(line, "~~~")) {
        *code = !*code; return;
    }
    if (*code) put_tag(writer, line, "ai-code");
    else if (g_str_has_prefix(line, "### ")) put_tag(writer, line + 4, "ai-h3");
    else if (g_str_has_prefix(line, "## ")) put_tag(writer, line + 3, "ai-h2");
    else if (g_str_has_prefix(line, "# ")) put_tag(writer, line + 2, "ai-h1");
    else if (g_str_has_prefix(line, "> ")) put_tag(writer, line + 2, "ai-quote");
    else if (g_str_has_prefix(line, "- ") || g_str_has_prefix(line, "* ")) {
        put(writer, "• "); inline_render(writer, line + 2);
    } else inline_render(writer, line);
    put(writer, "\n");
}

/**
 * @brief Codex markdown render.
 */
void codex_markdown_render(GtkTextBuffer *buffer, const char *markdown) {
    if (!buffer) return;
    gtk_text_buffer_set_text(buffer, "", 0);
    tags_ensure(buffer);
    Writer writer = { .buffer = buffer };
    gtk_text_buffer_get_end_iter(buffer, &writer.iter);
    gboolean code = FALSE;
    char **lines = g_strsplit(markdown ? markdown : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) line_render(&writer, lines[i], &code);
    g_strfreev(lines);
}
