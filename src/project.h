/**
 * @file src/project.h
 * @brief Project tree public API and implementation composition unit.
 * @details Projects change underneath the editor. We keep tree rows, filesystem context,
 *          and search helpers away from EditorTab so directory updates do not become
 *          buffer-management problems.
 */

#ifndef GRAPTOS_PROJECT_H
#define GRAPTOS_PROJECT_H

#include <gtk/gtk.h>

/**
 * @brief Project type definition.
 */
typedef struct _EditorWindow EditorWindow;

/**
 * @brief Project tree create.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *project_tree_create(EditorWindow *win);
/**
 * @brief Project tree load folder.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @param folder_path The folder path supplied by the caller.
 */
void project_tree_load_folder(EditorWindow *win, const char *folder_path);
/**
 * @brief Project tree clear.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 */
void project_tree_clear(EditorWindow *win);
/**
 * @brief Project tree refresh.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 */
void project_tree_refresh(EditorWindow *win);
/**
 * @brief Project tree close context.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 */
void project_tree_close_context(EditorWindow *win);
/**
 * @brief Project root count.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @return The computed value requested by the caller.
 */
guint project_root_count(EditorWindow *win);
/**
 * @brief Project root at.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @param index The index supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
const char *project_root_at(EditorWindow *win, guint index);
/**
 * @brief Project root for path.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
const char *project_root_for_path(EditorWindow *win, const char *path);
/**
 * @brief Project has roots.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean project_has_roots(EditorWindow *win);

#endif /* GRAPTOS_PROJECT_H */
