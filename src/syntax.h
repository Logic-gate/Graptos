/**
 * @file src/syntax.h
 * @brief Syntax definition model and lookup helpers.
 * @details The syntax layer gives Graptoς useful language behavior without requiring LSP.
 *          YAML keeps the language definitions editable, while this code handles loading,
 *          highlighting, diagnostics, and safe fallbacks.
 */

#ifndef GRAPTOS_SYNTAX_H
#define GRAPTOS_SYNTAX_H

#include <gtk/gtk.h>

/**
 * @brief Graptoς max highlight bytes macro.
 */
#define GRAPTOS_MAX_HIGHLIGHT_BYTES (2u * 1024u * 1024u)
/**
 * @brief Graptoς max regex matches per rule macro.
 */
#define GRAPTOS_MAX_REGEX_MATCHES_PER_RULE 12000u
/**
 * @brief Graptoς highlight context lines macro.
 */
#define GRAPTOS_HIGHLIGHT_CONTEXT_LINES 120u


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
    char *scope; /**< Semantic formatter scope such as comment, string, character, or preprocessor. */
    char *pattern; /**< Pattern. */
    char *color; /**< Color. */
    gboolean bold; /**< Bold. */
    gboolean italic; /**< Italic. */
    gboolean underline; /**< Underline. */
    GRegex *regex; /**< Regex. */
} SyntaxRule;

/**
 * @brief Optional syntax-aware formatting policy.
 * @details Structural tokens stay in the existing syntax fields. This section
 *          only describes layout choices that are formatter-specific.
 */
typedef struct {
    gboolean enabled; /**< TRUE when formatting may modify buffers for this syntax. */
    char *scope; /**< Formatting target policy, currently selection_or_block. */
    char *profile; /**< Generic formatter profile such as brace_semicolon. */
    char *block_style; /**< Generic block model such as braces. */
    char *statement_style; /**< Generic statement model such as semicolon. */
    char *brace_style; /**< Brace placement policy: attach, allman, or stroustrup. */
    gboolean one_statement_per_line; /**< Split complete statements onto separate lines. */
    gboolean space_before_block_opener; /**< Insert a space before block openers. */
    gboolean space_after_comma; /**< Insert one space after commas. */
    gboolean space_after_control_keyword; /**< Insert one space after control keywords. */
    gboolean space_around_binary_operators; /**< Insert spaces around common binary operators. */
    gboolean space_around_logical_operators; /**< Insert spaces around binary logical operators && and ||. */
    char *pointer_alignment; /**< Pointer spacing policy: name, type, or middle. */
    guint continuation_indent; /**< Extra indent width for continued expressions. */
    guint max_column; /**< Preferred maximum line width before comma wrapping. */
    guint max_blank_lines; /**< Maximum preserved consecutive blank lines. */
    gint case_indent; /**< Extra indent for switch labels. */
    GPtrArray *logical_operators; /**< char*: operators spaced by logical-operator policy. */
    GPtrArray *binary_operators; /**< char*: operators spaced by binary-operator policy. */
    GPtrArray *unary_prefix_operators; /**< char*: prefix operators that stay attached to operands. */
    GPtrArray *protected_scopes; /**< char*: rule scopes copied without internal formatting. */
    gboolean preserve_blank_lines; /**< Keep blank lines from the input. */
    gboolean wrap_after_comma; /**< Break after commas when max_column is exceeded. */
    gboolean collapse_empty_blocks; /**< Collapse empty blocks when enabled. */
} SyntaxFormatting;

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
    char *lsp_command; /**< Language server executable command. */
    GPtrArray *lsp_args; /**< char* language server command arguments. */
    char *lsp_language_id; /**< LSP languageId for textDocument items. */
    SyntaxFormatting *formatting; /**< Optional formatting policy. */
    gboolean index_enabled; /**< allow project/reference indexing. */
} SyntaxDef;

/**
 * @brief Syntax pair free.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param data The callback context passed by the caller.
 */
void syntax_pair_free(gpointer data);
/**
 * @brief Syntax rule free.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param data The callback context passed by the caller.
 */
void syntax_rule_free(gpointer data);
/**
 * @brief Syntax formatting free.
 * @details Formatting policy is optional and owned by the syntax definition.
 * @param data The formatting object to release.
 */
void syntax_formatting_free(gpointer data);
/**
 * @brief Syntax def free.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param data The callback context passed by the caller.
 */
void syntax_def_free(gpointer data);
/**
 * @brief Syntax load all.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *syntax_load_all(void);
/**
 * @brief Syntax load all for project roots.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param project_roots The project roots supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *syntax_load_all_for_roots(GPtrArray *project_roots);
/**
 * @brief Syntax by name.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param name The name supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
SyntaxDef *syntax_by_name(GPtrArray *syntaxes, const char *name);
/**
 * @brief Syntax for path.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
SyntaxDef *syntax_for_path(GPtrArray *syntaxes, const char *path);
/**
 * @brief Syntax path is indexable.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean syntax_path_is_indexable(GPtrArray *syntaxes, const char *path);
/**
 * @brief Syntax icon for path.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @param is_dir The is dir supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
const char *syntax_icon_for_path(GPtrArray *syntaxes, const char *path, gboolean is_dir);
/**
 * @brief Syntax language for path.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
const char *syntax_language_for_path(GPtrArray *syntaxes, const char *path);
/**
 * @brief Syntax for content.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param buffer The text buffer used for the operation.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
SyntaxDef *syntax_for_content(GPtrArray *syntaxes, GtkTextBuffer *buffer);
/**
 * @brief Syntax apply.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param buffer The text buffer used for the operation.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param active_syntax The active syntax supplied by the caller.
 */
void syntax_apply(GtkTextBuffer *buffer, GPtrArray *syntaxes, SyntaxDef *active_syntax);
/**
 * @brief Syntax apply range.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param buffer The text buffer used for the operation.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param active_syntax The active syntax supplied by the caller.
 * @param start_line The start line supplied by the caller.
 * @param end_line The end line supplied by the caller.
 */
void syntax_apply_range(GtkTextBuffer *buffer, GPtrArray *syntaxes, SyntaxDef *active_syntax, guint start_line, guint end_line);
/**
 * @brief Syntax clear.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param buffer The text buffer used for the operation.
 * @param syntaxes The syntaxes supplied by the caller.
 */
void syntax_clear(GtkTextBuffer *buffer, GPtrArray *syntaxes);
/**
 * @brief Syntax clear range.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param buffer The text buffer used for the operation.
 * @param syntaxes The syntaxes supplied by the caller.
 * @param start_line The start line supplied by the caller.
 * @param end_line The end line supplied by the caller.
 */
void syntax_clear_range(GtkTextBuffer *buffer, GPtrArray *syntaxes, guint start_line, guint end_line);
/**
 * @brief Syntax diagnostics.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntaxes The syntaxes supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *syntax_diagnostics(GPtrArray *syntaxes);

#endif /* GRAPTOS_SYNTAX_H */
