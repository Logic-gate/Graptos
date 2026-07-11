#ifndef CLEAF_SYNTAX_H
#define CLEAF_SYNTAX_H

#include <gtk/gtk.h>

#define CLEAF_MAX_HIGHLIGHT_BYTES (2u * 1024u * 1024u)
#define CLEAF_MAX_REGEX_MATCHES_PER_RULE 12000u
#define CLEAF_HIGHLIGHT_CONTEXT_LINES 120u


typedef struct {
    char *open;
    char *close;
} SyntaxPair;

typedef struct {
    char *name;
    char *pattern;
    char *color;
    gboolean bold;
    gboolean italic;
    gboolean underline;
    GRegex *regex;
} SyntaxRule;

typedef struct {
    char *name;
    char *line_comment;
    GPtrArray *extensions;  /* char* */
    GPtrArray *filenames;   /* char* exact basenames, e.g. Makefile */
    GPtrArray *completions; /* char* */
    GPtrArray *rules;       /* SyntaxRule* */
    GPtrArray *close_pairs; /* SyntaxPair*: general balanced delimiters */
    GPtrArray *line_close_pairs; /* SyntaxPair*: line-local required closers */
    GPtrArray *statement_required_enders; /* char*: e.g. ; */
    GPtrArray *statement_exempt_prefixes; /* char*: no missing-semicolon warning */
    GPtrArray *statement_exempt_suffixes; /* char*: no missing-semicolon warning */
    GPtrArray *indent_openers; /* char*: line suffix opens next indent */
    GPtrArray *indent_closers; /* char*: first non-space closes indent */
    gboolean auto_indent;
    char *icon;             /* short sidebar/index badge */
    char *import_style;     /* generic profile name from YAML */
    GPtrArray *import_keywords; /* char* starts a module/path import */
    GPtrArray *import_from_keywords; /* char* starts a from/import form */
    GPtrArray *import_member_keywords; /* char* separates module and members */
    GPtrArray *import_roots; /* char* static roots, relative to project/cwd */
    GPtrArray *import_env; /* char* environment variables containing paths */
    GPtrArray *import_extensions; /* char* importable file extensions */
    GPtrArray *import_member_files; /* char* files scanned for exported names */
    GPtrArray *import_static_modules; /* module=member|member rows from YAML */
    gboolean import_strip_extensions; /* remove suffixes from suggestions */
    gboolean import_dot_modules; /* map a.b to a/b for lookup */
    gboolean index_enabled; /* allow project/reference indexing */
} SyntaxDef;

void syntax_pair_free(gpointer data);
void syntax_rule_free(gpointer data);
void syntax_def_free(gpointer data);
GPtrArray *syntax_load_all(void);
SyntaxDef *syntax_by_name(GPtrArray *syntaxes, const char *name);
SyntaxDef *syntax_for_path(GPtrArray *syntaxes, const char *path);
gboolean syntax_path_is_indexable(GPtrArray *syntaxes, const char *path);
const char *syntax_icon_for_path(GPtrArray *syntaxes, const char *path, gboolean is_dir);
const char *syntax_language_for_path(GPtrArray *syntaxes, const char *path);
SyntaxDef *syntax_for_content(GPtrArray *syntaxes, GtkTextBuffer *buffer);
void syntax_apply(GtkTextBuffer *buffer, GPtrArray *syntaxes, SyntaxDef *active_syntax);
void syntax_apply_range(GtkTextBuffer *buffer, GPtrArray *syntaxes, SyntaxDef *active_syntax, guint start_line, guint end_line);
void syntax_clear(GtkTextBuffer *buffer, GPtrArray *syntaxes);
void syntax_clear_range(GtkTextBuffer *buffer, GPtrArray *syntaxes, guint start_line, guint end_line);
char *syntax_diagnostics(GPtrArray *syntaxes);

#endif /* CLEAF_SYNTAX_H */
