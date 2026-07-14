/**
 * @file src/app/app_file_search_actions.inc.c
 * @brief Cleaf app file search actions module.
 */

void set_search_panel(EditorWindow *win, gboolean visible, gboolean replace_mode) {
    if (!win || !win->search_revealer) return;
    /* Find and Replace as one panel.
    * This fixed an issue when creating a second dialog. Replace now exposes the extra widget
    */
    win->replace_mode = replace_mode;
    gtk_widget_set_visible(win->replace_label, replace_mode);
    gtk_widget_set_visible(win->replace_entry, replace_mode);
    gtk_widget_set_visible(win->replace_button, replace_mode);
    gtk_widget_set_visible(win->replace_all_button, replace_mode);
    gtk_revealer_set_reveal_child(GTK_REVEALER(win->search_revealer), visible);
    if (visible) {
        if (replace_mode) gtk_widget_grab_focus(win->replace_entry);
        else gtk_widget_grab_focus(win->find_entry);
    }
}


/**
 * @brief Action new.
 */
void action_new(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    EditorTab *tab = editor_tab_new(win);
    app_window_add_tab(win, tab, TRUE);
}


/**
 * @brief Action open.
 */
void action_open(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;

    char *filename = cleaf_open_file_dialog(app_window_gtk(win), "Open File");
    if (filename) {
        (void)app_window_open_file(win, filename);
        g_free(filename);
    }
}


/**
 * @brief Action open folder.
 */
void action_open_folder(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;

    char *folder = cleaf_select_folder_dialog(app_window_gtk(win),
                                              "Open Project Folder");
    if (!folder) return;

    project_tree_load_folder(win, folder);
    if (win->project_revealer) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(win->project_revealer),
                                      TRUE);
    }

    char *display = g_filename_display_basename(folder);
    char *msg = g_strdup_printf("Project opened: %s",
                                display ? display : folder);
    app_window_set_status(win, msg);
    g_free(msg);
    g_free(display);
    g_free(folder);
}

/**
 * @brief Cleaf executable path.
 */
static char *cleaf_executable_path(void) {
    GError *error = NULL;
    char *path = g_file_read_link("/proc/self/exe", &error);
    if (path) return path;
    g_clear_error(&error);

    const char *prgname = g_get_prgname();
    if (prgname && prgname[0] != '\0') return g_strdup(prgname);
    return g_strdup("cleaf");
}

/**
 * @brief Action open folder new instance.
 */
void action_open_folder_new_instance(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;

    char *folder = cleaf_select_folder_dialog(app_window_gtk(win),
                                              "Open Folder in New Instance");
    if (!folder) return;

    char *exe = cleaf_executable_path();
    if (!exe) {
        dialog_error(app_window_gtk(win), "Could not launch instance",
                     "Cleaf could not determine its executable path.");
        g_free(folder);
        return;
    }

    char *argv[] = { exe, folder, NULL };
    GError *error = NULL;
    gboolean ok = g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                                NULL, NULL, NULL, &error);
    if (!ok) {
        dialog_error(app_window_gtk(win), "Could not launch instance",
                     error ? error->message : "Unknown launch error.");
        g_clear_error(&error);
    } else {
        char *display = g_filename_display_basename(folder);
        char *msg = g_strdup_printf("Opened new instance for: %s",
                                    display ? display : folder);
        app_window_set_status(win, msg);
        g_free(msg);
        g_free(display);
    }

    g_free(exe);
    g_free(folder);
}


/**
 * @brief Action save.
 */
void action_save(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = app_window_current_tab(user_data);
    if (tab) editor_tab_save(tab, FALSE);
}


/**
 * @brief Action save as.
 */
void action_save_as(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = app_window_current_tab(user_data);
    if (tab) editor_tab_save(tab, TRUE);
}


/**
 * @brief Action show find.
 */
void action_show_find(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    set_search_panel(user_data, TRUE, FALSE);
}


/**
 * @brief Action show replace.
 */
void action_show_replace(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    set_search_panel(user_data, TRUE, TRUE);
}


/**
 * @brief Action hide search.
 */
void action_hide_search(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    set_search_panel(user_data, FALSE, FALSE);
}


/**
 * @brief Action find next.
 */
void action_find_next(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    EditorTab *tab = app_window_current_tab(win);
    const char *query = gtk_entry_get_text(GTK_ENTRY(win->find_entry));
    if (tab) editor_tab_find(tab, query, FALSE);
}


/**
 * @brief Action find prev.
 */
void action_find_prev(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    EditorTab *tab = app_window_current_tab(win);
    const char *query = gtk_entry_get_text(GTK_ENTRY(win->find_entry));
    if (tab) editor_tab_find(tab, query, TRUE);
}


/**
 * @brief Action replace.
 */
void action_replace(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    EditorTab *tab = app_window_current_tab(win);
    const char *find = gtk_entry_get_text(GTK_ENTRY(win->find_entry));
    const char *replace = gtk_entry_get_text(GTK_ENTRY(win->replace_entry));
    if (tab) editor_tab_replace_current(tab, find, replace);
}


/**
 * @brief Action replace all.
 */
void action_replace_all(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    EditorTab *tab = app_window_current_tab(win);
    const char *find = gtk_entry_get_text(GTK_ENTRY(win->find_entry));
    const char *replace = gtk_entry_get_text(GTK_ENTRY(win->replace_entry));
    if (tab) editor_tab_replace_all(tab, find, replace);
}


/**
 * @brief Action comment.
 */
void action_comment(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = app_window_current_tab(user_data);
    if (tab) editor_tab_toggle_comment(tab);
}


/**
 * @brief Action reload syntax.
 */
void action_reload_syntax(GtkWidget *widget, gpointer user_data) {
    // Syntax definitions are runtime files, so reload them without restarting the editor
    (void)widget;
    app_window_reload_syntaxes(user_data);
}


/**
 * @brief Action syntax diagnostics.
 */
void action_syntax_diagnostics(GtkWidget *widget, gpointer user_data) {
    //Should add functional diagnostics. This is just for show.
    (void)widget;
    EditorWindow *win = user_data;
    char *diag = syntax_diagnostics(win->syntaxes);
    dialog_info(app_window_gtk(win), "Syntax Diagnostics", diag);
    g_free(diag);
}


