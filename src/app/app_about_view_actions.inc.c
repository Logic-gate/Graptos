/**
 * @file src/app/app_about_view_actions.inc.c
 * @brief Cleaf app about view actions module.
 */

GtkWidget *pref_section(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "cleaf-menu-title");
    return label;
}

/**
 * @brief Pref tab label.
 */
GtkWidget *pref_tab_label(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    // Perhaps this should be in config or part of the themeing engine.
    gtk_widget_set_margin_start(label, 14);
    gtk_widget_set_margin_end(label, 14);
    gtk_widget_set_margin_top(label, 6);
    gtk_widget_set_margin_bottom(label, 6);
    gtk_widget_add_css_class(label, "cleaf-pref-tab");
    return label;
}


/**
 * @brief Cleaf prefers dark theme.
 */
static gboolean cleaf_prefers_dark_theme(void) {
    GtkSettings *settings = gtk_settings_get_default();
    gboolean dark = FALSE;
    
   //  Default to light logo when settings are unavailable
    if (settings) {
        g_object_get(settings, "gtk-application-prefer-dark-theme", &dark, NULL);
    }
    return dark;
}

/**
 * @brief Cleaf logo path for theme.
 */
static char *cleaf_logo_path_for_theme(gboolean dark) {
    const char *filename = dark ? "cleaf-logo-dark.png" : "cleaf-logo-light.png";

    char *installed = g_build_filename(DATADIR, "logos", filename, NULL);
    if (installed && g_file_test(installed, G_FILE_TEST_EXISTS)) return installed;
    g_free(installed);

    char *local = g_build_filename("data", "logos", filename, NULL);
    if (local && g_file_test(local, G_FILE_TEST_EXISTS)) return local;
    g_free(local);

    return NULL;
}

/**
 * @brief Cleaf about logo new.
 */
static GtkWidget *cleaf_about_logo_new(void) {
    char *path = cleaf_logo_path_for_theme(cleaf_prefers_dark_theme());
    if (!path) return NULL;

    GtkWidget *picture = gtk_picture_new_for_filename(path);
    gtk_widget_set_size_request(picture, 170, 170);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_halign(picture, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(picture, GTK_ALIGN_CENTER);
    g_free(path);
    return picture;
}

/**
 * @brief Cleaf about label new.
 */
static GtkWidget *cleaf_about_label_new(const char *text, gboolean title) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.5f);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    if (title) gtk_widget_add_css_class(label, "cleaf-menu-title");
    return label;
}

/**
 * @brief Action about.
 */
void action_about(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    GtkWindow *parent = app_window_gtk(win);
    /* GtkWindow instead of GtkAboutDialog */
    GtkWidget *dialog = gtk_window_new();
    gtk_widget_add_css_class(dialog, "cleaf-window");
    gtk_window_set_title(GTK_WINDOW(dialog), "About Cleaf");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 440, 330);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    if (parent) gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(box, "cleaf-root");
    cleaf_set_all_margins(box, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    GtkWidget *logo = cleaf_about_logo_new();
    if (logo) gtk_box_append(GTK_BOX(box), logo);
    gtk_box_append(GTK_BOX(box), cleaf_about_label_new("Cleaf", TRUE));
    gtk_box_append(GTK_BOX(box), cleaf_about_label_new(APP_VERSION, FALSE));
    gtk_box_append(GTK_BOX(box), cleaf_about_label_new(
        "A compact C/GTK text editor with YAML syntax definitions and project-aware completion.",
        FALSE));
    gtk_box_append(GTK_BOX(box), cleaf_about_label_new(
        "This program comes with absolutely no warranty.\nLicense: AGPL-3.0 license.",
        FALSE));
    //gtk_box_append(GTK_BOX(box), cleaf_about_label_new(APP_VERSION, FALSE));
    // Destroy dialog afterwards this window is created once.
    (void)cleaf_modal_window_run(GTK_WINDOW(dialog), GTK_RESPONSE_CLOSE);
    cleaf_widget_destroy(dialog);
}


/**
 * @brief Action show preferences.
 */
void action_show_preferences(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    dialog_info(app_window_gtk(win),
                "Preferences moved",
                "Runtime editing, syntax, intelligence, and safety controls are now on the bottom bar. Appearance colours are config-only.");
}


/**
 * @brief Action toggle minimap.
 */
void action_toggle_minimap(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    // update  global window preference first, then reapply tab state and persist the change.
    win->minimap_enabled = !win->minimap_enabled;
    apply_preferences_to_all_tabs(win);
    cleaf_config_save(win);
}


/**
 * @brief Action toggle preview.
 */
void action_toggle_preview(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    win->preview_enabled = !win->preview_enabled;
    apply_preferences_to_all_tabs(win);
    cleaf_config_save(win);
}


/**
 * @brief Action render latex.
 */
void action_render_latex(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = app_window_current_tab(user_data);
    /*
     * LaTeX rendering is tab-scoped because each editor tab owns its source buffer preview state and redner output.
     */
    if (tab) editor_tab_render_latex(tab);
}
