/**
 * @file src/git/git_credentials.inc.c
 * @brief Graptoς git credentials module.
 * @details Git already knows the repository truth. Graptoς adds UI, status parsing,
 *          credentials, and command wiring, but we avoid building a half-Git inside the
 *          editor.
 * @param repo The repo supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */

static char *remote_host_for_repo(const char *repo) {
    GraptosGitResult result;
    if (!run_git_args(repo, NULL, &result, "remote", "get-url", "origin", NULL)) {
        git_result_clear(&result);
        return g_strdup("github.com");
    }
    char *url = chomp_dup(result.out);
    git_result_clear(&result);
    char *host = NULL;
    if (g_str_has_prefix(url, "https://")) {
        const char *start = url + strlen("https://");
        const char *slash = strchr(start, '/');
        host = slash ? g_strndup(start, (gsize)(slash - start)) : g_strdup(start);
    } else if (strchr(url, '@') && strchr(url, ':')) {
        const char *at = strchr(url, '@');
        const char *colon = strchr(at + 1, ':');
        host = colon ? g_strndup(at + 1, (gsize)(colon - at - 1)) : NULL;
    }
    if (!host || host[0] == '\0') {
        g_clear_pointer(&host, g_free);
        host = g_strdup("github.com");
    }
    g_free(url);
    return host;
}

/**
 * @brief Credential dialog.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param win The win supplied by the caller.
 * @param protocol_out Output storage filled when the operation can provide a value.
 * @param host_out Output storage filled when the operation can provide a value.
 * @param username_out Output storage filled when the operation can provide a value.
 * @param secret_out Output storage filled when the operation can provide a value.
 * @param helper_out Output storage filled when the operation can provide a value.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *credential_dialog(EditorWindow *win,
                               char **protocol_out,
                               char **host_out,
                               char **username_out,
                               char **secret_out,
                               char **helper_out) {
    if (protocol_out) *protocol_out = NULL;
    if (host_out) *host_out = NULL;
    if (username_out) *username_out = NULL;
    if (secret_out) *secret_out = NULL;
    if (helper_out) *helper_out = NULL;

    char *repo = current_repo(win, NULL);
    char *default_host = remote_host_for_repo(repo);
    g_free(repo);

    GtkWidget *window = gtk_window_new();
    gtk_widget_add_css_class(window, "graptos-window");
    gtk_widget_add_css_class(window, "graptos-dialog");
    gtk_window_set_title(GTK_WINDOW(window), "Git Credentials");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    if (win) gtk_window_set_transient_for(GTK_WINDOW(window), app_window_gtk(win));

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(box, "graptos-root");
    gtk_widget_add_css_class(box, "graptos-dialog-root");
    gtk_widget_add_css_class(box, "graptos-flat-dialog");
    graptos_set_all_margins(box, 14);
    gtk_window_set_child(GTK_WINDOW(window), box);

    GtkWidget *title = gtk_label_new("Save Git HTTPS credentials through Git credential helper");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "graptos-menu-title");
    gtk_widget_add_css_class(title, "graptos-dialog-title");
    gtk_box_append(GTK_BOX(box), title);

    GtkWidget *hint = gtk_label_new("Supports GitHub, GitLab, Gitea, and generic HTTPS Git hosts. For GitHub, use a personal access token as the secret.");
    gtk_label_set_wrap(GTK_LABEL(hint), TRUE);
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_widget_add_css_class(hint, "graptos-dialog-body");
    gtk_box_append(GTK_BOX(box), hint);

    GtkWidget *protocol = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(protocol), "https");
    GtkWidget *host = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(host), default_host ? default_host : "github.com");
    GtkWidget *username = gtk_entry_new();
    GtkWidget *secret = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(secret), FALSE);
    GtkWidget *helper = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(helper), "credential.helper, e.g. cache --timeout=28800, store, libsecret; leave blank to keep existing");

    gtk_box_append(GTK_BOX(box), gtk_label_new("Protocol:"));
    gtk_box_append(GTK_BOX(box), protocol);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Host:"));
    gtk_box_append(GTK_BOX(box), host);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Username:"));
    gtk_box_append(GTK_BOX(box), username);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Token / password:"));
    gtk_box_append(GTK_BOX(box), secret);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Optional Git credential helper:"));
    gtk_box_append(GTK_BOX(box), helper);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(row, "graptos-dialog-actions");
    gtk_widget_set_halign(row, GTK_ALIGN_END);
    GtkWidget *cancel = graptos_flat_button_new("Cancel", NULL,
        G_CALLBACK(graptos_modal_window_respond),
        GINT_TO_POINTER(GTK_RESPONSE_CANCEL));
    GtkWidget *save = graptos_flat_button_new("Save", NULL,
        G_CALLBACK(graptos_modal_window_respond),
        GINT_TO_POINTER(GTK_RESPONSE_ACCEPT));
    gtk_widget_add_css_class(cancel, "graptos-dialog-action");
    gtk_widget_add_css_class(save, "graptos-dialog-action");
    gtk_widget_add_css_class(save, "graptos-dialog-action-default");
    gtk_box_append(GTK_BOX(row), cancel);
    gtk_box_append(GTK_BOX(row), save);
    gtk_box_append(GTK_BOX(box), row);

    char *error = NULL;
    if (graptos_modal_window_run(GTK_WINDOW(window), GTK_RESPONSE_CANCEL) == GTK_RESPONSE_ACCEPT) {
        *protocol_out = g_strdup(gtk_entry_get_text(GTK_ENTRY(protocol)));
        *host_out = g_strdup(gtk_entry_get_text(GTK_ENTRY(host)));
        *username_out = g_strdup(gtk_entry_get_text(GTK_ENTRY(username)));
        *secret_out = g_strdup(gtk_entry_get_text(GTK_ENTRY(secret)));
        *helper_out = g_strdup(gtk_entry_get_text(GTK_ENTRY(helper)));
        g_strstrip(*protocol_out);
        g_strstrip(*host_out);
        g_strstrip(*username_out);
        g_strstrip(*secret_out);
        g_strstrip(*helper_out);
        if ((*protocol_out)[0] == '\0' || (*host_out)[0] == '\0'
            || (*username_out)[0] == '\0' || (*secret_out)[0] == '\0') {
            error = g_strdup("Protocol, host, username, and secret are required.");
        }
    } else {
        error = g_strdup("cancelled");
    }

/**
 * @brief Action git credentials.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
    graptos_widget_destroy(window);
    g_free(default_host);
    return error;
}

/**
 * @brief Prompt for Git credentials and submit them to Git's helper.
 * @details Graptoς submits secrets to Git's configured credential helper instead
 *          of storing plaintext tokens in its own config. GitHub, GitLab, and
 *          Gitea differences are handled by the remote host and helper policy.
 * @param widget The widget that emitted the credentials action.
 * @param user_data The application window passed through GTK signal data.
 *
 * Graptoς submits secrets to Git's configured credential helper instead of
 * storing plaintext tokens in its own config.  GitHub/GitLab/Gitea differences
 * are handled by the remote host and helper policy.
 */
void action_git_credentials(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *protocol = NULL;
    char *host = NULL;
    char *username = NULL;
    char *secret = NULL;
    char *helper = NULL;
    char *err = credential_dialog(win, &protocol, &host, &username, &secret, &helper);
    if (err) {
        if (g_strcmp0(err, "cancelled") != 0) {
            app_window_set_error_status(win, "Git credentials", err);
        }
        g_free(err);
        g_free(protocol); g_free(host); g_free(username); g_free(secret); g_free(helper);
        return;
    }

    if (helper && helper[0]) {
        GraptosGitResult cfg;
        gboolean ok = run_git_args(NULL, NULL, &cfg, "config", "--global", "credential.helper", helper, NULL);
        if (!ok) {
            show_git_error(win, "Git Credential Helper", &cfg);
            git_result_clear(&cfg);
            g_free(protocol); g_free(host); g_free(username); g_free(secret); g_free(helper);
            return;
        }
        git_result_clear(&cfg);
    }

    char *input = g_strdup_printf("protocol=%s\nhost=%s\nusername=%s\npassword=%s\n\n",
                                  protocol, host, username, secret);
    const char *argv[] = { "git", "credential", "approve", NULL };
    GraptosGitResult result;
    gboolean ok = run_argv(argv, input, &result);
    if (!ok) show_git_error(win, "Git Credentials", &result);
    else dialog_info(app_window_gtk(win), "Git credentials saved", "Credentials were submitted to Git's configured credential helper.");
    git_result_clear(&result);

    g_free(input);
    g_free(protocol); g_free(host); g_free(username); g_free(secret); g_free(helper);
}

/**
 * @brief Action git refresh.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_refresh(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    graptos_git_refresh_and_rebuild(win);
    app_window_set_status(win, "Git status refreshed.");
}

/**
 * @brief Action git run.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_git_run(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    char *repo = current_repo(win, NULL);
    if (!repo) {
        app_window_set_error_status(win, "Git command failed",
                                    "No Git repository is active.");
        return;
    }

    char *args = dialog_prompt_text(app_window_gtk(win), "Run Git Command", "Arguments after `git` (example: log --oneline -10):", "status --short --branch");
    if (!args) {
        g_free(repo);
        return;
    }
    g_strstrip(args);
    if (args[0] == '\0') {
        g_free(args);
        g_free(repo);
        return;
    }

    int argc = 0;
    char **parsed = NULL;
    GError *parse_error = NULL;
    if (!g_shell_parse_argv(args, &argc, &parsed, &parse_error)) {
        app_window_set_error_status(win, "Git command parse failed",
                                    parse_error ? parse_error->message : "Could not parse arguments.");
        g_clear_error(&parse_error);
        g_free(args);
        g_free(repo);
        return;
    }

    GPtrArray *argv = git_argv_new(repo);
    for (int i = 0; i < argc; i++) argv_add(argv, parsed[i]);
    g_ptr_array_add(argv, NULL);

    GraptosGitResult result;
    gboolean ok = run_argv((const char * const *)argv->pdata, NULL, &result);
    show_result_or_error(win, "Git Command", &result, ok);
    if (ok) graptos_git_refresh_and_rebuild(win);
    git_result_clear(&result);

    g_ptr_array_free(argv, TRUE);
    g_strfreev(parsed);
    g_free(args);
    g_free(repo);
}
