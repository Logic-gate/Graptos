/**
 * @file src/project.h
 * @brief Project tree public API and implementation composition unit.
 */

#ifndef CLEAF_PROJECT_H
#define CLEAF_PROJECT_H

#include <gtk/gtk.h>

/**
 * @brief Project type definition.
 */
typedef struct _EditorWindow EditorWindow;

/**
 * @brief Project tree create.
 */
GtkWidget *project_tree_create(EditorWindow *win);
/**
 * @brief Project tree load folder.
 */
void project_tree_load_folder(EditorWindow *win, const char *folder_path);
/**
 * @brief Project tree clear.
 */
void project_tree_clear(EditorWindow *win);
/**
 * @brief Project tree refresh.
 */
void project_tree_refresh(EditorWindow *win);
/**
 * @brief Project tree close context.
 */
void project_tree_close_context(EditorWindow *win);
/**
 * @brief Project root count.
 */
guint project_root_count(EditorWindow *win);
/**
 * @brief Project root at.
 */
const char *project_root_at(EditorWindow *win, guint index);
/**
 * @brief Project root for path.
 */
const char *project_root_for_path(EditorWindow *win, const char *path);
/**
 * @brief Project has roots.
 */
gboolean project_has_roots(EditorWindow *win);

#endif /* CLEAF_PROJECT_H */
