#ifndef CLEAF_PROJECT_H
#define CLEAF_PROJECT_H

#include <gtk/gtk.h>

typedef struct _EditorWindow EditorWindow;

GtkWidget *project_tree_create(EditorWindow *win);
void project_tree_load_folder(EditorWindow *win, const char *folder_path);
void project_tree_clear(EditorWindow *win);
void project_tree_refresh(EditorWindow *win);
void project_tree_close_context(EditorWindow *win);
guint project_root_count(EditorWindow *win);
const char *project_root_at(EditorWindow *win, guint index);
const char *project_root_for_path(EditorWindow *win, const char *path);
gboolean project_has_roots(EditorWindow *win);

#endif /* CLEAF_PROJECT_H */
