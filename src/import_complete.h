/**
 * @file src/import_complete.h
 * @brief Import completion public API and candidate collection.
 * @details Import completion is intentionally pragmatic. We resolve enough project-local
 *          structure to be helpful when LSP is unavailable, without pretending this is a
 *          compiler for every language.
 */

#ifndef GRAPTOS_IMPORT_COMPLETE_H
#define GRAPTOS_IMPORT_COMPLETE_H

#include <gtk/gtk.h>

/**
 * @brief Import complete type definition.
 */
typedef struct _EditorTab EditorTab;

/**
 * @brief Graptoς import completion max results macro.
 */
#define GRAPTOS_IMPORT_COMPLETION_MAX_RESULTS 80u

/**
 * @brief Import completion tab is import context.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean import_completion_tab_is_import_context(EditorTab *tab);
/**
 * @brief Import completion candidates for tab.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *import_completion_candidates_for_tab(EditorTab *tab,
                                                const char *prefix,
                                                guint max_results);
/**
 * @brief Import member completion candidates for a resolved visible base.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param base The base supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *import_completion_member_candidates_for_tab(EditorTab *tab,
                                                       const char *base,
                                                       const char *prefix,
                                                       guint max_results);

#endif /* GRAPTOS_IMPORT_COMPLETE_H */
