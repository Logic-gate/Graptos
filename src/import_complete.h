#ifndef CLEAF_IMPORT_COMPLETE_H
#define CLEAF_IMPORT_COMPLETE_H

#include <gtk/gtk.h>

typedef struct _EditorTab EditorTab;

#define CLEAF_IMPORT_COMPLETION_MAX_RESULTS 80u

gboolean import_completion_tab_is_import_context(EditorTab *tab);
GPtrArray *import_completion_candidates_for_tab(EditorTab *tab,
                                                const char *prefix,
                                                guint max_results);

#endif /* CLEAF_IMPORT_COMPLETE_H */
