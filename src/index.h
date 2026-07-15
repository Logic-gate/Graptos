/**
 * @file src/index.h
 * @brief Project/reference index public API and implementation composition unit.
 */

#ifndef CLEAF_INDEX_H
#define CLEAF_INDEX_H

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
 */
void index_reference_free(gpointer data);
/**
 * @brief Index candidates for tab.
 */
GPtrArray *index_candidates_for_tab(EditorTab *tab, const char *prefix, guint max_results);
/**
 * @brief Index definition for word.
 */
char *index_definition_for_word(EditorTab *tab, const char *word);
/**
 * @brief Index references for word.
 */
GPtrArray *index_references_for_word(EditorTab *tab, const char *word, guint max_results);

#endif /* CLEAF_INDEX_H */
