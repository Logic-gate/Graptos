/**
 * @file src/formatter_lexer.c
 * @brief Protected lexical-region handling for the formatter.
 * @details Formatting code can rearrange whitespace around code, but comments,
 *          strings, characters, and preprocessor lines must be copied with their
 *          internal bytes intact.
 */

#include "formatter_private.h"

/**
 * @brief Return whether text at index begins a line after optional spaces.
 * @details Preprocessor protection is line based. Leading spaces still count as
 *          the beginning of the directive.
 * @param text Source text.
 * @param index Byte index being tested.
 * @return TRUE when only spaces or tabs precede index on the same line.
 */
static gboolean at_logical_line_start(const char *text, gsize index) {
    while (index > 0u) {
        char previous = text[index - 1u];
        if (previous == '\n' || previous == '\r') return TRUE;
        if (previous != ' ' && previous != '\t') return FALSE;
        index--;
    }
    return TRUE;
}

/**
 * @brief Copy a quoted literal.
 * @details Escaped bytes keep the next byte protected, which is enough for C
 *          strings and character literals in the first formatter pass.
 * @param out Destination string.
 * @param text Source text.
 * @param index Current byte index.
 * @param quote Quote byte.
 */
static void copy_quoted(GString *out, const char *text, gsize *index, char quote) {
    g_string_append_c(out, text[*index]);
    (*index)++;
    gboolean escaped = FALSE;
    while (text[*index]) {
        char ch = text[*index];
        g_string_append_c(out, ch);
        (*index)++;
        if (escaped) {
            escaped = FALSE;
        } else if (ch == '\\') {
            escaped = TRUE;
        } else if (ch == quote) {
            break;
        }
    }
}

/**
 * @brief Copy a line comment.
 * @details The newline is left for layout handling so line-start state remains
 *          centralized.
 * @param out Destination string.
 * @param text Source text.
 * @param index Current byte index.
 */
static void copy_line_comment(GString *out, const char *text, gsize *index) {
    while (text[*index] && text[*index] != '\n') {
        g_string_append_c(out, text[*index]);
        (*index)++;
    }
}

/**
 * @brief Copy a block comment.
 * @details Interior indentation is preserved because comments are prose as much
 *          as code structure.
 * @param out Destination string.
 * @param text Source text.
 * @param index Current byte index.
 */
static void copy_block_comment(GString *out, const char *text, gsize *index) {
    g_string_append(out, "/*");
    *index += 2u;
    while (text[*index]) {
        if (text[*index] == '*' && text[*index + 1u] == '/') {
            g_string_append(out, "*/");
            *index += 2u;
            return;
        }
        g_string_append_c(out, text[*index]);
        (*index)++;
    }
}

/**
 * @brief Copy a preprocessor directive, including continued lines.
 * @details Macro bodies are explicitly outside v1 formatting. Backslash-newline
 *          continuations keep every physical macro line protected.
 * @param out Destination string.
 * @param text Source text.
 * @param index Current byte index.
 */
static void copy_preprocessor(GString *out, const char *text, gsize *index) {
    gboolean continued = TRUE;
    while (text[*index] && continued) {
        continued = FALSE;
        while (text[*index] && text[*index] != '\n') {
            g_string_append_c(out, text[*index]);
            (*index)++;
        }
        if (*index > 0u && text[*index - 1u] == '\\') continued = TRUE;
        if (text[*index] == '\n') {
            g_string_append_c(out, '\n');
            (*index)++;
        }
    }
}

/**
 * @brief Prepare output before copying protected inline text.
 * @details The lexer is called speculatively for every byte, so preparation must
 *          happen only after a protected region has actually matched.
 * @param context Formatter context.
 */
static void prepare_protected_copy(GraptosFormatterContext *context) {
    gboolean was_line_start = context->line_start;
    graptos_formatter_append_indent(context, 0u);
    if (context->pending_space && !was_line_start) g_string_append_c(context->out, ' ');
    context->pending_space = FALSE;
}

/**
 * @brief Return whether a protected semantic scope is enabled.
 * @details Formatting configs can narrow protected scopes, but missing lists
 *          keep the safe default because modifying literal/comment interiors is
 *          worse than preserving too much text.
 * @param context Formatter context.
 * @param scope Semantic scope name.
 * @return TRUE when the scope should be copied unchanged.
 */
static gboolean protected_scope_enabled(GraptosFormatterContext *context,
                                        const char *scope) {
    if (!context || !context->formatting || !scope) return TRUE;
    GPtrArray *scopes = context->formatting->protected_scopes;
    if (!scopes || scopes->len == 0u) return TRUE;
    for (guint i = 0u; i < scopes->len; i++) {
        const char *item = g_ptr_array_index(scopes, i);
        if (g_strcmp0(item, scope) == 0) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Return whether source starts with the configured line comment token.
 * @details This uses the existing syntax-level `line_comment` field instead of
 *          tying the formatter to C-family `//` comments.
 * @param context Formatter context.
 * @param text Source text.
 * @param index Source byte index.
 * @return TRUE when a line comment begins at index.
 */
static gboolean starts_line_comment(GraptosFormatterContext *context,
                                    const char *text,
                                    gsize index) {
    const char *token = context && context->syntax ? context->syntax->line_comment : NULL;
    return token && token[0] != '\0' && text && g_str_has_prefix(text + index, token);
}

/**
 * @brief Return whether syntax rules mention a token in comments.
 * @details Some syntaxes support more than one line-comment prefix while the
 *          legacy syntax model has one `line_comment` string. Looking at
 *          comment-scoped rules keeps this generic and avoids a PHP-specific
 *          branch for `#`.
 * @param syntax Active syntax definition.
 * @param token Token text to find in comment rules.
 * @return TRUE when a comment rule pattern mentions token.
 */
static gboolean comment_rules_mention_token(const SyntaxDef *syntax,
                                            const char *token) {
    if (!syntax || !syntax->rules || !token) return FALSE;
    for (guint i = 0u; i < syntax->rules->len; i++) {
        SyntaxRule *rule = g_ptr_array_index(syntax->rules, i);
        gboolean is_comment = rule &&
            (g_strcmp0(rule->scope, "comment") == 0 ||
             (rule->name && strstr(rule->name, "comment") != NULL));
        if (is_comment && rule->pattern && strstr(rule->pattern, token) != NULL) {
            return TRUE;
        }
    }
    return FALSE;
}

gboolean graptos_formatter_copy_protected(GraptosFormatterContext *context,
                                          const char *text,
                                          gsize *index) {
    if (!context || !context->out || !text || !index || !text[*index]) return FALSE;
    gboolean preprocessor = text[*index] == '#' && at_logical_line_start(text, *index);
    if (protected_scope_enabled(context, "comment") &&
        starts_line_comment(context, text, *index)) {
        prepare_protected_copy(context);
        copy_line_comment(context->out, text, index);
        return TRUE;
    }
    if (protected_scope_enabled(context, "comment") &&
        text[*index] == '/' && text[*index + 1u] == '*') {
        prepare_protected_copy(context);
        copy_block_comment(context->out, text, index);
        return TRUE;
    }
    if (protected_scope_enabled(context, "string") && text[*index] == '"') {
        prepare_protected_copy(context);
        copy_quoted(context->out, text, index, '"');
        return TRUE;
    }
    if (protected_scope_enabled(context, "character") && text[*index] == '\'') {
        prepare_protected_copy(context);
        copy_quoted(context->out, text, index, '\'');
        return TRUE;
    }
    if (protected_scope_enabled(context, "string") && text[*index] == '`') {
        prepare_protected_copy(context);
        copy_quoted(context->out, text, index, '`');
        return TRUE;
    }
    if (protected_scope_enabled(context, "comment") && text[*index] == '#' &&
        comment_rules_mention_token(context->syntax, "#")) {
        prepare_protected_copy(context);
        copy_line_comment(context->out, text, index);
        return TRUE;
    }
    if (protected_scope_enabled(context, "preprocessor") && preprocessor) {
        copy_preprocessor(context->out, text, index);
        context->line_start = TRUE;
        context->pending_space = FALSE;
        return TRUE;
    }
    return FALSE;
}

gboolean graptos_formatter_offset_protected(const SyntaxDef *syntax,
                                            const char *text,
                                            gsize byte_offset) {
    if (!text) return FALSE;
    const char *line_comment = syntax ? syntax->line_comment : NULL;
    gboolean in_line_comment = FALSE;
    gboolean in_block_comment = FALSE;
    gboolean in_string = FALSE;
    gboolean in_character = FALSE;
    gboolean in_backtick_string = FALSE;
    gboolean in_preprocessor = FALSE;
    gboolean escaped = FALSE;
    for (gsize i = 0u; text[i] && i < byte_offset; i++) {
        char ch = text[i];
        char next = text[i + 1u];
        if (in_line_comment) {
            if (ch == '\n') in_line_comment = FALSE;
            continue;
        }
        if (in_preprocessor) {
            if (ch == '\n' && (i == 0u || text[i - 1u] != '\\')) in_preprocessor = FALSE;
            continue;
        }
        if (in_block_comment) {
            if (ch == '*' && next == '/') {
                in_block_comment = FALSE;
                i++;
            }
            continue;
        }
        if (in_string || in_character || in_backtick_string) {
            if (escaped) {
                escaped = FALSE;
            } else if (ch == '\\') {
                escaped = TRUE;
            } else if ((in_string && ch == '"') ||
                       (in_character && ch == '\'') ||
                       (in_backtick_string && ch == '`')) {
                in_string = FALSE;
                in_character = FALSE;
                in_backtick_string = FALSE;
            }
            continue;
        }
        if (ch == '#' && at_logical_line_start(text, i)) in_preprocessor = TRUE;
        else if (ch == '#' && comment_rules_mention_token(syntax, "#")) {
            in_line_comment = TRUE;
        }
        else if (line_comment && line_comment[0] != '\0' &&
                 g_str_has_prefix(text + i, line_comment)) {
            in_line_comment = TRUE;
        }
        else if (ch == '/' && next == '*') {
            in_block_comment = TRUE;
            i++;
        } else if (ch == '"') in_string = TRUE;
        else if (ch == '\'') in_character = TRUE;
        else if (ch == '`') in_backtick_string = TRUE;
    }
    return in_line_comment || in_block_comment || in_string || in_character ||
           in_backtick_string || in_preprocessor;
}
