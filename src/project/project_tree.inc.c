/**
 * @file src/project/project_tree.inc.c
 * @brief Graptoς project tree module.
 * @details Projects change underneath the editor. We keep tree rows, filesystem context,
 *          and search helpers away from EditorTab so directory updates do not become
 *          buffer-management problems.
 */

typedef struct {
    GtkWidget *scrolled; /**< Scrolled. */
    gdouble value; /**< Value. */
} ProjectScrollRestore;

/**
 * @brief Clear active project tree filesystem monitors.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 */
static void project_tree_clear_monitors(EditorWindow *win) {
    if (!win || !win->project_monitors) return;
    g_hash_table_remove_all(win->project_monitors);
}

/**
 * @brief Refresh the project tree after a debounced filesystem event.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean project_tree_refresh_timeout_cb(gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win) return G_SOURCE_REMOVE;

    win->project_refresh_timeout = 0u;
    graptos_git_refresh_all(win);
    project_tree_rebuild(win);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Schedule a debounced live project tree refresh.
 * @details File monitors can fire several events for one save or copy. We batch
 *          them into one refresh so the tree stays live without rebuilding
 *          repeatedly while the filesystem is still settling.
 * @param win The win supplied by the caller.
 */
static void project_tree_schedule_refresh(EditorWindow *win) {
    if (!win || win->project_refresh_timeout != 0u) return;
    win->project_refresh_timeout = g_timeout_add(150u,
                                                 project_tree_refresh_timeout_cb,
                                                 win);
}

/**
 * @brief Handle filesystem monitor events for watched project directories.
 * @details Only structural events matter to the tree. Content-only noise can be
 *          ignored, but creates, deletes, renames, and moves need a rebuild so
 *          files appear without closing and reopening the folder.
 * @param monitor The monitor supplied by the caller.
 * @param file The file object supplied by GTK or GIO.
 * @param other_file The other file supplied by the caller.
 * @param event_type The event type supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void project_tree_monitor_changed(GFileMonitor *monitor,
                                         GFile *file,
                                         GFile *other_file,
                                         GFileMonitorEvent event_type,
                                         gpointer user_data) {
    (void)monitor;
    (void)file;
    (void)other_file;

    EditorWindow *win = user_data;
    switch (event_type) {
    case G_FILE_MONITOR_EVENT_CHANGED:
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_DELETED:
    case G_FILE_MONITOR_EVENT_CREATED:
    case G_FILE_MONITOR_EVENT_MOVED:
    case G_FILE_MONITOR_EVENT_RENAMED:
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_MOVED_OUT:
        project_tree_schedule_refresh(win);
        break;
    default:
        break;
    }
}

/**
 * @brief Watch a visible project directory for live tree refreshes.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 */
static void project_tree_watch_directory(EditorWindow *win, const char *path) {
    if (!win || !path || !g_file_test(path, G_FILE_TEST_IS_DIR)) return;

    if (!win->project_monitors) {
        win->project_monitors = g_hash_table_new_full(g_str_hash,
                                                      g_str_equal,
                                                      g_free,
                                                      g_object_unref);
    }

    char *canonical = g_canonicalize_filename(path, NULL);
    if (!canonical) return;
    if (g_hash_table_contains(win->project_monitors, canonical)) {
        g_free(canonical);
        return;
    }

    GFile *file = g_file_new_for_path(canonical);
    GError *error = NULL;
    GFileMonitor *monitor = g_file_monitor_directory(file,
                                                     G_FILE_MONITOR_WATCH_MOVES,
                                                     NULL,
                                                     &error);
    g_object_unref(file);

    if (!monitor) {
        g_clear_error(&error);
        g_free(canonical);
        return;
    }

    g_signal_connect(monitor, "changed",
                     G_CALLBACK(project_tree_monitor_changed), win);
    g_hash_table_replace(win->project_monitors, canonical, monitor);
}

/**
 * @brief Project tree scrolled window.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *project_tree_scrolled_window(EditorWindow *win) {
    GtkWidget *widget = win ? win->project_list : NULL;
    while (widget) {
        if (GTK_IS_SCROLLED_WINDOW(widget)) return widget;
        widget = gtk_widget_get_parent(widget);
    }
    return NULL;
}

/**
 * @brief Project restore scroll cb.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean project_restore_scroll_cb(gpointer user_data) {
    ProjectScrollRestore *restore = user_data;
    if (!restore || !restore->scrolled) {
        g_free(restore);
        return G_SOURCE_REMOVE;
    }
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(restore->scrolled));
    if (adj) {
        gdouble max = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
        if (max < 0.0) max = 0.0;
        gtk_adjustment_set_value(adj, CLAMP(restore->value, 0.0, max));
    }
    g_object_unref(restore->scrolled);
    g_free(restore);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Project restore scroll later.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param scrolled The scrolled supplied by the caller.
 * @param value The value being parsed, stored, or applied.
 */
static void project_restore_scroll_later(GtkWidget *scrolled, gdouble value) {
    if (!scrolled) return;
    ProjectScrollRestore *restore = g_new0(ProjectScrollRestore, 1);
    restore->scrolled = g_object_ref(scrolled);
    restore->value = value;
    g_idle_add_full(G_PRIORITY_LOW, project_restore_scroll_cb, restore, NULL);
}

/**
 * @brief Project tree rebuild.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 */
static void project_tree_rebuild(EditorWindow *win) {
    if (!win || !win->project_list) return;
    project_tree_clear_monitors(win);

    GtkWidget *scrolled = project_tree_scrolled_window(win);
    GtkAdjustment *adj = scrolled
        ? gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled))
        : NULL;
    gdouble scroll_value = adj ? gtk_adjustment_get_value(adj) : 0.0;

    project_context_popover_close_for_list(win);

    GtkWidget *row = gtk_widget_get_first_child(win->project_list);
    while (row) {
        GtkWidget *next = gtk_widget_get_next_sibling(row);
        gtk_list_box_remove(GTK_LIST_BOX(win->project_list), row);
        row = next;
    }

    ProjectBuild build;
    build.win = win;
    build.nodes = 0u;

    guint count = project_root_count(win);
    for (guint i = 0u; i < count && build.nodes < GRAPTOS_PROJECT_MAX_NODES; i++) {
        const char *root = project_root_at(win, i);
        if (root) add_visible_path(&build, root, 0u);
    }

    project_restore_scroll_later(scrolled, scroll_value);
}

/**
 * @brief On project row activated.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param list_box The list box supplied by the caller.
 * @param row The row supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void on_project_row_activated(GtkListBox *list_box,
                                     GtkListBoxRow *row,
                                     gpointer user_data) {
    (void)list_box;
    EditorWindow *win = user_data;
    if (!win || !row) return;

    ProjectRow *data = g_object_get_data(G_OBJECT(row), "graptos-project-row");
    if (!data || !data->path) return;

    if (data->is_dir) {
        gboolean expanded = project_path_is_expanded(win, data->path);
        project_set_expanded(win, data->path, !expanded);
        project_tree_rebuild(win);
        return;
    }

    (void)app_window_open_file(win, data->path);
}

/**
 * @brief Section label.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *section_label(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_start(label, 16);
    gtk_widget_set_margin_top(label, 14);
    gtk_widget_set_margin_bottom(label, 6);
    gtk_widget_add_css_class(label, "graptos-project-section");
    return label;
}

/**
 * @brief Project tree create.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *project_tree_create(EditorWindow *win) {
    if (!win) return NULL;

    if (!win->project_expanded) {
        win->project_expanded = g_hash_table_new_full(g_str_hash,
                                                      g_str_equal,
                                                      g_free,
                                                      NULL);
    }

    win->project_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(win->project_list),
                                    GTK_SELECTION_NONE);
    gtk_widget_set_size_request(win->project_list, 250, -1);
    gtk_widget_add_css_class(win->project_list, "graptos-project-tree");
    g_signal_connect(win->project_list, "row-activated",
                     G_CALLBACK(on_project_row_activated), win);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  win->project_list);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "graptos-project-pane");
    gtk_box_append(GTK_BOX(box), section_label("FOLDERS"));
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(box), scrolled);
    return box;
}

/**
 * @brief Project tree close context.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 */
void project_tree_close_context(EditorWindow *win) {
    project_context_popover_close_for_list(win);
}

/**
 * @brief Project tree refresh.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 */
void project_tree_refresh(EditorWindow *win) {
    project_tree_rebuild(win);
}

/**
 * @brief Project tree clear.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 */
void project_tree_clear(EditorWindow *win) {
    if (!win) return;
    project_context_popover_close_for_list(win);
    graptos_source_cancel(&win->project_refresh_timeout);
    project_tree_clear_monitors(win);
    if (win->project_expanded) g_hash_table_remove_all(win->project_expanded);
    if (win->project_roots) g_ptr_array_set_size(win->project_roots, 0u);
    g_clear_pointer(&win->project_root, g_free);
    project_tree_rebuild(win);
}

/**
 * @brief Project tree load folder.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @param folder_path The folder path supplied by the caller.
 */
void project_tree_load_folder(EditorWindow *win, const char *folder_path) {
    if (!win || !folder_path
        || !g_file_test(folder_path, G_FILE_TEST_IS_DIR)) {
        return;
    }

    if (!win->project_roots) {
        win->project_roots = g_ptr_array_new_with_free_func(g_free);
    }

    char *canonical = g_canonicalize_filename(folder_path, NULL);
    if (!canonical) return;

    if (!project_root_exists(win, canonical)) {
        g_ptr_array_add(win->project_roots, g_strdup(canonical));
    }

    g_free(win->project_root);
    win->project_root = g_strdup(canonical);
    project_set_expanded(win, canonical, TRUE);
    graptos_git_refresh_all(win);
    project_tree_rebuild(win);
    app_window_reload_syntaxes(win);
    g_free(canonical);
}
