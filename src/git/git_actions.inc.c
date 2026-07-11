static void show_git_output(EditorWindow *win,
                            const char *title,
                            const char *body) {
    GtkWidget *window = gtk_window_new();
    gtk_widget_add_css_class(window, "cleaf-window");
    gtk_window_set_title(GTK_WINDOW(window), title ? title : "Git");
    gtk_window_set_default_size(GTK_WINDOW(window), 760, 520);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    if (win) gtk_window_set_transient_for(GTK_WINDOW(window), app_window_gtk(win));

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(box, "cleaf-root");
    cleaf_set_all_margins(box, 10);
    gtk_window_set_child(GTK_WINDOW(window), box);

    GtkWidget *label = gtk_label_new(title ? title : "Git");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "cleaf-menu-title");
    gtk_box_append(GTK_BOX(box), label);

    GtkTextBuffer *buffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(buffer, body ? body : "", -1);
    GtkWidget *view = gtk_text_view_new_with_buffer(buffer);
    g_object_unref(buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_NONE);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view);
    gtk_box_append(GTK_BOX(box), scrolled);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(row, GTK_ALIGN_END);
    GtkWidget *close = cleaf_flat_button_new("Close", NULL,
        G_CALLBACK(cleaf_modal_window_respond),
        GINT_TO_POINTER(GTK_RESPONSE_CLOSE));
    gtk_box_append(GTK_BOX(row), close);
    gtk_box_append(GTK_BOX(box), row);

    (void)cleaf_modal_window_run(GTK_WINDOW(window), GTK_RESPONSE_CLOSE);
    cleaf_widget_destroy(window);
}

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
    show_git_output(win, operation ? operation : git_error_title(result->kind), text->str);
    g_string_free(text, TRUE);
}

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

void action_git_status(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        dialog_error(app_window_gtk(win), "Git status failed", "The current file or project folder is not inside a Git repository.");
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

static SyntaxDef *diff_syntax(EditorWindow *win) {
    if (!win || !win->syntaxes) return NULL;
    for (guint i = 0u; i < win->syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(win->syntaxes, i);
        if (syntax && syntax->name && g_ascii_strcasecmp(syntax->name, "Diff") == 0) return syntax;
    }
    return NULL;
}

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

void action_git_diff(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *rel = NULL;
    char *repo = current_repo(win, &rel);
    if (!repo) {
        dialog_error(app_window_gtk(win), "Git diff failed", "The current file or project folder is not inside a Git repository.");
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

static void refresh_after_command(EditorWindow *win, const char *status) {
    if (status) app_window_set_status(win, status);
    cleaf_git_refresh_and_rebuild(win);
}

void action_git_stage(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *rel = NULL;
    char *repo = current_repo(win, &rel);
    if (!repo || !rel || g_strcmp0(rel, ".") == 0) {
        dialog_error(app_window_gtk(win), "Git stage failed", "Open a file inside a Git repository first, or use Stage All.");
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

void action_git_unstage(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *rel = NULL;
    char *repo = current_repo(win, &rel);
    if (!repo || !rel || g_strcmp0(rel, ".") == 0) {
        dialog_error(app_window_gtk(win), "Git unstage failed", "Open a file inside a Git repository first.");
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

void action_git_stage_all(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        dialog_error(app_window_gtk(win), "Git stage all failed", "No Git repository is active.");
        return;
    }

    CleafGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result, "add", "-A", NULL);
    if (!ok) show_git_error(win, "Git Stage All", &result);
    else refresh_after_command(win, "Git staged all changes.");
    git_result_clear(&result);
    g_free(repo);
}

void action_git_commit(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        dialog_error(app_window_gtk(win), "Git commit failed", "No Git repository is active.");
        return;
    }

    char *message = dialog_prompt_text(app_window_gtk(win), "Git Commit", "Commit message:", "");
    if (!message) {
        g_free(repo);
        return;
    }
    g_strstrip(message);
    if (message[0] == '\0') {
        dialog_error(app_window_gtk(win), "Git commit failed", "Commit message cannot be empty.");
        g_free(message);
        g_free(repo);
        return;
    }

    CleafGitResult check;
    gboolean staged = !run_git_args(repo, NULL, &check, "diff", "--cached", "--quiet", NULL)
        && check.exit_code == 1;
    git_result_clear(&check);
    if (!staged) {
        dialog_error(app_window_gtk(win), "Git commit failed", "No staged changes to commit. Use Git > Stage or Git > Stage All first.");
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

void action_git_pull(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        dialog_error(app_window_gtk(win), "Git pull failed", "No Git repository is active.");
        return;
    }
    CleafGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result, "pull", "--ff-only", NULL);
    show_result_or_error(win, "Git Pull", &result, ok);
    if (ok) refresh_after_command(win, "Git pull completed.");
    git_result_clear(&result);
    g_free(repo);
}

void action_git_push(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        dialog_error(app_window_gtk(win), "Git push failed", "No Git repository is active.");
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

