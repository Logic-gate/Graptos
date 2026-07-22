/**
 * @file src/syntax_loader.c
 * @brief YAML-like syntax definition loader.
 * @details The syntax layer gives Graptoς useful language behavior without requiring LSP.
 *          YAML keeps the language definitions editable, while this code handles loading,
 *          highlighting, diagnostics, and safe fallbacks.
 */

#include "syntax_private.h"

#include <errno.h>
#include <string.h>

#ifndef DATADIR
/**
 * @brief Datadir macro.
 */
#define DATADIR "/usr/local/share/graptos"
#endif

/**
 * @brief Trim dup.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *trim_dup(const char *text) {
    if (!text) return g_strdup("");
    char *copy = g_strdup(text);
    if (!copy) return NULL;
    return g_strstrip(copy);
}

/**
 * @brief Unquote value.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param value The value being parsed, stored, or applied.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *unquote_value(const char *value) {
    char *s = trim_dup(value);
    if (!s) return NULL;
    gsize len = strlen(s);
    if (len >= 2u && ((s[0] == '"' && s[len - 1u] == '"') || (s[0] == '\'' && s[len - 1u] == '\''))) {
        s[len - 1u] = '\0';
        char *out = g_strdup(s + 1);
        g_free(s);
        return out;
    }
    return s;
}

/**
 * @brief Parse bool value.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param value The value being parsed, stored, or applied.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean parse_bool_value(const char *value) {
    char *s = trim_dup(value);
    if (!s) return FALSE;
    gboolean result = g_ascii_strcasecmp(s, "true") == 0 ||
                      g_ascii_strcasecmp(s, "yes") == 0 ||
                      strcmp(s, "1") == 0;
    g_free(s);
    return result;
}

/**
 * @brief Parse an unsigned integer.
 * @details Formatting adds a few numeric policy fields. Keeping their parsing
 *          in this loader keeps the syntax file contract in one place.
 * @param value The scalar value from the syntax file.
 * @return Parsed integer, or zero when parsing fails.
 */
static guint parse_uint_value(const char *value) {
    char *s = trim_dup(value);
    if (!s) return 0u;
    char *end = NULL;
    errno = 0;
    unsigned long parsed = strtoul(s, &end, 10);
    gboolean ok = errno == 0 && end != s;
    g_free(s);
    return ok ? (guint)parsed : 0u;
}

/**
 * @brief Parse a signed integer.
 * @details Case-label indentation may be zero or negative in later styles, so
 *          the parser keeps this separate from unsigned widths.
 * @param value The scalar value from the syntax file.
 * @return Parsed integer, or zero when parsing fails.
 */
static gint parse_int_value(const char *value) {
    char *s = trim_dup(value);
    if (!s) return 0;
    char *end = NULL;
    errno = 0;
    long parsed = strtol(s, &end, 10);
    gboolean ok = errno == 0 && end != s;
    g_free(s);
    return ok ? (gint)parsed : 0;
}

/**
 * @brief Split key value.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param key_out Output storage filled when the operation can provide a value.
 * @param value_out Output storage filled when the operation can provide a value.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean split_key_value(const char *line, char **key_out, char **value_out) {
    const char *colon = strchr(line, ':');
    if (!colon) return FALSE;
    char *key = g_strndup(line, (gsize)(colon - line));
    char *value = g_strdup(colon + 1);
    if (!key || !value) {
        g_free(key);
        g_free(value);
        return FALSE;
    }
    g_strstrip(key);
    g_strstrip(value);
    *key_out = key;
    *value_out = value;
    return TRUE;
}

/**
 * @brief Parse string list.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param out Output storage filled when the lookup succeeds.
 * @param value The value being parsed, stored, or applied.
 * @param dot_prefix The dot prefix supplied by the caller.
 */
static void parse_string_list(GPtrArray *out, const char *value, gboolean dot_prefix) {
    if (!out || !value) return;
    char *s = trim_dup(value);
    if (!s) return;
    if (s[0] == '[') {
        gsize len = strlen(s);
        if (len > 0u && s[len - 1u] == ']') s[len - 1u] = '\0';
        memmove(s, s + 1, strlen(s));
    }
    char **parts = g_strsplit(s, ",", -1);
    for (guint i = 0; parts && parts[i]; i++) {
        char *part = unquote_value(parts[i]);
        if (part && part[0] != '\0') {
            if (dot_prefix && part[0] != '.') {
                char *with_dot = g_strdup_printf(".%s", part);
                g_free(part);
                part = with_dot;
            }
            g_ptr_array_add(out, part);
        } else {
            g_free(part);
        }
    }
    g_strfreev(parts);
    g_free(s);
}


/**
 * @brief Syntax pair new from spec.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param spec The spec supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static SyntaxPair *syntax_pair_new_from_spec(const char *spec) {
    if (!spec) return NULL;
    const char *sep = strstr(spec, "=>");
    if (!sep) return NULL;

    char *open = g_strndup(spec, (gsize)(sep - spec));
    char *close = g_strdup(sep + 2);
    if (!open || !close) {
        g_free(open);
        g_free(close);
        return NULL;
    }
    g_strstrip(open);
    g_strstrip(close);
    if (open[0] == '\0' || close[0] == '\0') {
        g_free(open);
        g_free(close);
        return NULL;
    }

    SyntaxPair *pair = g_new0(SyntaxPair, 1);
    if (!pair) {
        g_free(open);
        g_free(close);
        return NULL;
    }
    pair->open = open;
    pair->close = close;
    return pair;
}

/**
 * @brief Parse pair list.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param out Output storage filled when the lookup succeeds.
 * @param value The value being parsed, stored, or applied.
 */
static void parse_pair_list(GPtrArray *out, const char *value) {
    if (!out || !value) return;
    GPtrArray *items = g_ptr_array_new_with_free_func(g_free);
    if (!items) return;
    parse_string_list(items, value, FALSE);
    for (guint i = 0; i < items->len; i++) {
        const char *spec = g_ptr_array_index(items, i);
        SyntaxPair *pair = syntax_pair_new_from_spec(spec);
        if (pair) g_ptr_array_add(out, pair);
    }
    g_ptr_array_free(items, TRUE);
}

/**
 * @brief Parse extensions.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntax The syntax definition used by the editor path.
 * @param value The value being parsed, stored, or applied.
 */
static void parse_extensions(SyntaxDef *syntax, const char *value) {
    if (!syntax) return;
    parse_string_list(syntax->extensions, value, TRUE);
}

/**
 * @brief Parse completions.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntax The syntax definition used by the editor path.
 * @param value The value being parsed, stored, or applied.
 */
static void parse_completions(SyntaxDef *syntax, const char *value) {
    if (!syntax) return;
    parse_string_list(syntax->completions, value, FALSE);
}

/**
 * @brief Parse filenames.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntax The syntax definition used by the editor path.
 * @param value The value being parsed, stored, or applied.
 */
static void parse_filenames(SyntaxDef *syntax, const char *value) {
    if (!syntax) return;
    parse_string_list(syntax->filenames, value, FALSE);
}

/**
 * @brief Parse rule field.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param rule The rule supplied by the caller.
 * @param key The key supplied by the caller.
 * @param value The value being parsed, stored, or applied.
 */
static void parse_rule_field(SyntaxRule *rule, const char *key, const char *value) {
    if (!rule || !key || !value) return;
    if (g_ascii_strcasecmp(key, "name") == 0) {
        g_free(rule->name);
        rule->name = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "scope") == 0) {
        g_free(rule->scope);
        rule->scope = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "pattern") == 0) {
        g_free(rule->pattern);
        rule->pattern = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "color") == 0 || g_ascii_strcasecmp(key, "foreground") == 0) {
        g_free(rule->color);
        rule->color = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "bold") == 0) {
        rule->bold = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "italic") == 0) {
        rule->italic = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "underline") == 0) {
        rule->underline = parse_bool_value(value);
    }
}

/**
 * @brief Compile syntax rule.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param rule The rule supplied by the caller.
 * @param filename The filename supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean compile_syntax_rule(SyntaxRule *rule, const char *filename) {
    if (!rule || !rule->pattern || rule->pattern[0] == '\0') return FALSE;
    g_autoptr(GError) error = NULL;
    rule->regex = g_regex_new(rule->pattern, G_REGEX_MULTILINE | G_REGEX_OPTIMIZE, 0, &error);
    if (!rule->regex) {
        g_warning("Invalid regex in %s (%s): %s", filename ? filename : "syntax file",
                  rule->name ? rule->name : "unnamed", error ? error->message : "unknown error");
        return FALSE;
    }
    if (!rule->name) rule->name = g_strdup("rule");
    if (!rule->color) rule->color = g_strdup("#d4d4d4");
    return TRUE;
}


/**
 * @brief Parse rule line.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntax The syntax definition used by the editor path.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param current_rule The current rule supplied by the caller.
 */
static void parse_rule_line(SyntaxDef *syntax, char *line,
                            SyntaxRule **current_rule) {
    if (g_str_has_prefix(line, "-")) {
        *current_rule = g_new0(SyntaxRule, 1);
        if (*current_rule) g_ptr_array_add(syntax->rules, *current_rule);
        line = g_strstrip(line + 1);
    }
    if (!*current_rule || line[0] == '\0') return;

    char *key = NULL;
    char *value = NULL;
    if (split_key_value(line, &key, &value)) {
        parse_rule_field(*current_rule, key, value);
    }
    g_free(key);
    g_free(value);
}

/**
 * @brief Ensure a syntax owns a formatter policy.
 * @details Missing formatting sections must remain disabled. The policy is
 *          allocated only when a syntax file explicitly enters formatting.
 * @param syntax The syntax definition being parsed.
 * @return The owned formatter policy, or NULL on allocation failure.
 */
static SyntaxFormatting *ensure_formatting(SyntaxDef *syntax) {
    if (!syntax) return NULL;
    if (!syntax->formatting) {
        syntax->formatting = g_new0(SyntaxFormatting, 1);
        if (syntax->formatting) {
            syntax->formatting->scope = g_strdup("selection_or_block");
            syntax->formatting->profile = g_strdup("brace_semicolon");
            syntax->formatting->block_style = g_strdup("braces");
            syntax->formatting->statement_style = g_strdup("semicolon");
            syntax->formatting->brace_style = g_strdup("attach");
            syntax->formatting->pointer_alignment = g_strdup("name");
            syntax->formatting->logical_operators = g_ptr_array_new_with_free_func(g_free);
            syntax->formatting->binary_operators = g_ptr_array_new_with_free_func(g_free);
            syntax->formatting->unary_prefix_operators = g_ptr_array_new_with_free_func(g_free);
            syntax->formatting->protected_scopes = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(syntax->formatting->logical_operators, g_strdup("&&"));
            g_ptr_array_add(syntax->formatting->logical_operators, g_strdup("||"));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("+"));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("-"));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("*"));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("/"));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("%"));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("="));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("=="));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("!="));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("<"));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup(">"));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup("<="));
            g_ptr_array_add(syntax->formatting->binary_operators, g_strdup(">="));
            g_ptr_array_add(syntax->formatting->unary_prefix_operators, g_strdup("!"));
            g_ptr_array_add(syntax->formatting->protected_scopes, g_strdup("comment"));
            g_ptr_array_add(syntax->formatting->protected_scopes, g_strdup("string"));
            g_ptr_array_add(syntax->formatting->protected_scopes, g_strdup("character"));
            g_ptr_array_add(syntax->formatting->protected_scopes, g_strdup("preprocessor"));
            syntax->formatting->preserve_blank_lines = TRUE;
            syntax->formatting->max_blank_lines = 1u;
        }
    }
    return syntax->formatting;
}

/**
 * @brief Replace one formatter string list from YAML.
 * @details Formatting lists are policy, not structure. Clearing first keeps
 *          explicit YAML declarations authoritative over built-in defaults.
 * @param list Destination string array.
 * @param value YAML inline list value.
 */
static void parse_formatting_string_list(GPtrArray *list, const char *value) {
    if (!list || !value) return;
    g_ptr_array_set_size(list, 0u);
    parse_string_list(list, value, FALSE);
}

/**
 * @brief Return whether a key belongs to the formatting section.
 * @details Syntax files are parsed after whitespace trimming, so this guard
 *          lets top-level keys end a formatting section instead of being
 *          mistaken for unknown formatter fields.
 * @param key Candidate YAML key.
 * @return TRUE for known formatter keys.
 */
static gboolean formatting_key_known(const char *key) {
    if (!key) return FALSE;
    return g_ascii_strcasecmp(key, "enabled") == 0 ||
           g_ascii_strcasecmp(key, "scope") == 0 ||
           g_ascii_strcasecmp(key, "profile") == 0 ||
           g_ascii_strcasecmp(key, "block_style") == 0 ||
           g_ascii_strcasecmp(key, "statement_style") == 0 ||
           g_ascii_strcasecmp(key, "brace_style") == 0 ||
           g_ascii_strcasecmp(key, "one_statement_per_line") == 0 ||
           g_ascii_strcasecmp(key, "space_before_block_opener") == 0 ||
           g_ascii_strcasecmp(key, "space_after_comma") == 0 ||
           g_ascii_strcasecmp(key, "space_after_control_keyword") == 0 ||
           g_ascii_strcasecmp(key, "space_around_binary_operators") == 0 ||
           g_ascii_strcasecmp(key, "space_around_logical_operators") == 0 ||
           g_ascii_strcasecmp(key, "pointer_alignment") == 0 ||
           g_ascii_strcasecmp(key, "continuation_indent") == 0 ||
           g_ascii_strcasecmp(key, "max_column") == 0 ||
           g_ascii_strcasecmp(key, "column_limit") == 0 ||
           g_ascii_strcasecmp(key, "max_blank_lines") == 0 ||
           g_ascii_strcasecmp(key, "case_indent") == 0 ||
           g_ascii_strcasecmp(key, "logical_operators") == 0 ||
           g_ascii_strcasecmp(key, "binary_operators") == 0 ||
           g_ascii_strcasecmp(key, "unary_prefix_operators") == 0 ||
           g_ascii_strcasecmp(key, "protected_scopes") == 0 ||
           g_ascii_strcasecmp(key, "preserve_blank_lines") == 0 ||
           g_ascii_strcasecmp(key, "wrap_after_comma") == 0 ||
           g_ascii_strcasecmp(key, "collapse_empty_blocks") == 0;
}

/**
 * @brief Parse one formatting field.
 * @details The formatting section describes layout preferences only. Structural
 *          tokens stay in the existing top-level syntax fields.
 * @param syntax The syntax definition being parsed.
 * @param key Formatting field name.
 * @param value Formatting field value.
 */
static void parse_formatting_field(SyntaxDef *syntax,
                                   const char *key,
                                   const char *value) {
    SyntaxFormatting *formatting = ensure_formatting(syntax);
    if (!formatting || !key || !value) return;
    if (g_ascii_strcasecmp(key, "enabled") == 0) {
        formatting->enabled = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "scope") == 0) {
        g_free(formatting->scope);
        formatting->scope = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "profile") == 0) {
        g_free(formatting->profile);
        formatting->profile = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "block_style") == 0) {
        g_free(formatting->block_style);
        formatting->block_style = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "statement_style") == 0) {
        g_free(formatting->statement_style);
        formatting->statement_style = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "brace_style") == 0) {
        g_free(formatting->brace_style);
        formatting->brace_style = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "one_statement_per_line") == 0) {
        formatting->one_statement_per_line = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "space_before_block_opener") == 0) {
        formatting->space_before_block_opener = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "space_after_comma") == 0) {
        formatting->space_after_comma = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "space_after_control_keyword") == 0) {
        formatting->space_after_control_keyword = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "space_around_binary_operators") == 0) {
        formatting->space_around_binary_operators = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "space_around_logical_operators") == 0) {
        formatting->space_around_logical_operators = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "pointer_alignment") == 0) {
        g_free(formatting->pointer_alignment);
        formatting->pointer_alignment = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "continuation_indent") == 0) {
        formatting->continuation_indent = parse_uint_value(value);
    } else if (g_ascii_strcasecmp(key, "max_column") == 0 ||
               g_ascii_strcasecmp(key, "column_limit") == 0) {
        formatting->max_column = parse_uint_value(value);
    } else if (g_ascii_strcasecmp(key, "max_blank_lines") == 0) {
        formatting->max_blank_lines = parse_uint_value(value);
    } else if (g_ascii_strcasecmp(key, "case_indent") == 0) {
        formatting->case_indent = parse_int_value(value);
    } else if (g_ascii_strcasecmp(key, "logical_operators") == 0) {
        parse_formatting_string_list(formatting->logical_operators, value);
    } else if (g_ascii_strcasecmp(key, "binary_operators") == 0) {
        parse_formatting_string_list(formatting->binary_operators, value);
    } else if (g_ascii_strcasecmp(key, "unary_prefix_operators") == 0) {
        parse_formatting_string_list(formatting->unary_prefix_operators, value);
    } else if (g_ascii_strcasecmp(key, "protected_scopes") == 0) {
        parse_formatting_string_list(formatting->protected_scopes, value);
    } else if (g_ascii_strcasecmp(key, "preserve_blank_lines") == 0) {
        formatting->preserve_blank_lines = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "wrap_after_comma") == 0) {
        formatting->wrap_after_comma = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "collapse_empty_blocks") == 0) {
        formatting->collapse_empty_blocks = parse_bool_value(value);
    } else {
        g_warning("Unknown formatting field '%s' in syntax %s",
                  key,
                  syntax && syntax->name ? syntax->name : "(unnamed)");
    }
}

/**
 * @brief Validate formatter enum fields.
 * @details Invalid layout names are safer as disabled formatting than as silent
 *          fallbacks, because formatting modifies user text.
 * @param syntax The syntax definition to validate.
 * @param path Syntax file path used in warnings.
 */
static void validate_formatting(SyntaxDef *syntax, const char *path) {
    SyntaxFormatting *f = syntax ? syntax->formatting : NULL;
    if (!f) return;
    gboolean valid = TRUE;
    if (g_strcmp0(f->scope, "selection_or_block") != 0) valid = FALSE;
    if (g_strcmp0(f->profile, "brace_semicolon") != 0 &&
        g_strcmp0(f->profile, "brace_optional_semicolon") != 0 &&
        g_strcmp0(f->profile, "indent_colon") != 0 &&
        g_strcmp0(f->profile, "markup") != 0 &&
        g_strcmp0(f->profile, "data") != 0 &&
        g_strcmp0(f->profile, "plain") != 0) {
        valid = FALSE;
    }
    if (g_strcmp0(f->block_style, "braces") != 0 &&
        g_strcmp0(f->block_style, "indent") != 0 &&
        g_strcmp0(f->block_style, "tags") != 0 &&
        g_strcmp0(f->block_style, "none") != 0) {
        valid = FALSE;
    }
    if (g_strcmp0(f->statement_style, "semicolon") != 0 &&
        g_strcmp0(f->statement_style, "optional_semicolon") != 0 &&
        g_strcmp0(f->statement_style, "newline") != 0 &&
        g_strcmp0(f->statement_style, "none") != 0) {
        valid = FALSE;
    }
    if (g_strcmp0(f->brace_style, "attach") != 0 &&
        g_strcmp0(f->brace_style, "allman") != 0 &&
        g_strcmp0(f->brace_style, "stroustrup") != 0) {
        valid = FALSE;
    }
    if (g_strcmp0(f->pointer_alignment, "name") != 0 &&
        g_strcmp0(f->pointer_alignment, "type") != 0 &&
        g_strcmp0(f->pointer_alignment, "middle") != 0) {
        valid = FALSE;
    }
    if (!valid) {
        g_warning("Invalid formatting enum in %s; formatting disabled",
                  path ? path : "syntax file");
        f->enabled = FALSE;
    }
}

/**
 * @brief Parse top level field.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntax The syntax definition used by the editor path.
 * @param key The key supplied by the caller.
 * @param value The value being parsed, stored, or applied.
 */
static void parse_top_level_field(SyntaxDef *syntax,
                                  const char *key,
                                  const char *value) {
    if (g_ascii_strcasecmp(key, "name") == 0) {
        g_free(syntax->name);
        syntax->name = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "extensions") == 0) {
        parse_extensions(syntax, value);
    } else if (g_ascii_strcasecmp(key, "filenames") == 0 ||
               g_ascii_strcasecmp(key, "file_names") == 0) {
        parse_filenames(syntax, value);
    } else if (g_ascii_strcasecmp(key, "icon") == 0 ||
               g_ascii_strcasecmp(key, "badge") == 0) {
        g_free(syntax->icon);
        syntax->icon = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "import_style") == 0 ||
               g_ascii_strcasecmp(key, "import") == 0 ||
               g_ascii_strcasecmp(key, "imports") == 0) {
        g_free(syntax->import_style);
        syntax->import_style = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "index") == 0 ||
               g_ascii_strcasecmp(key, "indexable") == 0) {
        syntax->index_enabled = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "completions") == 0 ||
               g_ascii_strcasecmp(key, "keywords") == 0) {
        parse_completions(syntax, value);
    } else if (g_ascii_strcasecmp(key, "import_keywords") == 0) {
        parse_string_list(syntax->import_keywords, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "import_from_keywords") == 0) {
        parse_string_list(syntax->import_from_keywords, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "import_member_keywords") == 0) {
        parse_string_list(syntax->import_member_keywords, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "import_roots") == 0) {
        parse_string_list(syntax->import_roots, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "import_env") == 0 ||
               g_ascii_strcasecmp(key, "import_env_vars") == 0) {
        parse_string_list(syntax->import_env, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "import_extensions") == 0) {
        parse_string_list(syntax->import_extensions, value, TRUE);
    } else if (g_ascii_strcasecmp(key, "import_member_files") == 0) {
        parse_string_list(syntax->import_member_files, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "import_static_modules") == 0) {
        parse_string_list(syntax->import_static_modules, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "lsp_command") == 0) {
        g_free(syntax->lsp_command);
        syntax->lsp_command = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "lsp_args") == 0) {
        parse_string_list(syntax->lsp_args, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "lsp_language_id") == 0 ||
               g_ascii_strcasecmp(key, "lsp_language") == 0) {
        g_free(syntax->lsp_language_id);
        syntax->lsp_language_id = unquote_value(value);
    } else if (g_ascii_strcasecmp(key, "import_strip_extensions") == 0) {
        syntax->import_strip_extensions = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "import_dot_modules") == 0) {
        syntax->import_dot_modules = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "close_pairs") == 0 ||
               g_ascii_strcasecmp(key, "closing_pairs") == 0 ||
               g_ascii_strcasecmp(key, "balanced_pairs") == 0) {
        parse_pair_list(syntax->close_pairs, value);
    } else if (g_ascii_strcasecmp(key, "line_close_pairs") == 0 ||
               g_ascii_strcasecmp(key, "line_closing_pairs") == 0) {
        parse_pair_list(syntax->line_close_pairs, value);
    } else if (g_ascii_strcasecmp(key, "statement_required_enders") == 0 ||
               g_ascii_strcasecmp(key, "statement_enders") == 0) {
        parse_string_list(syntax->statement_required_enders, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "statement_exempt_prefixes") == 0) {
        parse_string_list(syntax->statement_exempt_prefixes, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "statement_exempt_suffixes") == 0) {
        parse_string_list(syntax->statement_exempt_suffixes, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "indent_openers") == 0) {
        parse_string_list(syntax->indent_openers, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "indent_closers") == 0) {
        parse_string_list(syntax->indent_closers, value, FALSE);
    } else if (g_ascii_strcasecmp(key, "auto_indent") == 0) {
        syntax->auto_indent = parse_bool_value(value);
    } else if (g_ascii_strcasecmp(key, "line_comment") == 0) {
        g_free(syntax->line_comment);
        syntax->line_comment = unquote_value(value);
    }
}

/**
 * @brief Top level key accepts block list.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param key The key supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean top_level_key_accepts_block_list(const char *key) {
    if (!key) return FALSE;
    return g_ascii_strcasecmp(key, "extensions") == 0 ||
           g_ascii_strcasecmp(key, "filenames") == 0 ||
           g_ascii_strcasecmp(key, "file_names") == 0 ||
           g_ascii_strcasecmp(key, "completions") == 0 ||
           g_ascii_strcasecmp(key, "keywords") == 0 ||
           g_ascii_strcasecmp(key, "import_keywords") == 0 ||
           g_ascii_strcasecmp(key, "import_from_keywords") == 0 ||
           g_ascii_strcasecmp(key, "import_member_keywords") == 0 ||
           g_ascii_strcasecmp(key, "import_roots") == 0 ||
           g_ascii_strcasecmp(key, "import_env") == 0 ||
           g_ascii_strcasecmp(key, "import_env_vars") == 0 ||
           g_ascii_strcasecmp(key, "import_extensions") == 0 ||
           g_ascii_strcasecmp(key, "import_member_files") == 0 ||
           g_ascii_strcasecmp(key, "import_static_modules") == 0 ||
           g_ascii_strcasecmp(key, "lsp_args") == 0 ||
           g_ascii_strcasecmp(key, "close_pairs") == 0 ||
           g_ascii_strcasecmp(key, "closing_pairs") == 0 ||
           g_ascii_strcasecmp(key, "balanced_pairs") == 0 ||
           g_ascii_strcasecmp(key, "line_close_pairs") == 0 ||
           g_ascii_strcasecmp(key, "line_closing_pairs") == 0 ||
           g_ascii_strcasecmp(key, "statement_required_enders") == 0 ||
           g_ascii_strcasecmp(key, "statement_enders") == 0 ||
           g_ascii_strcasecmp(key, "statement_exempt_prefixes") == 0 ||
           g_ascii_strcasecmp(key, "statement_exempt_suffixes") == 0 ||
           g_ascii_strcasecmp(key, "indent_openers") == 0 ||
           g_ascii_strcasecmp(key, "indent_closers") == 0;
}

/**
 * @brief Parse top level line.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntax The syntax definition used by the editor path.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param block_list_key The block list key supplied by the caller.
 */
static void parse_top_level_line(SyntaxDef *syntax, const char *line,
                                 char **block_list_key) {
    char *key = NULL;
    char *value = NULL;
    if (!split_key_value(line, &key, &value)) return;

    if (block_list_key) {
        g_clear_pointer(block_list_key, g_free);
        if (value[0] == '\0' && top_level_key_accepts_block_list(key)) {
            *block_list_key = g_strdup(key);
        }
    }

    if (value[0] != '\0') {
        parse_top_level_field(syntax, key, value);
    }
    g_free(key);
    g_free(value);
}

/**
 * @brief Parse syntax line.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntax The syntax definition used by the editor path.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param in_rules The in rules supplied by the caller.
 * @param current_rule The current rule supplied by the caller.
 * @param block_list_key The block list key supplied by the caller.
 */
static void parse_syntax_line(SyntaxDef *syntax, char *line,
                              gboolean *in_rules,
                              SyntaxRule **current_rule,
                              gboolean *in_formatting,
                              char **block_list_key) {
    if (g_str_has_prefix(line, "rules:")) {
        if (block_list_key) g_clear_pointer(block_list_key, g_free);
        if (in_formatting) *in_formatting = FALSE;
        *in_rules = TRUE;
        return;
    }

    if (!*in_rules && g_str_has_prefix(line, "formatting:")) {
        if (block_list_key) g_clear_pointer(block_list_key, g_free);
        if (in_formatting) *in_formatting = TRUE;
        ensure_formatting(syntax);
        return;
    }

    if (!*in_rules && in_formatting && *in_formatting) {
        char *key = NULL;
        char *value = NULL;
        if (split_key_value(line, &key, &value)) {
            if (formatting_key_known(key)) {
                parse_formatting_field(syntax, key, value);
                g_free(key);
                g_free(value);
                return;
            }
            *in_formatting = FALSE;
        }
        g_free(key);
        g_free(value);
    }

    if (!*in_rules && block_list_key && *block_list_key && g_str_has_prefix(line, "-")) {
        char *item = g_strdup(line + 1);
        if (item) {
            g_strstrip(item);
            if (item[0] != '\0') parse_top_level_field(syntax, *block_list_key, item);
            g_free(item);
        }
        return;
    }

    if (*in_rules && g_str_has_prefix(line, "-")) {
        parse_rule_line(syntax, line, current_rule);
        return;
    }
    if (*in_rules && *current_rule && strchr(line, ':') &&
        (g_str_has_prefix(line, "name:") || g_str_has_prefix(line, "pattern:") ||
         g_str_has_prefix(line, "scope:") ||
         g_str_has_prefix(line, "color:") || g_str_has_prefix(line, "foreground:") ||
         g_str_has_prefix(line, "bold:") || g_str_has_prefix(line, "italic:") ||
         g_str_has_prefix(line, "underline:"))) {
        parse_rule_line(syntax, line, current_rule);
        return;
    }

    if (block_list_key) g_clear_pointer(block_list_key, g_free);
    parse_top_level_line(syntax, line, block_list_key);
}

/**
 * @brief Compile syntax rules.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntax The syntax definition used by the editor path.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean compile_syntax_rules(SyntaxDef *syntax, const char *path) {
    if (!syntax->name || !syntax->rules || syntax->rules->len == 0u) {
        return FALSE;
    }
    for (gint i = (gint)syntax->rules->len - 1; i >= 0; i--) {
        SyntaxRule *rule = g_ptr_array_index(syntax->rules, (guint)i);
        if (!compile_syntax_rule(rule, path)) {
            g_ptr_array_remove_index(syntax->rules, (guint)i);
        }
    }
    return syntax->rules->len > 0u;
}

/**
 * @brief Load syntax file.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static SyntaxDef *load_syntax_file(const char *path) {
    g_autofree char *contents = NULL;
    gsize length = 0u;
    g_autoptr(GError) error = NULL;
    if (!g_file_get_contents(path, &contents, &length, &error)) {
        g_warning("Could not read syntax file %s: %s", path,
                  error ? error->message : "unknown error");
        return NULL;
    }

    SyntaxDef *syntax = syntax_def_new_default();
    gboolean in_rules = FALSE;
    gboolean in_formatting = FALSE;
    SyntaxRule *current_rule = NULL;
    g_autofree char *block_list_key = NULL;
    g_auto(GStrv) lines = g_strsplit(contents, "\n", -1);
    for (guint i = 0u; syntax && lines && lines[i]; i++) {
        g_autofree char *raw = g_strdup(lines[i]);
        if (!raw) continue;
        g_strchomp(raw);
        char *line = g_strstrip(raw);
        if (line[0] != '\0' && line[0] != '#') {
            parse_syntax_line(syntax, line, &in_rules, &current_rule, &in_formatting, &block_list_key);
        }
    }

    if (!syntax) return NULL;
    validate_formatting(syntax, path);

    if (!compile_syntax_rules(syntax, path)) {
        syntax_def_free(syntax);
        return NULL;
    }
    return syntax;
}


/**
 * @brief Syntax name compare.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param a Pointer to the first syntax definition pointer.
 * @param b Pointer to the second syntax definition pointer.
 * @return The computed value requested by the caller.
 */
static gint syntax_name_compare(gconstpointer a, gconstpointer b) {
    const SyntaxDef *sa = *(SyntaxDef * const *)a;
    const SyntaxDef *sb = *(SyntaxDef * const *)b;
    return g_ascii_strcasecmp(sa && sa->name ? sa->name : "", sb && sb->name ? sb->name : "");
}

/**
 * @brief Load syntax dir.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param dir_path The dir path supplied by the caller.
 */
static void load_syntax_dir(GPtrArray *syntaxes, const char *dir_path) {
    if (!syntaxes || !dir_path) return;
    GDir *dir = g_dir_open(dir_path, 0, NULL);
    if (!dir) return;
    const char *name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (!g_str_has_suffix(name, ".yaml") && !g_str_has_suffix(name, ".yml")) continue;
        char *path = g_build_filename(dir_path, name, NULL);
        SyntaxDef *syntax = load_syntax_file(path);
        if (syntax) g_ptr_array_add(syntaxes, syntax);
        g_free(path);
    }
    g_dir_close(dir);
}

/**
 * @brief Load syntax dirs for a project root.
 * @details Project syntax files are allowed in both `syntax/` and
 *          `.graptos/syntax/`. The first form is convenient for language packs;
 *          the hidden form is better for project-local editor tweaks.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param project_root The project root supplied by the caller.
 */
static void load_project_syntax_dirs(GPtrArray *syntaxes,
                                     const char *project_root) {
    if (!syntaxes || !project_root || project_root[0] == '\0') return;

    char *root_syntax = g_build_filename(project_root, "syntax", NULL);
    char *root_graptos_syntax = g_build_filename(project_root, ".graptos",
                                               "syntax", NULL);
    load_syntax_dir(syntaxes, root_syntax);
    load_syntax_dir(syntaxes, root_graptos_syntax);
    g_free(root_syntax);
    g_free(root_graptos_syntax);
}

/**
 * @brief Remove duplicate syntax definitions by name.
 * @details Syntax directories are loaded in priority order. Removing later
 *          duplicates lets user and project definitions override installed
 *          defaults without adding a separate precedence system.
 * @param syntaxes The syntaxes supplied by the caller.
 */
static void remove_duplicate_syntaxes(GPtrArray *syntaxes) {
    if (!syntaxes) return;

    for (gint i = (gint)syntaxes->len - 1; i >= 0; i--) {
        SyntaxDef *candidate = g_ptr_array_index(syntaxes, (guint)i);
        gboolean duplicate = FALSE;
        for (guint j = 0; j < (guint)i; j++) {
            SyntaxDef *existing = g_ptr_array_index(syntaxes, j);
            if (candidate && existing && candidate->name && existing->name &&
                g_ascii_strcasecmp(candidate->name, existing->name) == 0) {
                duplicate = TRUE;
                break;
            }
        }
        if (duplicate) g_ptr_array_remove_index(syntaxes, (guint)i);
    }
}

/**
 * @brief Syntax load all for project roots.
 * @details Loading checks config, project, development, and installed syntax
 *          locations in one pass. That makes local development and installed
 *          builds behave the same, with earlier paths winning on duplicates.
 * @param project_roots The project roots supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *syntax_load_all_for_roots(GPtrArray *project_roots) {
    GPtrArray *syntaxes = g_ptr_array_new_with_free_func(syntax_def_free);
    if (!syntaxes) return NULL;

    char *cwd = g_get_current_dir();
    char *config_syntax = g_build_filename(g_get_user_config_dir(), "graptos", "syntax", NULL);
    char *config_root = g_build_filename(g_get_user_config_dir(), "graptos", NULL);
    char *user_data = g_build_filename(g_get_user_data_dir(), "graptos", "syntax", NULL);
    char *local = g_build_filename(cwd, "syntax", NULL);
    char *parent = g_path_get_dirname(cwd);
    char *parent_syntax = g_build_filename(parent, "syntax", NULL);
    char *system = g_build_filename(DATADIR, "syntax", NULL);

    /*
     * Earlier directories win when duplicate language names are removed below.
     * User config remains highest priority, then open project roots, then
     * process-local development folders, then installed system syntaxes.
     */
    load_syntax_dir(syntaxes, config_syntax);
    load_syntax_dir(syntaxes, config_root);
    load_syntax_dir(syntaxes, user_data);
    if (project_roots) {
        for (guint i = 0u; i < project_roots->len; i++) {
            const char *root = g_ptr_array_index(project_roots, i);
            load_project_syntax_dirs(syntaxes, root);
        }
    }
    load_syntax_dir(syntaxes, local);
    load_syntax_dir(syntaxes, parent_syntax);
    load_syntax_dir(syntaxes, system);

    g_free(cwd);
    g_free(config_syntax);
    g_free(config_root);
    g_free(user_data);
    g_free(local);
    g_free(parent);
    g_free(parent_syntax);
    g_free(system);

    remove_duplicate_syntaxes(syntaxes);
    g_ptr_array_sort(syntaxes, syntax_name_compare);
    return syntaxes;
}

/**
 * @brief Syntax load all.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *syntax_load_all(void) {
    return syntax_load_all_for_roots(NULL);
}
