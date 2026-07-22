/**
 * @file src/syntax_highlight.c
 * @brief Legacy syntax tag application helpers.
 * @details The syntax layer gives Graptoς useful language behavior without requiring LSP.
 *          YAML keeps the language definitions editable, while this code handles loading,
 *          highlighting, diagnostics, and safe fallbacks.
 */

#include "syntax.h"

#include <string.h>

/**
 * @brief Tag name for rule.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param rule The rule supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *tag_name_for_rule(const SyntaxRule *rule) {
    return g_strdup_printf("graptos-syntax-%p", (const void *)rule);
}

/**
 * @brief Ensure rule tag.
 * @details Tags are keyed by the rule pointer because YAML syntax rules are
 *          already unique after loading. That avoids inventing global names for
 *          every regex while still letting GTK reuse tags between highlights.
 * @param buffer The text buffer used for the operation.
 * @param rule The rule supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkTextTag *ensure_rule_tag(GtkTextBuffer *buffer, const SyntaxRule *rule) {
    if (!buffer || !rule) return NULL;
    char *name = tag_name_for_rule(rule);
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, name);
    if (!tag) {
        tag = gtk_text_buffer_create_tag(buffer, name,
                                         "foreground", rule->color ? rule->color : "#d4d4d4",
                                         "weight", rule->bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
                                         "style", rule->italic ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL,
                                         "underline", rule->underline ? PANGO_UNDERLINE_SINGLE : PANGO_UNDERLINE_NONE,
                                         NULL);
    }
    g_free(name);
    return tag;
}

/**
 * @brief Syntax iter range for lines.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param buffer The text buffer used for the operation.
 * @param start_line The start line supplied by the caller.
 * @param end_line The end line supplied by the caller.
 * @param start The start supplied by the caller.
 * @param end The end supplied by the caller.
 */
static void syntax_iter_range_for_lines(GtkTextBuffer *buffer, guint start_line, guint end_line, GtkTextIter *start, GtkTextIter *end) {
    if (!buffer || !start || !end) return;

    gint line_count = gtk_text_buffer_get_line_count(buffer);
    if (line_count <= 0) {
        gtk_text_buffer_get_bounds(buffer, start, end);
        return;
    }

    if (start_line >= (guint)line_count) start_line = (guint)line_count - 1u;
    if (end_line >= (guint)line_count) end_line = (guint)line_count - 1u;
    if (end_line < start_line) end_line = start_line;

    gtk_text_buffer_get_iter_at_line(buffer, start, (gint)start_line);
    if (end_line + 1u >= (guint)line_count) {
        gtk_text_buffer_get_end_iter(buffer, end);
    } else {
        gtk_text_buffer_get_iter_at_line(buffer, end, (gint)(end_line + 1u));
    }
}

/**
 * @brief Syntax clear range.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param buffer The text buffer used for the operation.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param start_line The start line supplied by the caller.
 * @param end_line The end line supplied by the caller.
 */
void syntax_clear_range(GtkTextBuffer *buffer, GPtrArray *syntaxes, guint start_line, guint end_line) {
    if (!buffer || !syntaxes) return;
    GtkTextIter start;
    GtkTextIter end;
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    syntax_iter_range_for_lines(buffer, start_line, end_line, &start, &end);
    for (guint i = 0; i < syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(syntaxes, i);
        if (!syntax || !syntax->rules) continue;
        for (guint j = 0; j < syntax->rules->len; j++) {
            SyntaxRule *rule = g_ptr_array_index(syntax->rules, j);
            char *name = tag_name_for_rule(rule);
            GtkTextTag *tag = gtk_text_tag_table_lookup(table, name);
            if (tag) gtk_text_buffer_remove_tag(buffer, tag, &start, &end);
            g_free(name);
        }
    }
}

/**
 * @brief Syntax clear.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param buffer The text buffer used for the operation.
 * @param syntaxes The syntaxes supplied by the caller.
 */
void syntax_clear(GtkTextBuffer *buffer, GPtrArray *syntaxes) {
    if (!buffer) return;
    gint lines = gtk_text_buffer_get_line_count(buffer);
    syntax_clear_range(buffer, syntaxes, 0u, lines > 0 ? (guint)lines - 1u : 0u);
}

/**
 * @brief Syntax apply range.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param buffer The text buffer used for the operation.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param active_syntax The active syntax supplied by the caller.
 * @param start_line The start line supplied by the caller.
 * @param end_line The end line supplied by the caller.
 */
void syntax_apply_range(GtkTextBuffer *buffer, GPtrArray *syntaxes, SyntaxDef *active_syntax, guint start_line, guint end_line) {
    if (!buffer) return;

    syntax_clear_range(buffer, syntaxes, start_line, end_line);
    if (!active_syntax || !active_syntax->rules) return;

    GtkTextIter start;
    GtkTextIter end;
    syntax_iter_range_for_lines(buffer, start_line, end_line, &start, &end);
    gint base_offset = gtk_text_iter_get_offset(&start);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (!text) return;

    for (guint r = 0; r < active_syntax->rules->len; r++) {
        SyntaxRule *rule = g_ptr_array_index(active_syntax->rules, r);
        if (!rule || !rule->regex) continue;
        GtkTextTag *tag = ensure_rule_tag(buffer, rule);
        if (!tag) continue;

        GMatchInfo *match_info = NULL;
        gboolean matched = g_regex_match(rule->regex, text, 0, &match_info);
        guint count = 0u;
        while (matched && match_info && count < GRAPTOS_MAX_REGEX_MATCHES_PER_RULE) {
            gint start_pos = -1;
            gint end_pos = -1;
            if (g_match_info_fetch_pos(match_info, 0, &start_pos, &end_pos) && end_pos > start_pos) {
                glong char_start = g_utf8_pointer_to_offset(text, text + start_pos);
                glong char_end = g_utf8_pointer_to_offset(text, text + end_pos);
                GtkTextIter a;
                GtkTextIter b;
                gtk_text_buffer_get_iter_at_offset(buffer, &a, base_offset + (gint)char_start);
                gtk_text_buffer_get_iter_at_offset(buffer, &b, base_offset + (gint)char_end);
                gtk_text_buffer_apply_tag(buffer, tag, &a, &b);
            } else if (end_pos == start_pos) {
                break;
            }
            count++;
            matched = g_match_info_next(match_info, NULL);
        }
        if (match_info) g_match_info_free(match_info);
    }
    g_free(text);
}

/**
 * @brief Syntax apply.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param buffer The text buffer used for the operation.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param active_syntax The active syntax supplied by the caller.
 */
void syntax_apply(GtkTextBuffer *buffer, GPtrArray *syntaxes, SyntaxDef *active_syntax) {
    if (!buffer) return;

    if ((guint)gtk_text_buffer_get_char_count(buffer) > GRAPTOS_MAX_HIGHLIGHT_BYTES) {
        return;
    }

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (!text) return;
    gsize bytes = strlen(text);
    if (bytes > GRAPTOS_MAX_HIGHLIGHT_BYTES) {
        g_free(text);
        return;
    }
    g_free(text);

    gint lines = gtk_text_buffer_get_line_count(buffer);
    syntax_apply_range(buffer, syntaxes, active_syntax, 0u, lines > 0 ? (guint)lines - 1u : 0u);
}
