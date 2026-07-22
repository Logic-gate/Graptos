/**
 * @file src/formatter_spacing.c
 * @brief Token-local whitespace normalization for the formatter.
 * @details Layout decides lines; this module handles small spacing choices such
 *          as operators and pointer declarators.
 */

#include "formatter_private.h"

/**
 * @brief Remove trailing spaces from formatted output.
 * @details Most formatter actions want to decide the exact next separator, so
 *          pending and emitted spaces are cleared before structural tokens.
 * @param out Output buffer.
 */
static void trim_output_spaces(GString *out) {
    while (out && out->len > 0u &&
           (out->str[out->len - 1u] == ' ' || out->str[out->len - 1u] == '\t')) {
        g_string_truncate(out, out->len - 1u);
    }
}

/**
 * @brief Append one output byte and keep formatter column state close enough.
 * @details Spacing helpers mostly emit short ASCII operator fragments. Updating
 *          the column here keeps later comma wrapping from seeing stale columns.
 * @param context Formatter context.
 * @param ch Byte to append.
 */
static void spacing_append_char(GraptosFormatterContext *context, char ch) {
    if (!context || !context->out) return;
    g_string_append_c(context->out, ch);
    if (ch == '\n') context->current_column = 0u;
    else context->current_column++;
}

/**
 * @brief Return whether a byte can be part of an identifier.
 * @details This intentionally uses C-like ASCII identifiers for v1 because C is
 *          the first syntax with formatting enabled.
 * @param ch Byte to test.
 * @return TRUE for identifier bytes.
 */
static gboolean ident_byte(char ch) {
    return g_ascii_isalnum((guchar)ch) || ch == '_';
}

/**
 * @brief Return whether the current line looks like a declaration.
 * @details Pointer alignment should avoid multiplication. A line with an
 *          assignment before the star is treated as expression code.
 * @param out Output generated so far.
 * @return TRUE when pointer-declaration spacing is plausible.
 */
static gboolean current_line_declarative(GString *out) {
    if (!out || out->len == 0u) return FALSE;
    const char *start = out->str + out->len;
    while (start > out->str && start[-1] != '\n') start--;
    gboolean saw_ident = FALSE;
    for (const char *p = start; *p; p++) {
        if (*p == '=' || *p == ';' || *p == ')' || *p == ']') return FALSE;
        if (ident_byte(*p)) saw_ident = TRUE;
    }
    return saw_ident;
}

/**
 * @brief Return whether a following token can be a declarator name.
 * @details The pointer heuristic needs an identifier after optional spaces.
 * @param text Source text.
 * @param index Byte after the star.
 * @return TRUE when an identifier follows.
 */
static gboolean next_is_identifier(const char *text, gsize index) {
    while (text && (text[index] == ' ' || text[index] == '\t')) index++;
    return text && (g_ascii_isalpha((guchar)text[index]) || text[index] == '_');
}

/**
 * @brief Return whether one list entry matches source at index.
 * @details Operator policy is syntax-driven. Exact token matching lets the same
 *          backend support symbolic and word operators without language
 *          branches.
 * @param list Formatter token list.
 * @param text Source text.
 * @param index Source byte offset.
 * @param matched_len Length of the matched token.
 * @return TRUE when a token matched.
 */
static gboolean match_list_token(GPtrArray *list,
                                 const char *text,
                                 gsize index,
                                 gsize *matched_len) {
    if (matched_len) *matched_len = 0u;
    if (!list || !text) return FALSE;
    const char *best = NULL;
    gsize best_len = 0u;
    for (guint i = 0u; i < list->len; i++) {
        const char *token = g_ptr_array_index(list, i);
        if (!token || token[0] == '\0') continue;
        gsize len = strlen(token);
        if (len <= best_len) continue;
        if (g_str_has_prefix(text + index, token)) {
            gboolean word_start = ident_byte(token[0]);
            gboolean word_end = ident_byte(token[len - 1u]);
            char previous = index > 0u ? text[index - 1u] : '\0';
            char next = text[index + len];
            if ((word_start && ident_byte(previous)) || (word_end && ident_byte(next))) {
                continue;
            }
            best = token;
            best_len = len;
        }
    }
    if (!best) return FALSE;
    if (matched_len) *matched_len = best_len;
    return TRUE;
}

/**
 * @brief Return whether a symbolic byte belongs to the legacy operator set.
 * @details Test fixtures can build SyntaxFormatting by hand. The fallback keeps
 *          those fixtures and old syntax files working while YAML gains
 *          explicit operator lists.
 * @param ch Byte to classify.
 * @return TRUE for a common C-like operator byte.
 */
static gboolean fallback_operator_byte(char ch) {
    return strchr("+-*/%=!<>|&^", ch) != NULL;
}

/**
 * @brief Append a compact source token.
 * @details Prefix operators such as `!` should not grow spaces before the
 *          operand. The token can still be more than one byte for non-C-like
 *          languages when their profiles are implemented.
 * @param context Formatter context.
 * @param text Source text.
 * @param index Current source byte index.
 * @param len Token byte length.
 */
static void append_compact_token(GraptosFormatterContext *context,
                                 const char *text,
                                 gsize *index,
                                 gsize len) {
    for (gsize i = 0u; i < len; i++) spacing_append_char(context, text[*index + i]);
    *index += len;
}

gboolean graptos_formatter_append_pointer(GraptosFormatterContext *context,
                                          const char *text,
                                          gsize *index) {
    if (!context || !context->formatting || !text || !index || text[*index] != '*') {
        return FALSE;
    }
    if (!current_line_declarative(context->out) || !next_is_identifier(text, *index + 1u)) {
        return FALSE;
    }

    trim_output_spaces(context->out);
    context->pending_space = FALSE;
    const char *alignment = context->formatting->pointer_alignment;
    if (g_strcmp0(alignment, "type") == 0) {
        g_string_append(context->out, "* ");
    } else if (g_strcmp0(alignment, "middle") == 0) {
        g_string_append(context->out, " * ");
    } else {
        g_string_append(context->out, " *");
    }
    (*index)++;
    while (text[*index] == ' ' || text[*index] == '\t') (*index)++;
    return TRUE;
}

gboolean graptos_formatter_append_operator(GraptosFormatterContext *context,
                                           const char *text,
                                           gsize *index) {
    if (!context || !context->formatting || !text || !index) return FALSE;
    char ch = text[*index];
    char next = text[*index + 1u];
    char previous = *index > 0u ? text[*index - 1u] : '\0';
    gsize logical_len = 0u;
    gsize unary_len = 0u;
    gboolean matched_logical = match_list_token(context->formatting->logical_operators,
                                                text,
                                                *index,
                                                &logical_len);
    gboolean matched_unary = match_list_token(context->formatting->unary_prefix_operators,
                                              text,
                                              *index,
                                              &unary_len);
    gboolean matched_binary = match_list_token(context->formatting->binary_operators,
                                               text,
                                               *index,
                                               NULL);
    if (!matched_logical && !matched_unary && !matched_binary &&
        !fallback_operator_byte(ch)) {
        return FALSE;
    }
    if (matched_logical || (ch == '&' && next == '&') || (ch == '|' && next == '|')) {
        if (context->formatting->space_around_logical_operators) {
            trim_output_spaces(context->out);
            spacing_append_char(context, ' ');
            append_compact_token(context, text, index, matched_logical ? logical_len : 2u);
            spacing_append_char(context, ' ');
            context->pending_space = FALSE;
        } else {
            append_compact_token(context, text, index, matched_logical ? logical_len : 2u);
        }
        while (text[*index] == ' ' || text[*index] == '\t') (*index)++;
        return TRUE;
    }
    if ((ch == '+' && next == '+') || (ch == '-' && next == '-') ||
        (ch == '-' && next == '>')) {
        spacing_append_char(context, ch);
        spacing_append_char(context, next);
        *index += 2u;
        return TRUE;
    }
    if ((ch == '&' && previous == '&') || (ch == '|' && previous == '|')) {
        spacing_append_char(context, ch);
        (*index)++;
        return TRUE;
    }
    if ((matched_unary && unary_len > 0u) || (ch == '!' && next != '=')) {
        if (context->pending_space && context->out->len > 0u &&
            context->out->str[context->out->len - 1u] != ' ' &&
            context->out->str[context->out->len - 1u] != '\t') {
            spacing_append_char(context, ' ');
        }
        context->pending_space = FALSE;
        append_compact_token(context, text, index, matched_unary ? unary_len : 1u);
        while (text[*index] == ' ' || text[*index] == '\t') (*index)++;
        return TRUE;
    }
    if (ch == '*' && graptos_formatter_append_pointer(context, text, index)) return TRUE;

    trim_output_spaces(context->out);
    if (context->formatting->space_around_binary_operators) {
        spacing_append_char(context, ' ');
        spacing_append_char(context, ch);
        if (next == '=' || (ch == '<' && next == '<') || (ch == '>' && next == '>') ||
            (ch == '&' && next == '&') || (ch == '|' && next == '|')) {
            spacing_append_char(context, next);
            (*index)++;
        }
        spacing_append_char(context, ' ');
        context->pending_space = FALSE;
    } else {
        spacing_append_char(context, ch);
    }
    (*index)++;
    while (text[*index] == ' ' || text[*index] == '\t') (*index)++;
    return TRUE;
}
