/**
 * @file src/project/project_context.inc.c
 * @brief Cleaf project context module.
 */

static gboolean project_path_is_expanded(EditorWindow *win, const char *path) {
    return win && win->project_expanded && path
        && g_hash_table_contains(win->project_expanded, path);
}

/**
 * @brief Project set expanded.
 */
static void project_set_expanded(EditorWindow *win,
                                 const char *path,
                                 gboolean expanded) {
    if (!win || !path) return;
    if (!win->project_expanded) {
        win->project_expanded = g_hash_table_new_full(g_str_hash,
                                                      g_str_equal,
                                                      g_free,
                                                      NULL);
    }

    if (expanded) {
        g_hash_table_replace(win->project_expanded, g_strdup(path),
                             GINT_TO_POINTER(TRUE));
    } else {
        g_hash_table_remove(win->project_expanded, path);
    }
}

/**
 * @brief Row label.
 */
static GtkWidget *row_label(const char *text, const char *css_class) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
    if (css_class) gtk_widget_add_css_class(label, css_class);
    return label;
}

/**
 * @brief Close project folder for path.
 */
static void close_project_folder_for_path(EditorWindow *win, const char *path) {
    if (!win || !win->project_roots || !path) return;

    const char *root = project_root_for_path(win, path);
    if (!root) return;
    char *root_copy = g_strdup(root);

    for (guint i = 0u; i < win->project_roots->len; i++) {
        const char *candidate = g_ptr_array_index(win->project_roots, i);
        if (g_strcmp0(candidate, root_copy) == 0) {
            g_ptr_array_remove_index(win->project_roots, i);
            break;
        }
    }

    if (win->project_expanded) g_hash_table_remove(win->project_expanded, root_copy);
    if (g_strcmp0(win->project_root, root_copy) == 0) {
        g_free(win->project_root);
        win->project_root = win->project_roots->len > 0u
            ? g_strdup(g_ptr_array_index(win->project_roots,
                                         win->project_roots->len - 1u))
            : NULL;
    }

    project_tree_rebuild(win);
    if (win->project_roots->len == 0u && win->project_revealer) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(win->project_revealer),
                                      FALSE);
    }

    char *base = g_filename_display_basename(root_copy);
    char *msg = g_strdup_printf("Closed folder: %s", base ? base : root_copy);
    app_window_set_status(win, msg);
    g_free(msg);
    g_free(base);
    g_free(root_copy);
}

/**
 * @brief Project context open.
 */
static void project_context_open(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ProjectAction *action = user_data;
    if (!action || !action->win || !action->path) return;

    if (action->is_dir) {
        gboolean expanded = project_path_is_expanded(action->win, action->path);
        project_set_expanded(action->win, action->path, !expanded);
        project_tree_rebuild(action->win);
        return;
    }

    (void)app_window_open_file(action->win, action->path);
}

/**
 * @brief Project context close folder.
 */
static void project_context_close_folder(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ProjectAction *action = user_data;
    if (!action) return;
    close_project_folder_for_path(action->win, action->path);
}

/**
 * @brief Project context toggle lock.
 */
static void project_context_toggle_lock(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ProjectAction *action = user_data;
    if (!action || !action->win || !action->path || action->is_dir) return;

    gboolean locked = !app_window_is_file_locked(action->win, action->path);
    app_window_set_file_locked(action->win, action->path, locked);
    project_tree_rebuild(action->win);

    char *base = g_filename_display_basename(action->path);
    char *msg = g_strdup_printf("%s editing: %s",
                                locked ? "Locked" : "Unlocked",
                                base ? base : action->path);
    app_window_set_status(action->win, msg);
    g_free(msg);
    g_free(base);
}

/**
 * @brief Invalid new name.
 */
static gboolean invalid_new_name(const char *name) {
    if (!name || name[0] == '\0') return TRUE;
    if (g_strcmp0(name, ".") == 0 || g_strcmp0(name, "..") == 0) return TRUE;
    return strchr(name, G_DIR_SEPARATOR) != NULL;
}

/**
 * @brief Project context rename.
 */
static void project_context_rename(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ProjectAction *action = user_data;
    if (!action || !action->win || !action->path) return;

    g_autofree char *old_base = g_path_get_basename(action->path);
    g_autofree char *new_name = dialog_prompt_text(app_window_gtk(action->win),
                                                   "Rename",
                                                   "New name:",
                                                   old_base);
    if (!new_name) return;
    g_strstrip(new_name);

    if (invalid_new_name(new_name)) {
        app_window_set_error_status(action->win, "Invalid name",
                                    "Use a plain file or folder name, not a path.");
        return;
    }

    if (g_strcmp0(new_name, old_base) == 0) {
        return;
    }

    g_autofree char *dir = g_path_get_dirname(action->path);
    g_autofree char *target = g_build_filename(dir, new_name, NULL);

    if (g_file_test(target, G_FILE_TEST_EXISTS)) {
        app_window_set_error_status(action->win, "Rename failed",
                                    "A file or folder with that name already exists.");
        return;
    }

    if (g_rename(action->path, target) != 0) {
        app_window_set_error_status(action->win, "Rename failed",
                                    g_strerror(errno));
        return;
    }

    gboolean was_expanded = action->is_dir
        && project_path_is_expanded(action->win, action->path);
    if (was_expanded) project_set_expanded(action->win, target, TRUE);
    if (action->win->project_expanded) {
        g_hash_table_remove(action->win->project_expanded, action->path);
    }

    app_window_note_path_renamed(action->win, action->path, target);
    project_tree_rebuild(action->win);

    g_autofree char *msg = g_strdup_printf("Renamed %s to %s", old_base, new_name);
    app_window_set_status(action->win, msg);
}

/**
 * @brief Project context button.
 */
static GtkWidget *project_context_button(const char *label,
                                         GCallback callback,
                                         EditorWindow *win,
                                         const char *path,
                                         gboolean is_dir) {
    GtkWidget *button = cleaf_flat_button_new(label, NULL, NULL, NULL);
    ProjectAction *action = project_action_new(win, path, is_dir);
    g_object_set_data_full(G_OBJECT(button), "cleaf-project-action", action,
                           project_action_free);
    g_signal_connect(button, "clicked", callback, action);
    return button;
}

/**
 * @brief Project context popover closed.
 */
static void project_context_popover_closed(GtkPopover *popover,
                                           gpointer user_data) {
    GtkWidget *owner = user_data;
    if (owner) {
        GtkWidget *stored = g_object_get_data(G_OBJECT(owner),
                                             "cleaf-project-context-popover");
        if (stored == GTK_WIDGET(popover)) {
            g_object_set_data(G_OBJECT(owner),
                              "cleaf-project-context-popover", NULL);
        }
    }
    cleaf_widget_destroy(GTK_WIDGET(popover));
}

/**
 * @brief Project context popover close for list.
 */
static void project_context_popover_close_for_list(EditorWindow *win) {
    if (!win || !win->project_list) return;
    GtkWidget *old_popover = g_object_get_data(G_OBJECT(win->project_list),
                                               "cleaf-project-context-popover");
    if (old_popover && GTK_IS_POPOVER(old_popover)) {
        cleaf_popover_hide(old_popover);
        cleaf_widget_destroy(old_popover);
        g_object_set_data(G_OBJECT(win->project_list),
                          "cleaf-project-context-popover", NULL);
    }
}

/**
 * @brief On project row right click.
 */
static void on_project_row_right_click(GtkGestureClick *gesture,
                                       int n_press,
                                       double x,
                                       double y,
                                       gpointer user_data) {
    (void)n_press;
    (void)x;
    (void)y;
    GtkWidget *row = gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture));
    EditorWindow *win = user_data;
    if (!win || !row) return;

    ProjectRow *data = g_object_get_data(G_OBJECT(row), "cleaf-project-row");
    if (!data || !data->path) return;

    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);

    project_context_popover_close_for_list(win);

    GtkWidget *popover = gtk_popover_new();
    cleaf_popover_attach(popover, win->project_list);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "cleaf-context-popover");
    g_object_set_data(G_OBJECT(win->project_list),
                      "cleaf-project-context-popover", popover);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    cleaf_set_all_margins(box, 6);
    gtk_box_append(GTK_BOX(box),
                   project_context_button(data->is_dir ? "Expand / Collapse" : "Open",
                                          G_CALLBACK(project_context_open),
                                          win, data->path, data->is_dir));

    if (!data->is_dir) {
        gboolean locked = app_window_is_file_locked(win, data->path);
        gtk_box_append(GTK_BOX(box),
                       project_context_button(locked ? "Unlock Editing" : "Lock Editing",
                                              G_CALLBACK(project_context_toggle_lock),
                                              win, data->path, data->is_dir));
    }

    gtk_box_append(GTK_BOX(box),
                   project_context_button("Rename",
                                          G_CALLBACK(project_context_rename),
                                          win, data->path, data->is_dir));
    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    gtk_box_append(GTK_BOX(box),
                   project_context_button("Close Folder",
                                          G_CALLBACK(project_context_close_folder),
                                          win, data->path, data->is_dir));

    graphene_point_t source = GRAPHENE_POINT_INIT((float)x, (float)y);
    graphene_point_t target = GRAPHENE_POINT_INIT((float)x, (float)y);
    if (!gtk_widget_compute_point(row, win->project_list, &source, &target)) {
        target.x = (float)x;
        target.y = (float)y;
    }
    GdkRectangle rect = { (int)target.x, (int)target.y, 1, 1 };
    gtk_popover_set_child(GTK_POPOVER(popover), box);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    /*
     * The context popover is parented to the project list rather than a row so
     * rebuilding the tree cannot finalize a row that still owns a GtkPopover.
     */
    g_signal_connect(popover, "closed",
                     G_CALLBACK(project_context_popover_closed),
                     win->project_list);
    cleaf_popover_show(popover);
}
