/**
 * @file src/app/app_preference_actions.inc.c
 * @brief Cleaf app preference actions module.
 */

void set_tab_policy(EditorWindow *win, guint width, gboolean insert_spaces) {
    if (!win) return;
    if (width == 0u) width = 4u;
    if (width > 16u) width = 16u;
    win->tab_width = width;
    win->insert_spaces = insert_spaces;
    apply_tab_policy_to_all_tabs(win); //Existing tabs 
    cleaf_config_save(win);
}


/**
 * @brief Action tab spaces 2.
 */
void action_tab_spaces_2(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    set_tab_policy(user_data, 2u, TRUE);
}


/**
 * @brief Action tab spaces 4.
 */
void action_tab_spaces_4(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    set_tab_policy(user_data, 4u, TRUE);
}


/**
 * @brief Action tab spaces 8.
 */
void action_tab_spaces_8(GtkWidget *widget, gpointer user_data) {
    //I found this in Sublime, I have no idea who uses 8...propably legacy use.
    (void)widget;
    set_tab_policy(user_data, 8u, TRUE);
}


/**
 * @brief Action tab hard tabs.
 */
void action_tab_hard_tabs(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    set_tab_policy(user_data, 4u, FALSE);
}


/**
 * @brief Action toggle autocomplete.
 */
void action_toggle_autocomplete(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    // Autocomplete is applied through the same tab-policy path because editor view settings are refreshed together
    win->autocomplete_enabled = !win->autocomplete_enabled;
    apply_tab_policy_to_all_tabs(win);
    cleaf_config_save(win);
}


/**
 * @brief Action toggle autosave.
 */
void action_toggle_autosave(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    // Autosave owns a timer, so toggling it needs to restart or stop the timer instead of only updating the saved preference.
    win->auto_save_enabled = !win->auto_save_enabled;
    restart_auto_save_timer(win);
    // Refresh buttons so the bottom bar reflects the actual autosave state.
    update_policy_buttons(win);
    cleaf_config_save(win);
}


/**
 * @brief Action toggle backup.
 */
void action_toggle_backup(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    win->backup_enabled = !win->backup_enabled;
    apply_tab_policy_to_all_tabs(win);
    cleaf_config_save(win);
}


/**
 * @brief Rgba to hex.
 */
char *rgba_to_hex(const GdkRGBA *rgba) {
    if (!rgba) return g_strdup("#000000");
    /*
     * GdkRGBA stores channels as 0.0-1.0 doubles. Round to the nearest byte so
     * saved colours stay stable when written back to config.
     */
    int r = (int)(rgba->red * 255.0 + 0.5);
    int g = (int)(rgba->green * 255.0 + 0.5);
    int b = (int)(rgba->blue * 255.0 + 0.5);
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return g_strdup_printf("#%02X%02X%02X", r, g, b);
}


/**
 * @brief Choose color for slot.
 */
void choose_color_for_slot(EditorWindow *win,
                           GtkWidget *parent_widget,
                           const char *title,
                           char **slot) {
    (void)parent_widget;
    if (!win || !slot) return;

    char *value = dialog_prompt_text(app_window_gtk(win),
                                     title ? title : "Choose Color",
                                     "Hex colour (#RRGGBB):",
                                     *slot ? *slot : "");
    if (!value) return;

    GdkRGBA parsed;
    if (!gdk_rgba_parse(&parsed, value)) {
        dialog_error(app_window_gtk(win), "Invalid colour",
                     "Use a valid GTK colour, such as #1E1E1E.");
        g_free(value);
        return;
    }

    g_free(*slot);
    *slot = rgba_to_hex(&parsed);
    g_free(value);
    apply_preferences_to_all_tabs(win);
    cleaf_config_save(win);
}


/**
 * @brief Action choose background.
 */
void action_choose_background(GtkWidget *widget, gpointer user_data) {
    EditorWindow *win = user_data;
    choose_color_for_slot(win, widget, "Editor Background", win ? &win->editor_bg_color : NULL);
}


/**
 * @brief Action choose sidebar background.
 */
void action_choose_sidebar_background(GtkWidget *widget, gpointer user_data) {
    EditorWindow *win = user_data;
    choose_color_for_slot(win, widget, "Sidebar Background", win ? &win->sidebar_bg_color : NULL);
}


/**
 * @brief Action choose tabbar background.
 */
void action_choose_tabbar_background(GtkWidget *widget, gpointer user_data) {
    EditorWindow *win = user_data;
    choose_color_for_slot(win, widget, "Tab Bar Background", win ? &win->tabbar_bg_color : NULL);
}


/**
 * @brief Action choose scroll preview background.
 */
void action_choose_scroll_preview_background(GtkWidget *widget, gpointer user_data) {
    EditorWindow *win = user_data;
    choose_color_for_slot(win, widget, "Scroll Preview Background", win ? &win->scroll_preview_bg_color : NULL);
}


/**
 * @brief Action choose popover background.
 */
void action_choose_popover_background(GtkWidget *widget, gpointer user_data) {
    EditorWindow *win = user_data;
    choose_color_for_slot(win, widget, "Popover Background", win ? &win->popover_bg_color : NULL);
}


/**
 * @brief Action reset background.
 */
void action_reset_background(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    g_clear_pointer(&win->editor_bg_color, g_free);
    apply_preferences_to_all_tabs(win);
    cleaf_config_save(win);
}


/**
 * @brief Action reset all backgrounds.
 */
void action_reset_all_backgrounds(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    /*
     * Reset every colour override back to defaults. This is intentionally
     * explicit so new colour slots are visible during review and not hidden in a generic array.
     */
    g_clear_pointer(&win->editor_bg_color, g_free);
    g_clear_pointer(&win->editor_fg_color, g_free);
    g_clear_pointer(&win->editor_gutter_bg_color, g_free);
    g_clear_pointer(&win->editor_gutter_fg_color, g_free);
    g_clear_pointer(&win->editor_current_line_bg_color, g_free);
    g_clear_pointer(&win->editor_selection_bg_color, g_free);
    g_clear_pointer(&win->editor_selection_fg_color, g_free);
    g_clear_pointer(&win->editor_cursor_color, g_free);
    g_clear_pointer(&win->sidebar_bg_color, g_free);
    g_clear_pointer(&win->tabbar_bg_color, g_free);
    g_clear_pointer(&win->tabbar_fg_color, g_free);
    g_clear_pointer(&win->tab_active_bg_color, g_free);
    g_clear_pointer(&win->tab_active_fg_color, g_free);
    g_clear_pointer(&win->topbar_bg_color, g_free);
    g_clear_pointer(&win->topbar_fg_color, g_free);
    g_clear_pointer(&win->bottombar_bg_color, g_free);
    g_clear_pointer(&win->bottombar_fg_color, g_free);
    g_clear_pointer(&win->button_bg_color, g_free);
    g_clear_pointer(&win->button_fg_color, g_free);
    g_clear_pointer(&win->button_hover_bg_color, g_free);
    g_clear_pointer(&win->button_active_bg_color, g_free);
    g_clear_pointer(&win->input_bg_color, g_free);
    g_clear_pointer(&win->input_fg_color, g_free);
    g_clear_pointer(&win->input_border_color, g_free);
    g_clear_pointer(&win->project_tree_fg_color, g_free);
    g_clear_pointer(&win->project_tree_selected_bg_color, g_free);
    g_clear_pointer(&win->project_tree_selected_fg_color, g_free);
    g_clear_pointer(&win->scroll_preview_bg_color, g_free);
    g_clear_pointer(&win->scroll_preview_fg_color, g_free);
    g_clear_pointer(&win->popover_bg_color, g_free);
    g_clear_pointer(&win->ref_popover_bg_color, g_free);
    g_clear_pointer(&win->ref_popover_fg_color, g_free);
    g_clear_pointer(&win->ref_popover_heading_color, g_free);
    g_clear_pointer(&win->ref_popover_title_color, g_free);
    g_clear_pointer(&win->ref_popover_kind_color, g_free);
    g_clear_pointer(&win->ref_popover_snippet_color, g_free);
    g_clear_pointer(&win->completion_popover_bg_color, g_free);
    g_clear_pointer(&win->completion_popover_fg_color, g_free);
    g_clear_pointer(&win->completion_popover_detail_color, g_free);
    g_clear_pointer(&win->completion_selection_bg_color, g_free);
    g_clear_pointer(&win->completion_selection_fg_color, g_free);
    g_clear_pointer(&win->dialog_bg_color, g_free);
    g_clear_pointer(&win->dialog_fg_color, g_free);
    g_clear_pointer(&win->dialog_border_color, g_free);
    g_clear_pointer(&win->dialog_title_color, g_free);
    g_clear_pointer(&win->search_match_bg_color, g_free);
    g_clear_pointer(&win->search_match_fg_color, g_free);
    g_clear_pointer(&win->diagnostic_warning_bg_color, g_free);
    g_clear_pointer(&win->diagnostic_warning_fg_color, g_free);
    apply_preferences_to_all_tabs(win);
    cleaf_config_save(win);
}


