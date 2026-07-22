/**
 * @file src/index.h
 * @brief Project/reference index public API and implementation composition unit.
 * @details The index is our local memory of the project. It trades perfect semantic
 *          knowledge for speed and predictability, which is the right fallback when
 *          external tooling is absent or incomplete.
 */

#ifndef GRAPTOS_INDEX_H
#define GRAPTOS_INDEX_H

#include <gtk/gtk.h>

/**
 * @brief Index type definition.
 */
typedef struct _EditorTab EditorTab;

/**
 * @brief Index type definition.
 */
typedef struct {
    char *path; /**< NULL means active unsaved buffer. */
    char *display; /**< compact path label. */
    guint line; /**< Line. */
    char *snippet; /**< Snippet. */
    char *kind; /**< Kind. */
} IndexReference;

/**
 * @brief Index reference free.
 * @details The index is a lightweight fallback when richer language services cannot answer. The comment shows where inexpensive symbol data is collected and where callers receive owned results.
 * @param data The callback context passed by the caller.
 */
void index_reference_free(gpointer data);
/**
 * @brief Index candidates for tab.
 * @details The index is a lightweight fallback when richer language services cannot answer. The comment shows where inexpensive symbol data is collected and where callers receive owned results.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *index_candidates_for_tab(EditorTab *tab, const char *prefix, guint max_results);
/**
 * @brief Index definition for word.
 * @details The index is a lightweight fallback when richer language services cannot answer. The comment shows where inexpensive symbol data is collected and where callers receive owned results.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *index_definition_for_word(EditorTab *tab, const char *word);
/**
 * @brief Index references for word.
 * @details The index is a lightweight fallback when richer language services cannot answer. The comment shows where inexpensive symbol data is collected and where callers receive owned results.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *index_references_for_word(EditorTab *tab, const char *word, guint max_results);

#endif /* GRAPTOS_INDEX_H */
