/**
 * @file src/import_complete_private.h
 * @brief Internal import completion data structures.
 * @details Import completion is intentionally pragmatic. We resolve enough project-local
 *          structure to be helpful when LSP is unavailable, without pretending this is a
 *          compiler for every language.
 */

#ifndef GRAPTOS_IMPORT_COMPLETE_PRIVATE_H
#define GRAPTOS_IMPORT_COMPLETE_PRIVATE_H

#include "import_complete.h"
#include "editor_tab.h"
#include "syntax.h"

/**
 * @brief Graptoς import max dir entries macro.
 */
#define GRAPTOS_IMPORT_MAX_DIR_ENTRIES 4096u
/**
 * @brief Graptoς import max name macro.
 */
#define GRAPTOS_IMPORT_MAX_NAME 160u
/**
 * @brief Graptoς import max file bytes macro.
 */
#define GRAPTOS_IMPORT_MAX_FILE_BYTES (512u * 1024u)

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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param ctx The ctx supplied by the caller.
 */
void import_parse_clear(ImportParse *ctx);
/**
 * @brief Import parse current line.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param ctx The ctx supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean import_parse_current_line(EditorTab *tab, ImportParse *ctx);
/**
 * @brief Import collect candidates.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param ctx The ctx supplied by the caller.
 * @param out Output storage filled when the lookup succeeds.
 * @param max_results The max results supplied by the caller.
 */
void import_collect_candidates(EditorTab *tab,
                               ImportParse *ctx,
                               GPtrArray *out,
                               guint max_results);

#endif /* GRAPTOS_IMPORT_COMPLETE_PRIVATE_H */
