static void project_tree_rebuild(EditorWindow *win) {
    if (!win || !win->project_list) return;

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
    for (guint i = 0u; i < count && build.nodes < CLEAF_PROJECT_MAX_NODES; i++) {
        const char *root = project_root_at(win, i);
        if (root) add_visible_path(&build, root, 0u);
    }
}

static void on_project_row_activated(GtkListBox *list_box,
                                     GtkListBoxRow *row,
                                     gpointer user_data) {
    (void)list_box;
    EditorWindow *win = user_data;
    if (!win || !row) return;

    ProjectRow *data = g_object_get_data(G_OBJECT(row), "cleaf-project-row");
    if (!data || !data->path) return;

    if (data->is_dir) {
        gboolean expanded = project_path_is_expanded(win, data->path);
        project_set_expanded(win, data->path, !expanded);
        project_tree_rebuild(win);
        return;
    }

    (void)app_window_open_file(win, data->path);
}

static GtkWidget *section_label(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_start(label, 16);
    gtk_widget_set_margin_top(label, 14);
    gtk_widget_set_margin_bottom(label, 6);
    gtk_widget_add_css_class(label, "cleaf-project-section");
    return label;
}

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
    gtk_widget_add_css_class(win->project_list, "cleaf-project-tree");
    g_signal_connect(win->project_list, "row-activated",
                     G_CALLBACK(on_project_row_activated), win);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  win->project_list);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(box, "cleaf-project-pane");
    gtk_box_append(GTK_BOX(box), section_label("FOLDERS"));
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_box_append(GTK_BOX(box), scrolled);
    return box;
}

void project_tree_close_context(EditorWindow *win) {
    project_context_popover_close_for_list(win);
}

void project_tree_refresh(EditorWindow *win) {
    project_tree_rebuild(win);
}

void project_tree_clear(EditorWindow *win) {
    if (!win) return;
    project_context_popover_close_for_list(win);
    if (win->project_expanded) g_hash_table_remove_all(win->project_expanded);
    if (win->project_roots) g_ptr_array_set_size(win->project_roots, 0u);
    g_clear_pointer(&win->project_root, g_free);
    project_tree_rebuild(win);
}

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
    cleaf_git_refresh_all(win);
    project_tree_rebuild(win);
    g_free(canonical);
}
