/**
 * @file src/syntax_loader.c
 * @brief YAML-like syntax definition loader.
 */

#include "syntax_private.h"

#include <errno.h>
#include <string.h>

#ifndef DATADIR
/**
 * @brief Datadir macro.
 */
#define DATADIR "/usr/local/share/cleaf"
#endif

/**
 * @brief Trim dup.
 */
static char *trim_dup(const char *text) {
    if (!text) return g_strdup("");
    char *copy = g_strdup(text);
    if (!copy) return NULL;
    return g_strstrip(copy);
}

/**
 * @brief Unquote value.
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
 * @brief Split key value.
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
 */
static void parse_extensions(SyntaxDef *syntax, const char *value) {
    if (!syntax) return;
    parse_string_list(syntax->extensions, value, TRUE);
}

/**
 * @brief Parse completions.
 */
static void parse_completions(SyntaxDef *syntax, const char *value) {
    if (!syntax) return;
    parse_string_list(syntax->completions, value, FALSE);
}

/**
 * @brief Parse filenames.
 */
static void parse_filenames(SyntaxDef *syntax, const char *value) {
    if (!syntax) return;
    parse_string_list(syntax->filenames, value, FALSE);
}

/**
 * @brief Parse rule field.
 */
static void parse_rule_field(SyntaxRule *rule, const char *key, const char *value) {
    if (!rule || !key || !value) return;
    if (g_ascii_strcasecmp(key, "name") == 0) {
        g_free(rule->name);
        rule->name = unquote_value(value);
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
 */
static gboolean compile_syntax_rule(SyntaxRule *rule, const char *filename) {
    if (!rule || !rule->pattern || rule->pattern[0] == '\0') return FALSE;
    GError *error = NULL;
    rule->regex = g_regex_new(rule->pattern, G_REGEX_MULTILINE | G_REGEX_OPTIMIZE, 0, &error);
    if (!rule->regex) {
        g_warning("Invalid regex in %s (%s): %s", filename ? filename : "syntax file",
                  rule->name ? rule->name : "unnamed", error ? error->message : "unknown error");
        g_clear_error(&error);
        return FALSE;
    }
    if (!rule->name) rule->name = g_strdup("rule");
    if (!rule->color) rule->color = g_strdup("#d4d4d4");
    return TRUE;
}


/**
 * @brief Parse rule line.
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
 * @brief Parse top level field.
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
 */
static void parse_syntax_line(SyntaxDef *syntax, char *line,
                              gboolean *in_rules,
                              SyntaxRule **current_rule,
                              char **block_list_key) {
    if (g_str_has_prefix(line, "rules:")) {
        if (block_list_key) g_clear_pointer(block_list_key, g_free);
        *in_rules = TRUE;
        return;
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
 */
static SyntaxDef *load_syntax_file(const char *path) {
    char *contents = NULL;
    gsize length = 0u;
    GError *error = NULL;
    if (!g_file_get_contents(path, &contents, &length, &error)) {
        g_warning("Could not read syntax file %s: %s", path,
                  error ? error->message : "unknown error");
        g_clear_error(&error);
        return NULL;
    }

    SyntaxDef *syntax = syntax_def_new_default();
    gboolean in_rules = FALSE;
    SyntaxRule *current_rule = NULL;
    char *block_list_key = NULL;
    char **lines = g_strsplit(contents, "\n", -1);
    for (guint i = 0u; syntax && lines && lines[i]; i++) {
        char *raw = g_strdup(lines[i]);
        if (!raw) continue;
        g_strchomp(raw);
        char *line = g_strstrip(raw);
        if (line[0] != '\0' && line[0] != '#') {
            parse_syntax_line(syntax, line, &in_rules, &current_rule, &block_list_key);
        }
        g_free(raw);
    }

    g_clear_pointer(&block_list_key, g_free);
    g_strfreev(lines);
    g_free(contents);
    if (!syntax) return NULL;

    if (!compile_syntax_rules(syntax, path)) {
        syntax_def_free(syntax);
        return NULL;
    }
    return syntax;
}


/**
 * @brief Syntax name compare.
 */
static gint syntax_name_compare(gconstpointer a, gconstpointer b) {
    const SyntaxDef *sa = *(SyntaxDef * const *)a;
    const SyntaxDef *sb = *(SyntaxDef * const *)b;
    return g_ascii_strcasecmp(sa && sa->name ? sa->name : "", sb && sb->name ? sb->name : "");
}

/**
 * @brief Load syntax dir.
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
 * @brief Syntax load all.
 */
GPtrArray *syntax_load_all(void) {
    GPtrArray *syntaxes = g_ptr_array_new_with_free_func(syntax_def_free);
    if (!syntaxes) return NULL;

    char *cwd = g_get_current_dir();
    char *config_syntax = g_build_filename(g_get_user_config_dir(), "cleaf", "syntax", NULL);
    char *config_root = g_build_filename(g_get_user_config_dir(), "cleaf", NULL);
    char *user_data = g_build_filename(g_get_user_data_dir(), "cleaf", "syntax", NULL);
    char *local = g_build_filename(cwd, "syntax", NULL);
    char *parent = g_path_get_dirname(cwd);
    char *parent_syntax = g_build_filename(parent, "syntax", NULL);
    char *system = g_build_filename(DATADIR, "syntax", NULL);

    load_syntax_dir(syntaxes, config_syntax);
    load_syntax_dir(syntaxes, config_root);
    load_syntax_dir(syntaxes, user_data);
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
    g_ptr_array_sort(syntaxes, syntax_name_compare);
    return syntaxes;
}
