/**
 * @file src/git/git_actions.inc.c
 * @brief Cleaf git actions module.
 */

static void show_git_output(EditorWindow *win,
                            const char *title,
                            const char *body) {
    dialog_output(win ? app_window_gtk(win) : NULL,
                  title ? title : "Git",
                  title ? title : "Git",
                  body);
}

/**
 * @brief Show git error.
 */
static void show_git_error(EditorWindow *win,
                           const char *operation,
                           const CleafGitResult *result) {
    if (!result) return;
    GString *text = g_string_new(NULL);
    g_string_append_printf(text, "%s\n\n", result->message ? result->message : git_error_title(result->kind));
    g_string_append_printf(text, "%s\n\n", git_error_hint(result->kind));
    g_string_append_printf(text, "Exit code: %d\n\n", result->exit_code);
    if (result->err && result->err[0]) {
        g_string_append(text, "stderr:\n");
        g_string_append(text, result->err);
        g_string_append(text, "\n\n");
    }
    if (result->out && result->out[0]) {
        g_string_append(text, "stdout:\n");
        g_string_append(text, result->out);
    }
    app_window_set_error_status(win,
                                operation ? operation : git_error_title(result->kind),
                                text->str);
    g_string_free(text, TRUE);
}

/**
 * @brief Show result or error.
 */
static void show_result_or_error(EditorWindow *win,
                                 const char *title,
                                 const CleafGitResult *result,
                                 gboolean ok) {
    if (!result) return;
    if (!ok) {
        show_git_error(win, title, result);
        return;
    }
    GString *text = g_string_new(NULL);
    if (result->out && result->out[0]) g_string_append(text, result->out);
    if (result->err && result->err[0]) {
        if (text->len > 0u) g_string_append(text, "\n");
        g_string_append(text, result->err);
    }
    if (text->len == 0u) g_string_append(text, "Git command completed.");
    show_git_output(win, title, text->str);
    g_string_free(text, TRUE);
}

/**
 * @brief Repo summary.
 */
static char *repo_summary(const char *repo) {
    GString *summary = g_string_new(NULL);
    g_string_append_printf(summary, "Repository: %s\n", repo ? repo : "none");

    CleafGitResult branch;
    if (run_git_args(repo, NULL, &branch, "branch", "--show-current", NULL)) {
        char *b = chomp_dup(branch.out);
        g_string_append_printf(summary, "Branch: %s\n", b && b[0] ? b : "detached or unnamed");
        g_free(b);
    } else {
        g_string_append(summary, "Branch: unknown\n");
    }
    git_result_clear(&branch);

    CleafGitResult remote;
    if (run_git_args(repo, NULL, &remote, "remote", "-v", NULL)) {
        g_string_append(summary, "\nRemotes:\n");
        g_string_append(summary, remote.out && remote.out[0] ? remote.out : "none\n");
    }
    git_result_clear(&remote);
    return g_string_free(summary, FALSE);
}

/**
 * @brief Action git status.
 */
void action_git_status(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        app_window_set_error_status(win, "Git status failed",
                                    "The current file or project folder is not inside a Git repository.");
        return;
    }

    CleafGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result, "status", "--short", "--branch", NULL);
    if (!ok) {
        show_git_error(win, "Git Status", &result);
        git_result_clear(&result);
        g_free(repo);
        return;
    }

    char *summary = repo_summary(repo);
    GString *body = g_string_new(summary ? summary : "");
    g_string_append(body, "\nStatus:\n");
    g_string_append(body, result.out && result.out[0] ? result.out : "Clean working tree.\n");
    show_git_output(win, "Git Status", body->str);
    g_string_free(body, TRUE);
    g_free(summary);
    git_result_clear(&result);
    g_free(repo);
}

/**
 * @brief Diff syntax.
 */
static SyntaxDef *diff_syntax(EditorWindow *win) {
    if (!win || !win->syntaxes) return NULL;
    for (guint i = 0u; i < win->syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(win->syntaxes, i);
        if (syntax && syntax->name && g_ascii_strcasecmp(syntax->name, "Diff") == 0) return syntax;
    }
    return NULL;
}

/**
 * @brief Open text tab.
 */
static void open_text_tab(EditorWindow *win,
                          const char *title,
                          const char *text,
                          gboolean diff) {
    if (!win) return;
    EditorTab *tab = editor_tab_new(win);
    tab->applying_change = TRUE;
    gtk_text_buffer_set_text(tab->buffer, text ? text : "", -1);
    tab->applying_change = FALSE;
    tab->modified = FALSE;
    tab->locked = TRUE;
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tab->text_view), FALSE);
    if (diff) editor_tab_set_syntax(tab, diff_syntax(win), TRUE);
    app_window_add_tab(win, tab, TRUE);
    if (tab->tab_title) gtk_label_set_text(GTK_LABEL(tab->tab_title), title ? title : "Git Output");
    editor_tab_update_status(tab);
}

/**
 * @brief Action git diff.
 */
void action_git_diff(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *rel = NULL;
    char *repo = current_repo(win, &rel);
    if (!repo) {
        app_window_set_error_status(win, "Git diff failed",
                                    "The current file or project folder is not inside a Git repository.");
        g_free(rel);
        return;
    }

    CleafGitResult result;
    gboolean ok;
    if (rel && rel[0] && g_strcmp0(rel, ".") != 0) {
        ok = run_git_args(repo, NULL, &result, "diff", "--", rel, NULL);
    } else {
        ok = run_git_args(repo, NULL, &result, "diff", NULL);
    }

    if (!ok) show_git_error(win, "Git Diff", &result);
    else open_text_tab(win, rel && rel[0] ? "Git Diff: current file" : "Git Diff", result.out && result.out[0] ? result.out : "No unstaged diff.\n", TRUE);
    git_result_clear(&result);
    g_free(rel);
    g_free(repo);
}

/**
 * @brief Refresh after command.
 */
static void refresh_after_command(EditorWindow *win, const char *status) {
    if (status) app_window_set_status(win, status);
    cleaf_git_refresh_and_rebuild(win);
}

/**
 * @brief Action git stage.
 */
void action_git_stage(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *rel = NULL;
    char *repo = current_repo(win, &rel);
    if (!repo || !rel || g_strcmp0(rel, ".") == 0) {
        app_window_set_error_status(win, "Git stage failed",
                                    "Open a file inside a Git repository first, or use Stage All.");
        g_free(rel);
        g_free(repo);
        return;
    }

    CleafGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result, "add", "--", rel, NULL);
    if (!ok) show_git_error(win, "Git Stage", &result);
    else refresh_after_command(win, "Git staged current file.");
    git_result_clear(&result);
    g_free(rel);
    g_free(repo);
}

/**
 * @brief Action git unstage.
 */
void action_git_unstage(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *rel = NULL;
    char *repo = current_repo(win, &rel);
    if (!repo || !rel || g_strcmp0(rel, ".") == 0) {
        app_window_set_error_status(win, "Git unstage failed",
                                    "Open a file inside a Git repository first.");
        g_free(rel);
        g_free(repo);
        return;
    }

    CleafGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result, "restore", "--staged", "--", rel, NULL);
    if (!ok) {
        git_result_clear(&result);
        ok = run_git_args(repo, NULL, &result, "reset", "HEAD", "--", rel, NULL);
    }
    if (!ok) show_git_error(win, "Git Unstage", &result);
    else refresh_after_command(win, "Git unstaged current file.");
    git_result_clear(&result);
    g_free(rel);
    g_free(repo);
}

/**
 * @brief Action git stage all.
 */
void action_git_stage_all(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        app_window_set_error_status(win, "Git stage all failed",
                                    "No Git repository is active.");
        return;
    }

    CleafGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result, "add", "-A", NULL);
    if (!ok) show_git_error(win, "Git Stage All", &result);
    else refresh_after_command(win, "Git staged all changes.");
    git_result_clear(&result);
    g_free(repo);
}

/**
 * @brief Action git commit.
 */
void action_git_commit(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        app_window_set_error_status(win, "Git commit failed",
                                    "No Git repository is active.");
        return;
    }

    char *message = dialog_prompt_text(app_window_gtk(win), "Git Commit", "Commit message:", "");
    if (!message) {
        g_free(repo);
        return;
    }
    g_strstrip(message);
    if (message[0] == '\0') {
        app_window_set_error_status(win, "Git commit failed",
                                    "Commit message cannot be empty.");
        g_free(message);
        g_free(repo);
        return;
    }

    CleafGitResult check;
    gboolean staged = !run_git_args(repo, NULL, &check, "diff", "--cached", "--quiet", NULL)
        && check.exit_code == 1;
    git_result_clear(&check);
    if (!staged) {
        app_window_set_error_status(win, "Git commit failed",
                                    "No staged changes to commit. Use Git > Stage or Git > Stage All first.");
        g_free(message);
        g_free(repo);
        return;
    }

    CleafGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result, "commit", "-m", message, NULL);
    if (!ok) show_git_error(win, "Git Commit", &result);
    else {
        show_result_or_error(win, "Git Commit", &result, TRUE);
        refresh_after_command(win, "Git commit completed.");
    }
    git_result_clear(&result);
    g_free(message);
    g_free(repo);
}

/**
 * @brief Action git pull.
 */
void action_git_pull(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        app_window_set_error_status(win, "Git pull failed",
                                    "No Git repository is active.");
        return;
    }
    CleafGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result, "pull", "--ff-only", NULL);
    show_result_or_error(win, "Git Pull", &result, ok);
    if (ok) refresh_after_command(win, "Git pull completed.");
    git_result_clear(&result);
    g_free(repo);
}

/**
 * @brief Action git push.
 */
void action_git_push(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        app_window_set_error_status(win, "Git push failed",
                                    "No Git repository is active.");
        return;
    }
    CleafGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result, "push", NULL);
    if (!ok && result.kind == CLEAF_GIT_ERROR_NO_REMOTE) {
        git_result_clear(&result);
        CleafGitResult branch;
        if (run_git_args(repo, NULL, &branch, "branch", "--show-current", NULL)) {
            char *b = chomp_dup(branch.out);
            git_result_clear(&branch);
            if (b && b[0] && dialog_confirm_yes_no(app_window_gtk(win), "Set upstream?", "No upstream appears to be configured. Push with `git push -u origin <current-branch>`?")) {
                ok = run_git_args(repo, NULL, &result, "push", "-u", "origin", b, NULL);
            } else {
                g_free(b);
                g_free(repo);
                return;
            }
            g_free(b);
        } else {
            git_result_clear(&branch);
            ok = FALSE;
            run_git_args(repo, NULL, &result, "push", NULL);
        }
    }
    show_result_or_error(win, "Git Push", &result, ok);
    if (ok) refresh_after_command(win, "Git push completed.");
    git_result_clear(&result);
    g_free(repo);
}
