/**
 * @file src/codex_markdown.c
 * @brief Codex response markdown renderer.
 * @details AI touches the parts of the app where mistakes are expensive: files,
 *          permissions, markdown, and long-running processes. We keep protocol, client,
 *          panel, and review logic separated so approval and cleanup paths are easy to
 *          audit.
 */

#include "codex_markdown.h"
#include <stdarg.h>
#include <string.h>

/**
 * @brief Codex markdown type definition.
 * @details Rendering walks a GtkTextBuffer iter forward. This tiny writer keeps
 *          the buffer and current insert point together so inline and block
 *          renderers do not pass both around separately.
 */
typedef struct { GtkTextBuffer *buffer; GtkTextIter iter; } Writer;

/**
 * @brief Tag ensure.
 * @details Tags are created lazily because the response buffer can be replaced
 *          many times. Reusing tag names keeps re-rendering cheap and avoids
 *          filling the tag table with duplicate styles.
 * @param buffer The text buffer used for the operation.
 * @param name The name supplied by the caller.
 * @param first The first supplied by the caller.
 */
static void tag_ensure(GtkTextBuffer *buffer, const char *name,
                       const char *first, ...) {
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, name);
    gboolean created = FALSE;
    if (!tag) {
        tag = gtk_text_tag_new(name);
        created = TRUE;
    }
    va_list args;
    va_start(args, first);
    g_object_set_valist(G_OBJECT(tag), first, args);
    va_end(args);
    if (created) {
        gtk_text_tag_table_add(table, tag);
        g_object_unref(tag);
    }
}

/**
 * @brief Tags ensure.
 * @details The renderer intentionally supports a small markdown subset. These
 *          tags cover the pieces Codex commonly emits without turning the
 *          response pane into a browser widget.
 * @param buffer The text buffer used for the operation.
 */
static void tags_ensure(GtkTextBuffer *buffer,
                        const char *heading_color,
                        const char *success_color,
                        const char *warning_color,
                        const char *code_fg_color,
                        const char *code_bg_color,
                        const char *quote_color) {
    const char *heading = heading_color && heading_color[0] == '#'
        ? heading_color : NULL;
    const char *success = success_color && success_color[0] == '#'
        ? success_color : heading;
    const char *warning = warning_color && warning_color[0] == '#'
        ? warning_color : heading;
    const char *code_fg = code_fg_color && code_fg_color[0] == '#'
        ? code_fg_color : NULL;
    const char *code_bg = code_bg_color && code_bg_color[0] == '#'
        ? code_bg_color : NULL;
    const char *quote = quote_color && quote_color[0] == '#'
        ? quote_color : code_fg;
    tag_ensure(buffer, "ai-h1", "weight", PANGO_WEIGHT_BOLD, "scale", 1.55,
               "foreground", heading, "pixels-above-lines", 10, NULL);
    tag_ensure(buffer, "ai-h2", "weight", PANGO_WEIGHT_BOLD, "scale", 1.30,
               "foreground", success, "pixels-above-lines", 8, NULL);
    tag_ensure(buffer, "ai-h3", "weight", PANGO_WEIGHT_BOLD, "scale", 1.12,
               "foreground", warning, NULL);
    tag_ensure(buffer, "ai-bold", "weight", PANGO_WEIGHT_BOLD, NULL);
    tag_ensure(buffer, "ai-italic", "style", PANGO_STYLE_ITALIC, NULL);
    tag_ensure(buffer, "ai-code", "family", "monospace", "foreground", code_fg,
               "background", code_bg, NULL);
    tag_ensure(buffer, "ai-quote", "style", PANGO_STYLE_ITALIC,
               "foreground", quote, "left-margin", 12, NULL);
    tag_ensure(buffer, "ai-link", "foreground", heading,
               "underline", PANGO_UNDERLINE_SINGLE, NULL);
}

/**
 * @brief Put.
 * @details Plain insertion still goes through a helper so escaped/unknown
 *          markdown falls back to visible text instead of being silently dropped.
 * @param writer The writer supplied by the caller.
 * @param text The text fragment supplied by the caller.
 */
static void put(Writer *writer, const char *text) {
    gtk_text_buffer_insert(writer->buffer, &writer->iter, text ? text : "", -1);
}

/**
 * @brief Put tag.
 * @details GtkTextBuffer applies tags over ranges, not over future writes. We
 *          remember the offset before inserting, then tag only the text we just
 *          added.
 * @param writer The writer supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param tag The tag supplied by the caller.
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
 * @details Inline parsing is deliberately forgiving. If a construct is not
 *          closed, we render the original character and move on so malformed AI
 *          output never eats the rest of the response.
 * @param writer The writer supplied by the caller.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
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
 * @details Blocks are handled line-by-line because the response pane only needs
 *          readable chat output. Fenced code toggles a simple state instead of
 *          building a full markdown AST.
 * @param writer The writer supplied by the caller.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param code The code supplied by the caller.
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
 * @details Rendering replaces the whole buffer. Codex responses are appendable
 *          at the panel level, while this function only owns transforming the
 *          current markdown string into styled text.
 * @param buffer The text buffer used for the operation.
 * @param markdown The markdown supplied by the caller.
 * @param heading_color The primary heading/link color.
 * @param success_color The secondary heading color.
 * @param warning_color The tertiary heading color.
 * @param code_fg_color The code foreground color.
 * @param code_bg_color The code background color.
 * @param quote_color The quoted text color.
 */
void codex_markdown_render(GtkTextBuffer *buffer,
                           const char *markdown,
                           const char *heading_color,
                           const char *success_color,
                           const char *warning_color,
                           const char *code_fg_color,
                           const char *code_bg_color,
                           const char *quote_color) {
    if (!buffer) return;
    gtk_text_buffer_set_text(buffer, "", 0);
    tags_ensure(buffer,
                heading_color,
                success_color,
                warning_color,
                code_fg_color,
                code_bg_color,
                quote_color);
    Writer writer = { .buffer = buffer };
    gtk_text_buffer_get_end_iter(buffer, &writer.iter);
    gboolean code = FALSE;
    char **lines = g_strsplit(markdown ? markdown : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) line_render(&writer, lines[i], &code);
    g_strfreev(lines);
}
