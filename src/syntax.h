/**
 * @file src/syntax.h
 * @brief Syntax definition model and lookup helpers.
 */

#ifndef CLEAF_SYNTAX_H
#define CLEAF_SYNTAX_H

#include <gtk/gtk.h>

/**
 * @brief Cleaf max highlight bytes macro.
 */
#define CLEAF_MAX_HIGHLIGHT_BYTES (2u * 1024u * 1024u)
/**
 * @brief Cleaf max regex matches per rule macro.
 */
#define CLEAF_MAX_REGEX_MATCHES_PER_RULE 12000u
/**
 * @brief Cleaf highlight context lines macro.
 */
#define CLEAF_HIGHLIGHT_CONTEXT_LINES 120u


/**
 * @brief Syntax type definition.
 */
typedef struct {
    char *open; /**< Open. */
    char *close; /**< Close. */
} SyntaxPair;

/**
 * @brief Syntax type definition.
 */
typedef struct {
    char *name; /**< Name. */
    char *pattern; /**< Pattern. */
    char *color; /**< Color. */
    gboolean bold; /**< Bold. */
    gboolean italic; /**< Italic. */
    gboolean underline; /**< Underline. */
    GRegex *regex; /**< Regex. */
} SyntaxRule;

/**
 * @brief Syntax type definition.
 */
typedef struct {
    char *name; /**< Name. */
    char *line_comment; /**< Line comment. */
    GPtrArray *extensions; /**< char*. */
    GPtrArray *filenames; /**< char* exact basenames, e.g. Makefile. */
    GPtrArray *completions; /**< char*. */
    GPtrArray *rules; /**< SyntaxRule*. */
    GPtrArray *close_pairs; /**< SyntaxPair*: general balanced delimiters. */
    GPtrArray *line_close_pairs; /**< SyntaxPair*: line-local required closers. */
    GPtrArray *statement_required_enders; /**< char*: e.g. ;. */
    GPtrArray *statement_exempt_prefixes; /**< char*: no missing-semicolon warning. */
    GPtrArray *statement_exempt_suffixes; /**< char*: no missing-semicolon warning. */
    GPtrArray *indent_openers; /**< char*: line suffix opens next indent. */
    GPtrArray *indent_closers; /**< char*: first non-space closes indent. */
    gboolean auto_indent; /**< Auto indent. */
    char *icon; /**< short sidebar/index badge. */
    char *import_style; /**< generic profile name from YAML. */
    GPtrArray *import_keywords; /**< char* starts a module/path import. */
    GPtrArray *import_from_keywords; /**< char* starts a from/import form. */
    GPtrArray *import_member_keywords; /**< char* separates module and members. */
    GPtrArray *import_roots; /**< char* static roots, relative to project/cwd. */
    GPtrArray *import_env; /**< char* environment variables containing paths. */
    GPtrArray *import_extensions; /**< char* importable file extensions. */
    GPtrArray *import_member_files; /**< char* files scanned for exported names. */
    GPtrArray *import_static_modules; /**< module=member|member rows from YAML. */
    gboolean import_strip_extensions; /**< remove suffixes from suggestions. */
    gboolean import_dot_modules; /**< map a.b to a/b for lookup. */
    gboolean index_enabled; /**< allow project/reference indexing. */
} SyntaxDef;

/**
 * @brief Syntax pair free.
 */
void syntax_pair_free(gpointer data);
/**
 * @brief Syntax rule free.
 */
void syntax_rule_free(gpointer data);
/**
 * @brief Syntax def free.
 */
void syntax_def_free(gpointer data);
/**
 * @brief Syntax load all.
 */
GPtrArray *syntax_load_all(void);
/**
 * @brief Syntax by name.
 */
SyntaxDef *syntax_by_name(GPtrArray *syntaxes, const char *name);
/**
 * @brief Syntax for path.
 */
SyntaxDef *syntax_for_path(GPtrArray *syntaxes, const char *path);
/**
 * @brief Syntax path is indexable.
 */
gboolean syntax_path_is_indexable(GPtrArray *syntaxes, const char *path);
/**
 * @brief Syntax icon for path.
 */
const char *syntax_icon_for_path(GPtrArray *syntaxes, const char *path, gboolean is_dir);
/**
 * @brief Syntax language for path.
 */
const char *syntax_language_for_path(GPtrArray *syntaxes, const char *path);
/**
 * @brief Syntax for content.
 */
SyntaxDef *syntax_for_content(GPtrArray *syntaxes, GtkTextBuffer *buffer);
/**
 * @brief Syntax apply.
 */
void syntax_apply(GtkTextBuffer *buffer, GPtrArray *syntaxes, SyntaxDef *active_syntax);
/**
 * @brief Syntax apply range.
 */
void syntax_apply_range(GtkTextBuffer *buffer, GPtrArray *syntaxes, SyntaxDef *active_syntax, guint start_line, guint end_line);
/**
 * @brief Syntax clear.
 */
void syntax_clear(GtkTextBuffer *buffer, GPtrArray *syntaxes);
/**
 * @brief Syntax clear range.
 */
void syntax_clear_range(GtkTextBuffer *buffer, GPtrArray *syntaxes, guint start_line, guint end_line);
/**
 * @brief Syntax diagnostics.
 */
char *syntax_diagnostics(GPtrArray *syntaxes);

#endif /* CLEAF_SYNTAX_H */
