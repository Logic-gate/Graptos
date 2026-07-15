/**
 * @file src/import_complete_private.h
 * @brief Internal import completion data structures.
 */

#ifndef CLEAF_IMPORT_COMPLETE_PRIVATE_H
#define CLEAF_IMPORT_COMPLETE_PRIVATE_H

#include "import_complete.h"
#include "editor_tab.h"
#include "syntax.h"

/**
 * @brief Cleaf import max dir entries macro.
 */
#define CLEAF_IMPORT_MAX_DIR_ENTRIES 4096u
/**
 * @brief Cleaf import max name macro.
 */
#define CLEAF_IMPORT_MAX_NAME 160u
/**
 * @brief Cleaf import max file bytes macro.
 */
#define CLEAF_IMPORT_MAX_FILE_BYTES (512u * 1024u)

/**
 * @brief Import complete private type definition.
 */
typedef enum {
    IMPORT_CTX_NONE = 0, /**< Import ctx none. */
    IMPORT_CTX_MODULES, /**< Import ctx modules. */
    IMPORT_CTX_MEMBERS, /**< Import ctx members. */
    IMPORT_CTX_PATH /**< Import ctx path. */
} ImportContextKind;

/**
 * @brief Import complete private type definition.
 */
typedef struct {
    ImportContextKind kind; /**< Kind. */
    char *module; /**< Module. */
    char *token; /**< Token. */
    char *dir_part; /**< Dir part. */
    char *base_part; /**< Base part. */
    gboolean system_path; /**< System path. */
} ImportParse;

/**
 * @brief Import parse clear.
 */
void import_parse_clear(ImportParse *ctx);
/**
 * @brief Import parse current line.
 */
gboolean import_parse_current_line(EditorTab *tab, ImportParse *ctx);
/**
 * @brief Import collect candidates.
 */
void import_collect_candidates(EditorTab *tab,
                               ImportParse *ctx,
                               GPtrArray *out,
                               guint max_results);

#endif /* CLEAF_IMPORT_COMPLETE_PRIVATE_H */
