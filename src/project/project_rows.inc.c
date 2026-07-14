/**
 * @file src/project/project_rows.inc.c
 * @brief Cleaf project rows module.
 */

static GtkWidget *project_icon_widget_for_path(const char *path,
                                                gboolean is_dir) {
    GPtrArray *candidates = project_icon_candidates_for_path(path, is_dir);
    if (!candidates || candidates->len == 0u) {
        if (candidates) g_ptr_array_free(candidates, TRUE);
        return NULL;
    }

    GtkWidget *icon = gtk_picture_new();
    gtk_widget_add_css_class(icon, "cleaf-project-icon");
    gtk_widget_set_size_request(icon, 18, 18);
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_picture_set_can_shrink(GTK_PICTURE(icon), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(icon), GTK_CONTENT_FIT_CONTAIN);

    GdkDisplay *display = gdk_display_get_default();
    GtkIconTheme *theme = display ? gtk_icon_theme_get_for_display(display) : NULL;
    if (!theme) {
        g_ptr_array_free(candidates, TRUE);
        return icon;
    }

    const char *primary = g_ptr_array_index(candidates, 0u);
    const char **fallbacks = NULL;
    if (candidates->len > 1u) {
        fallbacks = g_new0(const char *, candidates->len);
        for (guint i = 1u; i < candidates->len; i++) {
            fallbacks[i - 1u] = g_ptr_array_index(candidates, i);
        }
    }

    GtkIconPaintable *paintable = gtk_icon_theme_lookup_icon(
        theme,
        primary,
        fallbacks,
        16,
        1,
        GTK_TEXT_DIR_NONE,
        GTK_ICON_LOOKUP_PRELOAD);

    if (paintable) {
        gtk_picture_set_paintable(GTK_PICTURE(icon), GDK_PAINTABLE(paintable));
        g_object_unref(paintable);
    }

    g_free(fallbacks);
    g_ptr_array_free(candidates, TRUE);
    return icon;
}

/**
 * @brief Git status css class.
 */
static const char *git_status_css_class(const char *status) {
    if (!status || status[0] == '\0') return NULL;
    switch (status[0]) {
    case '?': return "cleaf-git-status-untracked";
    case '!': return "cleaf-git-status-conflict";
    case 'R': return "cleaf-git-status-renamed";
    case 'D': return "cleaf-git-status-deleted";
    case 'A': return "cleaf-git-status-added";
    case 'S': return "cleaf-git-status-staged";
    case 'M': return "cleaf-git-status-modified";
    default: return NULL;
    }
}


/**
 * @brief Append project row.
 */
static void append_project_row(ProjectBuild *build,
                               const char *path,
                               guint depth) {
    if (!build || !build->win || !build->win->project_list || !path) return;
    if (build->nodes >= CLEAF_PROJECT_MAX_NODES) return;

    gboolean is_dir = g_file_test(path, G_FILE_TEST_IS_DIR);
    gboolean expanded = is_dir && project_path_is_expanded(build->win, path);
    gboolean locked = !is_dir && app_window_is_file_locked(build->win, path);
    char *base = g_filename_display_basename(path);
    const char *badge = syntax_icon_for_path(build->win ? build->win->syntaxes : NULL,
                                             path, is_dir);
    gboolean show_badge = !is_dir && badge && badge[0] != '\0'
        && g_ascii_strcasecmp(badge, "mime") != 0
        && g_ascii_strcasecmp(badge, "auto") != 0;

    GtkWidget *row = gtk_list_box_row_new();
    ProjectRow *data = g_new0(ProjectRow, 1);
    data->path = g_strdup(path);
    data->is_dir = is_dir;
    g_object_set_data_full(G_OBJECT(row), "cleaf-project-row", data,
                           project_row_free);

    GtkGesture *right_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click),
                                  GDK_BUTTON_SECONDARY);
    g_signal_connect(right_click, "pressed",
                     G_CALLBACK(on_project_row_right_click), build->win);
    gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(right_click));

    GtkWidget *line = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(line, (int)(6u + depth * 14u));
    gtk_widget_set_margin_end(line, 6);
    gtk_widget_set_margin_top(line, 2);
    gtk_widget_set_margin_bottom(line, 2);

    GtkWidget *arrow = row_label(is_dir ? (expanded ? "▾" : "▸") : "", NULL);
    gtk_widget_set_size_request(arrow, 14, -1);
    gtk_box_append(GTK_BOX(line), arrow);

    GtkWidget *icon = show_badge ? NULL : project_icon_widget_for_path(path, is_dir);
    if (icon) {
        gtk_box_append(GTK_BOX(line), icon);
    } else if (show_badge) {
        GtkWidget *badge_label = row_label(badge, "cleaf-project-badge");
        gtk_widget_set_size_request(badge_label, 36, -1);
        gtk_box_append(GTK_BOX(line), badge_label);
    }

    if (!is_dir) {
        const char *git_status = cleaf_git_status_for_file(build->win, path);
        if (git_status && git_status[0] != '\0') {
            GtkWidget *git = row_label(git_status, "cleaf-git-status");
            const char *status_class = git_status_css_class(git_status);
            if (status_class) gtk_widget_add_css_class(git, status_class);
            gtk_widget_set_size_request(git, 18, -1);
            gtk_box_append(GTK_BOX(line), git);
        }
    }

    if (locked) {
        GtkWidget *lock = row_label("🔒", "cleaf-project-lock");
        gtk_widget_set_size_request(lock, 18, -1);
        gtk_box_append(GTK_BOX(line), lock);
    }

    GtkWidget *name = row_label(base ? base : path, NULL);
    gtk_widget_set_hexpand(name, TRUE);
    gtk_box_append(GTK_BOX(line), name);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), line);
    gtk_list_box_append(GTK_LIST_BOX(build->win->project_list), row);

    build->nodes++;
    g_free(base);
}

/**
 * @brief Add visible path.
 */
static void add_visible_path(ProjectBuild *build,
                             const char *path,
                             guint depth) {
    if (!build || !path) return;
    if (build->nodes >= CLEAF_PROJECT_MAX_NODES
        || depth > CLEAF_PROJECT_MAX_DEPTH) {
        return;
    }

    append_project_row(build, path, depth);
    if (!g_file_test(path, G_FILE_TEST_IS_DIR)) return;
    if (!project_path_is_expanded(build->win, path)) return;

    GPtrArray *names = sorted_dir_names(path);
    if (!names) return;

    for (guint i = 0u; i < names->len; i++) {
        char *child = g_build_filename(path, g_ptr_array_index(names, i),
                                       NULL);
        add_visible_path(build, child, depth + 1u);
        g_free(child);
        if (build->nodes >= CLEAF_PROJECT_MAX_NODES) break;
    }
    g_ptr_array_free(names, TRUE);
}

/**
 * @brief Project root count.
 */
guint project_root_count(EditorWindow *win) {
    if (!win || !win->project_roots) return 0u;
    return win->project_roots->len;
}

/**
 * @brief Project root at.
 */
const char *project_root_at(EditorWindow *win, guint index) {
    if (!win || !win->project_roots || index >= win->project_roots->len) return NULL;
    return g_ptr_array_index(win->project_roots, index);
}

/**
 * @brief Project has roots.
 */
gboolean project_has_roots(EditorWindow *win) {
    return project_root_count(win) > 0u;
}

/**
 * @brief Project root for path.
 */
const char *project_root_for_path(EditorWindow *win, const char *path) {
    if (!win || !path) return NULL;
    const char *best = NULL;
    gsize best_len = 0u;
    guint count = project_root_count(win);
    for (guint i = 0u; i < count; i++) {
        const char *root = project_root_at(win, i);
        if (!root) continue;
        gsize len = strlen(root);
        if (len <= best_len) continue;
        if (g_strcmp0(path, root) == 0
            || (g_str_has_prefix(path, root)
                && path[len] == G_DIR_SEPARATOR)) {
            best = root;
            best_len = len;
        }
    }
    if (!best && win->project_root && g_str_has_prefix(path, win->project_root)) {
        best = win->project_root;
    }
    return best;
}

/**
 * @brief Project root exists.
 */
static gboolean project_root_exists(EditorWindow *win, const char *canonical) {
    if (!canonical) return FALSE;
    guint count = project_root_count(win);
    for (guint i = 0u; i < count; i++) {
        if (g_strcmp0(project_root_at(win, i), canonical) == 0) return TRUE;
    }
    return FALSE;
}
