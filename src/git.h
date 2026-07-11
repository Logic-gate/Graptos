#ifndef CLEAF_GIT_H
#define CLEAF_GIT_H

#include <gtk/gtk.h>

typedef struct _EditorWindow EditorWindow;

const char *cleaf_git_status_for_file(EditorWindow *win, const char *path);
void cleaf_git_refresh_all(EditorWindow *win);
void cleaf_git_refresh_and_rebuild(EditorWindow *win);
char *cleaf_git_repo_for_path(const char *path);

void action_git_status(GtkWidget *widget, gpointer user_data);
void action_git_diff(GtkWidget *widget, gpointer user_data);
void action_git_stage(GtkWidget *widget, gpointer user_data);
void action_git_unstage(GtkWidget *widget, gpointer user_data);
void action_git_stage_all(GtkWidget *widget, gpointer user_data);
void action_git_commit(GtkWidget *widget, gpointer user_data);
void action_git_pull(GtkWidget *widget, gpointer user_data);
void action_git_push(GtkWidget *widget, gpointer user_data);
void action_git_credentials(GtkWidget *widget, gpointer user_data);
void action_git_refresh(GtkWidget *widget, gpointer user_data);
void action_git_run(GtkWidget *widget, gpointer user_data);

#endif /* CLEAF_GIT_H */
