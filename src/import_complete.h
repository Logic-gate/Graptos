/**
 * @file src/import_complete.h
 * @brief Import completion public API and candidate collection.
 */

#ifndef CLEAF_IMPORT_COMPLETE_H
#define CLEAF_IMPORT_COMPLETE_H

#include <gtk/gtk.h>

/**
 * @brief Import complete type definition.
 */
typedef struct _EditorTab EditorTab;

/**
 * @brief Cleaf import completion max results macro.
 */
#define CLEAF_IMPORT_COMPLETION_MAX_RESULTS 80u

/**
 * @brief Import completion tab is import context.
 */
gboolean import_completion_tab_is_import_context(EditorTab *tab);
/**
 * @brief Import completion candidates for tab.
 */
GPtrArray *import_completion_candidates_for_tab(EditorTab *tab,
                                                const char *prefix,
                                                guint max_results);

#endif /* CLEAF_IMPORT_COMPLETE_H */
