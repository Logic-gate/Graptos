/**
 * @file src/syntax.c
 * @brief Syntax definition model and lookup helpers.
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
 * @brief Syntax pair free.
 */
void syntax_pair_free(gpointer data) {
    SyntaxPair *pair = data;
    if (!pair) return;
    g_free(pair->open);
    g_free(pair->close);
    g_free(pair);
}

/**
 * @brief Syntax rule free.
 */
void syntax_rule_free(gpointer data) {
    SyntaxRule *rule = data;
    if (!rule) return;
    g_free(rule->name);
    g_free(rule->pattern);
    g_free(rule->color);
    if (rule->regex) g_regex_unref(rule->regex);
    g_free(rule);
}

/**
 * @brief Syntax def free.
 */
void syntax_def_free(gpointer data) {
    SyntaxDef *syntax = data;
    if (!syntax) return;
    g_free(syntax->name);
    g_free(syntax->line_comment);
    g_free(syntax->icon);
    g_free(syntax->import_style);
    if (syntax->import_keywords) g_ptr_array_free(syntax->import_keywords, TRUE);
    if (syntax->import_from_keywords) g_ptr_array_free(syntax->import_from_keywords, TRUE);
    if (syntax->import_member_keywords) g_ptr_array_free(syntax->import_member_keywords, TRUE);
    if (syntax->import_roots) g_ptr_array_free(syntax->import_roots, TRUE);
    if (syntax->import_env) g_ptr_array_free(syntax->import_env, TRUE);
    if (syntax->import_extensions) g_ptr_array_free(syntax->import_extensions, TRUE);
    if (syntax->import_member_files) g_ptr_array_free(syntax->import_member_files, TRUE);
    if (syntax->import_static_modules) g_ptr_array_free(syntax->import_static_modules, TRUE);
    if (syntax->close_pairs) g_ptr_array_free(syntax->close_pairs, TRUE);
    if (syntax->line_close_pairs) g_ptr_array_free(syntax->line_close_pairs, TRUE);
    if (syntax->statement_required_enders) g_ptr_array_free(syntax->statement_required_enders, TRUE);
    if (syntax->statement_exempt_prefixes) g_ptr_array_free(syntax->statement_exempt_prefixes, TRUE);
    if (syntax->statement_exempt_suffixes) g_ptr_array_free(syntax->statement_exempt_suffixes, TRUE);
    if (syntax->indent_openers) g_ptr_array_free(syntax->indent_openers, TRUE);
    if (syntax->indent_closers) g_ptr_array_free(syntax->indent_closers, TRUE);
    if (syntax->extensions) g_ptr_array_free(syntax->extensions, TRUE);
    if (syntax->filenames) g_ptr_array_free(syntax->filenames, TRUE);
    if (syntax->completions) g_ptr_array_free(syntax->completions, TRUE);
    if (syntax->rules) g_ptr_array_free(syntax->rules, TRUE);
    g_free(syntax);
}


/**
 * @brief Syntax def new default.
 */
SyntaxDef *syntax_def_new_default(void) {
    SyntaxDef *syntax = g_new0(SyntaxDef, 1);
    if (!syntax) return NULL;

    syntax->extensions = g_ptr_array_new_with_free_func(g_free);
    syntax->filenames = g_ptr_array_new_with_free_func(g_free);
    syntax->completions = g_ptr_array_new_with_free_func(g_free);
    syntax->import_keywords = g_ptr_array_new_with_free_func(g_free);
    syntax->import_from_keywords = g_ptr_array_new_with_free_func(g_free);
    syntax->import_member_keywords = g_ptr_array_new_with_free_func(g_free);
    syntax->import_roots = g_ptr_array_new_with_free_func(g_free);
    syntax->import_env = g_ptr_array_new_with_free_func(g_free);
    syntax->import_extensions = g_ptr_array_new_with_free_func(g_free);
    syntax->import_member_files = g_ptr_array_new_with_free_func(g_free);
    syntax->import_static_modules = g_ptr_array_new_with_free_func(g_free);
    syntax->rules = g_ptr_array_new_with_free_func(syntax_rule_free);
    syntax->close_pairs = g_ptr_array_new_with_free_func(syntax_pair_free);
    syntax->line_close_pairs = g_ptr_array_new_with_free_func(syntax_pair_free);
    syntax->statement_required_enders = g_ptr_array_new_with_free_func(g_free);
    syntax->statement_exempt_prefixes = g_ptr_array_new_with_free_func(g_free);
    syntax->statement_exempt_suffixes = g_ptr_array_new_with_free_func(g_free);
    syntax->indent_openers = g_ptr_array_new_with_free_func(g_free);
    syntax->indent_closers = g_ptr_array_new_with_free_func(g_free);
    syntax->auto_indent = TRUE;
    syntax->line_comment = g_strdup("#");
    syntax->icon = g_strdup("mime");
    syntax->import_style = g_strdup("generic");
    syntax->import_strip_extensions = TRUE;
    syntax->import_dot_modules = FALSE;
    syntax->index_enabled = TRUE;
    return syntax;
}

/**
 * @brief Syntax by name.
 */
SyntaxDef *syntax_by_name(GPtrArray *syntaxes, const char *name) {
    if (!syntaxes || !name) return NULL;
    for (guint i = 0; i < syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(syntaxes, i);
        if (syntax && syntax->name && g_ascii_strcasecmp(syntax->name, name) == 0) return syntax;
    }
    return NULL;
}

/**
 * @brief Syntax matches basename.
 */
static gboolean syntax_matches_basename(const SyntaxDef *syntax, const char *basename) {
    if (!syntax || !syntax->filenames || !basename) return FALSE;
    for (guint i = 0; i < syntax->filenames->len; i++) {
        const char *name = g_ptr_array_index(syntax->filenames, i);
        if (name && g_ascii_strcasecmp(name, basename) == 0) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Syntax matches extension.
 */
static gboolean syntax_matches_extension(const SyntaxDef *syntax, const char *path) {
    if (!syntax || !syntax->extensions || !path) return FALSE;
    const char *dot = strrchr(path, '.');
    if (!dot) return FALSE;
    for (guint e = 0; e < syntax->extensions->len; e++) {
        const char *ext = g_ptr_array_index(syntax->extensions, e);
        if (ext && g_ascii_strcasecmp(dot, ext) == 0) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Syntax for path.
 */
SyntaxDef *syntax_for_path(GPtrArray *syntaxes, const char *path) {
    if (!syntaxes || !path) return NULL;
    char *basename = g_path_get_basename(path);

    for (guint i = 0; i < syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(syntaxes, i);
        if (syntax_matches_basename(syntax, basename)) {
            g_free(basename);
            return syntax;
        }
    }

    for (guint i = 0; i < syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(syntaxes, i);
        if (syntax_matches_extension(syntax, path)) {
            g_free(basename);
            return syntax;
        }
    }

    g_free(basename);
    return NULL;
}

/**
 * @brief Syntax path is indexable.
 */
gboolean syntax_path_is_indexable(GPtrArray *syntaxes, const char *path) {
    SyntaxDef *syntax = syntax_for_path(syntaxes, path);
    return syntax ? syntax->index_enabled : FALSE;
}

/**
 * @brief Syntax icon for path.
 */
const char *syntax_icon_for_path(GPtrArray *syntaxes, const char *path, gboolean is_dir) {
    if (is_dir) return "DIR";
    SyntaxDef *syntax = syntax_for_path(syntaxes, path);
    if (syntax && syntax->icon && syntax->icon[0] != '\0') return syntax->icon;
    return "mime";
}

/**
 * @brief Syntax language for path.
 */
const char *syntax_language_for_path(GPtrArray *syntaxes, const char *path) {
    if (!path) return "Buffer";
    SyntaxDef *syntax = syntax_for_path(syntaxes, path);
    if (syntax && syntax->name && syntax->name[0] != '\0') return syntax->name;
    return "Text";
}

/**
 * @brief Text has token.
 */
static gboolean text_has_token(const char *text, const char *token) {
    if (!text || !token) return FALSE;
    return g_strstr_len(text, 16384, token) != NULL;
}

/**
 * @brief Syntax for content.
 */
SyntaxDef *syntax_for_content(GPtrArray *syntaxes, GtkTextBuffer *buffer) {
    if (!syntaxes || !buffer) return NULL;
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    end = start;
    gtk_text_iter_forward_chars(&end, 16384);
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (!text) return NULL;
    char *trimmed = g_strdup(text);
    if (trimmed) g_strstrip(trimmed);

    SyntaxDef *syntax = NULL;
    if (trimmed && g_str_has_prefix(trimmed, "#!") && (text_has_token(trimmed, "sh") || text_has_token(trimmed, "bash"))) {
        syntax = syntax_by_name(syntaxes, "Shell");
    } else if (trimmed && (g_str_has_prefix(trimmed, "import ") || g_str_has_prefix(trimmed, "from ") ||
                           g_str_has_prefix(trimmed, "def ") || g_str_has_prefix(trimmed, "class "))) {
        syntax = syntax_by_name(syntaxes, "Python");
    } else if (text_has_token(text, "#include") || text_has_token(text, "int main(") || text_has_token(text, "int main (")) {
        syntax = syntax_by_name(syntaxes, "C");
    } else if (trimmed && (g_str_has_prefix(trimmed, "---") || text_has_token(text, ": ") || text_has_token(text, ":\n"))) {
        syntax = syntax_by_name(syntaxes, "YAML");
    } else if (trimmed && (g_str_has_prefix(trimmed, "# ") || g_str_has_prefix(trimmed, "## ") || text_has_token(text, "```"))) {
        syntax = syntax_by_name(syntaxes, "Markdown");
    } else if (text_has_token(text, "\\documentclass") ||
               text_has_token(text, "\\begin{document}") ||
               text_has_token(text, "\\section{")) {
        syntax = syntax_by_name(syntaxes, "LaTeX");
    }

    g_free(trimmed);
    g_free(text);
    return syntax;
}
