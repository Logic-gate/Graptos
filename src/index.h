#ifndef CLEAF_INDEX_H
#define CLEAF_INDEX_H

#include <gtk/gtk.h>

typedef struct _EditorTab EditorTab;

typedef struct {
    char *path;      /* NULL means active unsaved buffer */
    char *display;   /* compact path label */
    guint line;
    char *snippet;
    char *kind;
} IndexReference;

void index_reference_free(gpointer data);
GPtrArray *index_candidates_for_tab(EditorTab *tab, const char *prefix, guint max_results);
char *index_definition_for_word(EditorTab *tab, const char *word);
GPtrArray *index_references_for_word(EditorTab *tab, const char *word, guint max_results);

#endif /* CLEAF_INDEX_H */
