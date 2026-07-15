/**
 * @file src/git.h
 * @brief Git integration public API and implementation composition unit.
 */

#ifndef CLEAF_GIT_H
#define CLEAF_GIT_H

#include <gtk/gtk.h>

/**
 * @brief Git type definition.
 */
typedef struct _EditorWindow EditorWindow;

/**
 * @brief Cleaf git status for file.
 */
const char *cleaf_git_status_for_file(EditorWindow *win, const char *path);
/**
 * @brief Cleaf git refresh all.
 */
void cleaf_git_refresh_all(EditorWindow *win);
/**
 * @brief Cleaf git refresh and rebuild.
 */
void cleaf_git_refresh_and_rebuild(EditorWindow *win);
/**
 * @brief Cleaf git repo for path.
 */
char *cleaf_git_repo_for_path(const char *path);

/**
 * @brief Action git status.
 */
void action_git_status(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git diff.
 */
void action_git_diff(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git stage.
 */
void action_git_stage(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git unstage.
 */
void action_git_unstage(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git stage all.
 */
void action_git_stage_all(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git commit.
 */
void action_git_commit(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git pull.
 */
void action_git_pull(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git push.
 */
void action_git_push(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git credentials.
 */
void action_git_credentials(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git refresh.
 */
void action_git_refresh(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action git run.
 */
void action_git_run(GtkWidget *widget, gpointer user_data);

#endif /* CLEAF_GIT_H */
