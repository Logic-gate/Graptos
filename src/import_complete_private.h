#ifndef CLEAF_IMPORT_COMPLETE_PRIVATE_H
#define CLEAF_IMPORT_COMPLETE_PRIVATE_H

#include "import_complete.h"
#include "editor_tab.h"
#include "syntax.h"

#define CLEAF_IMPORT_MAX_DIR_ENTRIES 4096u
#define CLEAF_IMPORT_MAX_NAME 160u
#define CLEAF_IMPORT_MAX_FILE_BYTES (512u * 1024u)

typedef enum {
    IMPORT_CTX_NONE = 0,
    IMPORT_CTX_MODULES,
    IMPORT_CTX_MEMBERS,
    IMPORT_CTX_PATH
} ImportContextKind;

typedef struct {
    ImportContextKind kind;
    char *module;
    char *token;
    char *dir_part;
    char *base_part;
    gboolean system_path;
} ImportParse;

void import_parse_clear(ImportParse *ctx);
gboolean import_parse_current_line(EditorTab *tab, ImportParse *ctx);
void import_collect_candidates(EditorTab *tab,
                               ImportParse *ctx,
                               GPtrArray *out,
                               guint max_results);

#endif /* CLEAF_IMPORT_COMPLETE_PRIVATE_H */
