/**
 * @file src/git.h
 * @brief Git integration public API and implementation composition unit.
 * @details Git already knows the repository truth. Graptoς adds UI, status parsing,
 *          credentials, and command wiring, but we avoid building a half-Git inside the
 *          editor.
 */

#ifndef GRAPTOS_GIT_H
#define GRAPTOS_GIT_H

#include <gtk/gtk.h>

/**
 * @brief Git type definition.
 */
typedef struct _EditorWindow EditorWindow;

/**
 * @brief Graptoς git status for file.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
const char *graptos_git_status_for_file(EditorWindow *win, const char *path);
/**
 * @brief Graptoς git refresh all.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param win The win supplied by the caller.
 */
void graptos_git_refresh_all(EditorWindow *win);
/**
 * @brief Graptoς git refresh and rebuild.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param win The win supplied by the caller.
 */
void graptos_git_refresh_and_rebuild(EditorWindow *win);
/**
 * @brief Graptoς git repo for path.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_git_repo_for_path(const char *path);

/**
 * @brief Action git status.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_status(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git diff.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_diff(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git stage.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_stage(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git unstage.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_unstage(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git stage all.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_stage_all(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git commit.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_commit(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git pull.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_pull(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git push.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_push(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git credentials.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_credentials(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git refresh.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_refresh(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git run.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_run(GtkWidget *widget, gpointer user_data);

#endif /* GRAPTOS_GIT_H */
