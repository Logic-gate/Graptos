/**
 * @file src/formatter_layout.c
 * @brief Structural layout pass for syntax-aware formatting.
 * @details This pass owns lines and indentation. It uses existing syntax fields
 *          for openers, closers, and statement terminators instead of inventing
 *          formatter-specific structural tokens.
 */

#include "formatter_private.h"

/**
 * @brief Return whether a string list contains a one-byte token.
 * @details The first formatter version targets the current C syntax where
 *          structural tokens are single-byte characters.
 * @param list Syntax string list.
 * @param ch Character to find.
 * @return TRUE when the list contains ch.
 */
static gboolean list_has_char(GPtrArray *list, char ch) {
    if (!list) return FALSE;
    for (guint i = 0u; i < list->len; i++) {
        const char *token = g_ptr_array_index(list, i);
        if (token && token[0] == ch && token[1] == '\0') return TRUE;
    }
    return FALSE;
}

/**
 * @brief Trim trailing spaces from output.
 * @details Structural punctuation decides exactly what follows it, so previous
 *          pending spaces are removed first.
 * @param out Formatter output.
 */
static void trim_spaces(GString *out) {
    while (out && out->len > 0u &&
           (out->str[out->len - 1u] == ' ' || out->str[out->len - 1u] == '\t')) {
        g_string_truncate(out, out->len - 1u);
    }
}

/**
 * @brief Append a newline if the output is not already at line start.
 * @details Repeated structure edits should not accumulate empty lines unless
 *          the source deliberately contained them and preservation is enabled.
 * @param context Formatter context.
 */
static void newline(GraptosFormatterContext *context) {
    if (!context || !context->out) return;
    trim_spaces(context->out);
    if (context->out->len == 0u ||
        context->out->str[context->out->len - 1u] != '\n') {
        g_string_append_c(context->out, '\n');
    }
    context->line_start = TRUE;
    context->current_column = 0u;
    context->pending_space = FALSE;
}

/**
 * @brief Append one output byte and update column state.
 * @details Layout decisions need a cheap current-column counter. The formatter
 *          targets ASCII syntax structure, so byte counting is acceptable for
 *          wrap thresholds in this first implementation.
 * @param context Formatter context.
 * @param ch Byte to append.
 */
static void append_output_char(GraptosFormatterContext *context, char ch) {
    if (!context || !context->out) return;
    g_string_append_c(context->out, ch);
    if (ch == '\n') {
        context->current_column = 0u;
        context->line_start = TRUE;
    } else {
        context->current_column++;
    }
}

void graptos_formatter_append_indent(GraptosFormatterContext *context,
                                     guint extra) {
    if (!context || !context->line_start) return;
    while (context->out && context->out->len > 0u &&
           (context->out->str[context->out->len - 1u] == ' ' ||
            context->out->str[context->out->len - 1u] == '\t')) {
        g_string_truncate(context->out, context->out->len - 1u);
    }
    guint level = context->indent_level + extra;
    if (context->preferences.insert_spaces) {
        guint width = context->preferences.tab_width == 0u ? 4u : context->preferences.tab_width;
        for (guint i = 0u; i < level * width; i++) append_output_char(context, ' ');
    } else {
        guint width = context->preferences.tab_width == 0u ? 4u : context->preferences.tab_width;
        for (guint i = 0u; i < level; i++) {
            g_string_append_c(context->out, '\t');
            context->current_column += width;
        }
    }
    context->line_start = FALSE;
}

/**
 * @brief Append pending collapsed whitespace when useful.
 * @details Leading indentation is handled separately, and spaces before common
 *          punctuation are dropped.
 * @param context Formatter context.
 * @param next Next source byte.
 */
static void flush_pending_space(GraptosFormatterContext *context, char next) {
    if (!context || !context->pending_space) return;
    if (!context->line_start && next != ')' && next != ']' && next != ';' &&
        next != ',' && next != '\0' &&
        context->out->len > 0u &&
        context->out->str[context->out->len - 1u] != ' ' &&
        context->out->str[context->out->len - 1u] != '\t') {
        append_output_char(context, ' ');
    }
    context->pending_space = FALSE;
}

/**
 * @brief Return whether upcoming text starts with else.
 * @details Stroustrup style keeps else attached to the preceding closing brace.
 * @param text Source text.
 * @param index Byte index after a closing brace.
 * @return TRUE when the next token is else.
 */
static gboolean next_token_is_else(const char *text, gsize index) {
    while (text && g_ascii_isspace((guchar)text[index])) index++;
    return text && g_str_has_prefix(text + index, "else") &&
           !g_ascii_isalnum((guchar)text[index + 4u]) && text[index + 4u] != '_';
}

/**
 * @brief Append an opening block token.
 * @details Brace placement is controlled by formatting policy, but the token
 *          itself comes from syntax->indent_openers.
 * @param context Formatter context.
 */
static void append_block_open(GraptosFormatterContext *context) {
    if (!context) return;
    trim_spaces(context->out);
    if (g_strcmp0(context->formatting->brace_style, "allman") == 0 &&
        context->out->len > 0u && context->out->str[context->out->len - 1u] != '\n') {
        newline(context);
        graptos_formatter_append_indent(context, 0u);
    } else if (context->formatting->space_before_block_opener &&
               context->out->len > 0u && context->out->str[context->out->len - 1u] != '\n' &&
               context->out->str[context->out->len - 1u] != ' ') {
        append_output_char(context, ' ');
    }
    append_output_char(context, '{');
    context->brace_depth++;
    context->indent_level++;
    newline(context);
}

/**
 * @brief Append a closing block token.
 * @details Closers reduce indentation before they are emitted because they
 *          belong to the parent block.
 * @param context Formatter context.
 * @param text Source text.
 * @param index Current source byte.
 */
static void append_block_close(GraptosFormatterContext *context,
                               const char *text,
                               gsize index) {
    if (!context) return;
    if (context->indent_level > 0u) context->indent_level--;
    if (!context->line_start) newline(context);
    graptos_formatter_append_indent(context, 0u);
    append_output_char(context, '}');
    if (context->brace_depth > 0u) context->brace_depth--;
    if (g_strcmp0(context->formatting->brace_style, "stroustrup") == 0 &&
        next_token_is_else(text, index + 1u)) {
        append_output_char(context, ' ');
        context->line_start = FALSE;
    } else {
        newline(context);
    }
}

/**
 * @brief Append a normal byte.
 * @details Normal text receives indentation and any pending collapsed space.
 * @param context Formatter context.
 * @param ch Byte to append.
 */
static void append_plain(GraptosFormatterContext *context, char ch) {
    gboolean was_line_start = context ? context->line_start : FALSE;
    graptos_formatter_append_indent(context, 0u);
    if (was_line_start) context->pending_space = FALSE;
    flush_pending_space(context, ch);
    append_output_char(context, ch);
    context->line_start = FALSE;
}

/**
 * @brief Return whether a comma should end the current line.
 * @details The first wrap pass is deliberately narrow: only configured comma
 *          sites wrap, and only after the line already exceeds max_column.
 * @param context Formatter context.
 * @return TRUE when the comma should be followed by a newline.
 */
static gboolean comma_should_wrap(GraptosFormatterContext *context) {
    return context && context->formatting &&
           context->formatting->wrap_after_comma &&
           context->formatting->max_column > 0u &&
           context->current_column >= context->formatting->max_column;
}

/**
 * @brief Return whether the next meaningful byte is a block closer.
 * @details Blank source lines directly before `}` usually come from previous
 *          layout and should not survive as an empty line in the formatted
 *          block.
 * @param text Source text.
 * @param index Byte index after a newline.
 * @return TRUE when optional whitespace is followed by `}`.
 */
static gboolean next_meaningful_is_closer(const char *text, gsize index) {
    while (text && (text[index] == ' ' || text[index] == '\t' ||
                    text[index] == '\r' || text[index] == '\n')) {
        index++;
    }
    return text && text[index] == '}';
}

void graptos_formatter_layout(GraptosFormatterContext *context,
                              const char *text) {
    if (!context || !text) return;
    guint blank_lines_pending = 0u;
    for (gsize i = 0u; text[i];) {
        if (graptos_formatter_copy_protected(context, text, &i)) {
            context->line_start = FALSE;
            continue;
        }

        char ch = text[i];
        if (ch == '\r') {
            i++;
            continue;
        }
        if (ch == '\n') {
            if (context->line_start) {
                guint max_blank = context->formatting->max_blank_lines;
                if (context->formatting->preserve_blank_lines &&
                    !next_meaningful_is_closer(text, i + 1u) &&
                    (max_blank == 0u || blank_lines_pending < max_blank)) {
                    append_output_char(context, '\n');
                    blank_lines_pending++;
                }
            } else {
                newline(context);
                blank_lines_pending = 0u;
            }
            i++;
            continue;
        }
        if (ch == ' ' || ch == '\t') {
            if (context->line_start) {
                context->pending_space = FALSE;
            } else {
                context->pending_space = TRUE;
            }
            i++;
            continue;
        }
        blank_lines_pending = 0u;

        if (list_has_char(context->syntax->indent_openers, ch) &&
            context->parenthesis_depth == 0u && context->bracket_depth == 0u) {
            append_block_open(context);
            i++;
            continue;
        }
        if (list_has_char(context->syntax->indent_closers, ch) &&
            context->parenthesis_depth == 0u && context->bracket_depth == 0u) {
            append_block_close(context, text, i);
            i++;
            continue;
        }
        if (ch == '(') {
            append_plain(context, ch);
            context->parenthesis_depth++;
            i++;
            continue;
        }
        if (ch == ')') {
            trim_spaces(context->out);
            append_plain(context, ch);
            if (context->parenthesis_depth > 0u) context->parenthesis_depth--;
            i++;
            continue;
        }
        if (ch == '[') {
            append_plain(context, ch);
            context->bracket_depth++;
            i++;
            continue;
        }
        if (ch == ']') {
            trim_spaces(context->out);
            append_plain(context, ch);
            if (context->bracket_depth > 0u) context->bracket_depth--;
            i++;
            continue;
        }
        if (ch == ',' && context->formatting->space_after_comma) {
            trim_spaces(context->out);
            append_output_char(context, ',');
            if (comma_should_wrap(context)) {
                newline(context);
                graptos_formatter_append_indent(context, 1u);
            } else {
                append_output_char(context, ' ');
            }
            context->line_start = FALSE;
            context->pending_space = FALSE;
            i++;
            while (text[i] == ' ' || text[i] == '\t') i++;
            continue;
        }
        if (list_has_char(context->syntax->statement_required_enders, ch) &&
            context->parenthesis_depth == 0u && context->bracket_depth == 0u) {
            trim_spaces(context->out);
            append_output_char(context, ch);
            if (context->formatting->one_statement_per_line) newline(context);
            i++;
            continue;
        }
        if (graptos_formatter_append_operator(context, text, &i)) continue;

        append_plain(context, ch);
        i++;
    }
    trim_spaces(context->out);
}
