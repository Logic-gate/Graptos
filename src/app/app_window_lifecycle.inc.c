/**
 * @file src/app/app_window_lifecycle.inc.c
 * @brief Cleaf app window lifecycle module.
 */

EditorTab *app_window_current_tab(EditorWindow *win) {
    if (!win || !win->notebook) return NULL;
    gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook));
    if (page < 0) return NULL;
    GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), page);
    if (!child) return NULL;
    return g_object_get_data(G_OBJECT(child), "cleaf-tab");
}

/**
 * @brief Codex status changed.
 */
static void codex_status_changed(CodexClient *client,
                                 CodexClientState state,
                                 const char *detail,
                                 gpointer user_data) {
    (void)client;
    EditorWindow *win = user_data;
    if (!win) return;
    codex_panel_set_connection(win->codex_panel, state, detail);

    if (state == CODEX_CLIENT_READY) {
        app_window_set_status(win, "Codex ready");
    } else if (state == CODEX_CLIENT_FAILED) {
        char *message = g_strdup_printf("Codex unavailable: %s",
                                        detail ? detail : "unknown error");
        app_window_set_status(win, message);
        g_free(message);
    }
}


/**
 * @brief App window new.
 */
EditorWindow *app_window_new(GtkApplication *application) {
    EditorWindow *win = g_new0(EditorWindow, 1);
    if (!win) return NULL;
    win->application = application;
    win->tab_width = 4u;
    win->insert_spaces = TRUE;
    win->autocomplete_enabled = TRUE;
    win->auto_save_enabled = FALSE;
    win->backup_enabled = FALSE;
    win->use_system_interface_font = FALSE;
    win->minimap_enabled = TRUE;
    win->preview_enabled = FALSE;
    win->use_gtksourceview_highlighting = TRUE;
    win->use_yaml_style_overrides = TRUE;
    // Without defaults here, GTK paints white until the first file/tab refresh reapplies config CSS
    win->editor_bg_color = g_strdup("#181a1f");
    win->editor_fg_color = g_strdup("#d4d4d4");
    win->editor_gutter_bg_color = g_strdup("#181a1f");
    win->editor_gutter_fg_color = g_strdup("#8b949e");
    win->editor_current_line_bg_color = g_strdup("#20232b");
    win->editor_selection_bg_color = g_strdup("#3a405c");
    win->editor_selection_fg_color = g_strdup("#ffffff");
    win->editor_cursor_color = g_strdup("#d4d4d4");
    win->sidebar_bg_color = g_strdup("#181a1f");
    win->tabbar_bg_color = g_strdup("#181a1f");
    win->tabbar_fg_color = g_strdup("#d4d4d4");
    win->tab_active_bg_color = g_strdup("#20232b");
    win->tab_active_fg_color = g_strdup("#ffffff");
    win->topbar_bg_color = g_strdup("#181a1f");
    win->topbar_fg_color = g_strdup("#d4d4d4");
    win->bottombar_bg_color = g_strdup("#181a1f");
    win->bottombar_fg_color = g_strdup("#d4d4d4");
    win->button_bg_color = g_strdup("#181a1f");
    win->button_fg_color = g_strdup("#d4d4d4");
    win->button_hover_bg_color = g_strdup("#2a2e3d");
    win->button_active_bg_color = g_strdup("#31364a");
    win->input_bg_color = g_strdup("#111318");
    win->input_fg_color = g_strdup("#d4d4d4");
    win->input_border_color = g_strdup("#3a4050");
    win->project_tree_fg_color = g_strdup("#d4d4d4");
    win->project_tree_selected_bg_color = g_strdup("#2a2e3d");
    win->project_tree_selected_fg_color = g_strdup("#ffffff");
    win->scroll_preview_bg_color = g_strdup("#181a1f");
    win->scroll_preview_fg_color = g_strdup("#8b949e");
    win->popover_bg_color = g_strdup("#181a1f");
    win->ref_popover_bg_color = g_strdup("#181a1f");
    win->ref_popover_fg_color = g_strdup("#d4d4d4");
    win->ref_popover_heading_color = g_strdup("#cba6f7");
    win->ref_popover_title_color = g_strdup("#89dceb");
    win->ref_popover_kind_color = g_strdup("#a6adc8");
    win->ref_popover_snippet_color = g_strdup("#d4d4d4");
    win->completion_popover_bg_color = g_strdup("#181a1f");
    win->completion_popover_fg_color = g_strdup("#d4d4d4");
    win->completion_popover_detail_color = g_strdup("#a6adc8");
    win->completion_selection_bg_color = g_strdup("#89b4fa");
    win->completion_selection_fg_color = g_strdup("#11111b");
    win->dialog_bg_color = g_strdup("#1b1f24");
    win->dialog_fg_color = g_strdup("#d4d4d4");
    win->dialog_border_color = g_strdup("#f1e0a8");
    win->dialog_title_color = g_strdup("#ffffff");
    win->search_match_bg_color = g_strdup("#515c7a");
    win->search_match_fg_color = g_strdup("#ffffff");
    win->diagnostic_warning_bg_color = g_strdup("#5f4b24");
    win->diagnostic_warning_fg_color = g_strdup("#ffd166");
    win->codex_preview_bg_color = NULL;
    win->codex_preview_fg_color = NULL;
    win->codex_prompt_bg_color = NULL;
    win->project_roots = g_ptr_array_new_with_free_func(g_free);
    win->locked_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    win->git_file_status = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    win->syntaxes = syntax_load_all();
    cleaf_config_load(win);
    if (!win->codex_preview_bg_color) {
        win->codex_preview_bg_color = g_strdup(win->editor_bg_color);
    }
    if (!win->codex_preview_fg_color) {
        win->codex_preview_fg_color = g_strdup("#d4d4d4");
    }
    if (!win->codex_prompt_bg_color) {
        win->codex_prompt_bg_color = g_strdup(win->editor_bg_color);
    }

    /*
     * Install both static and config-derived providers before presenting the
     * window so first-launch widgets inherit Cleaf colors from their first
     * allocation, not only after a later tab or file event.
     */
    cleaf_apply_css();
    app_window_apply_css(win);

    win->window = gtk_application_window_new(application);
    gtk_widget_add_css_class(win->window, "cleaf-window");
    gtk_window_set_default_size(GTK_WINDOW(win->window), 1100, 760);
    gtk_window_set_title(GTK_WINDOW(win->window), "Cleaf");

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(root, "cleaf-root");
    gtk_window_set_child(GTK_WINDOW(win->window), root);

    GtkWidget *top = build_top_bar(win);
    gtk_box_append(GTK_BOX(root), top);

    GtkWidget *main_pane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(main_pane, TRUE);
    gtk_box_append(GTK_BOX(root), main_pane);

    win->project_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(win->project_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(win->project_revealer), 120u);
    GtkWidget *project_pane = project_tree_create(win);
    gtk_revealer_set_child(GTK_REVEALER(win->project_revealer), project_pane);
    gtk_paned_set_start_child(GTK_PANED(main_pane), win->project_revealer);
    gtk_paned_set_resize_start_child(GTK_PANED(main_pane), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(main_pane), FALSE);

    GtkWidget *content_pane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_end_child(GTK_PANED(main_pane), content_pane);
    gtk_paned_set_resize_end_child(GTK_PANED(main_pane), TRUE);
    gtk_paned_set_shrink_end_child(GTK_PANED(main_pane), FALSE);

    win->notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(win->notebook), TRUE);
    gtk_paned_set_start_child(GTK_PANED(content_pane), win->notebook);
    gtk_paned_set_resize_start_child(GTK_PANED(content_pane), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(content_pane), FALSE);
    win->codex_panel = codex_panel_new(win);
    gtk_paned_set_end_child(GTK_PANED(content_pane),
                            codex_panel_widget(win->codex_panel));
    gtk_paned_set_resize_end_child(GTK_PANED(content_pane), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(content_pane), TRUE);
    gtk_paned_set_position(GTK_PANED(main_pane), 240);
    g_signal_connect(win->notebook, "switch-page", G_CALLBACK(on_switch_page), win);

    GtkWidget *search_panel = build_search_panel(win);
    gtk_box_append(GTK_BOX(root), search_panel);

    GtkWidget *tool_panel = build_tool_panel(win);
    gtk_box_append(GTK_BOX(root), tool_panel);

    GtkWidget *bottom = build_bottom_bar(win);
    gtk_box_append(GTK_BOX(root), bottom);

    g_signal_connect(win->window, "close-request", G_CALLBACK(on_window_close_request), win);

    /*
     * Window shortcuts use capture phase because GtkSourceView consumes several
     * navigation keys, including PageUp/PageDown, during normal bubble-phase
     * delivery.  Capturing here keeps application-level tab switching reliable.
     */
    GtkEventController *key_controller = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key_controller, GTK_PHASE_CAPTURE);
    g_signal_connect(key_controller, "key-pressed",
                     G_CALLBACK(on_window_key_pressed), win);
    gtk_widget_add_controller(win->window, key_controller);

    EditorTab *tab = editor_tab_new(win);
    app_window_add_tab(win, tab, TRUE);
    app_window_update_ui(win);
    restart_auto_save_timer(win);
    if (!g_getenv("CLEAF_TEST_MODE")) {
        win->codex_client = codex_client_new(codex_status_changed, win);
        if (win->codex_client) {
            codex_client_set_event_func(win->codex_client,
                                        codex_panel_handle_event);
            char *fallback_cwd = win->project_root ? NULL : g_get_current_dir();
            const char *cwd = win->project_root ? win->project_root : fallback_cwd;
            codex_client_start(win->codex_client, cwd);
            g_free(fallback_cwd);
        }
    }
    gtk_window_present(GTK_WINDOW(win->window));
    set_search_panel(win, FALSE, FALSE);
    return win;
}


/**
 * @brief App window free.
 */
void app_window_free(EditorWindow *win) {
    if (!win) return;
    project_tree_close_context(win);
    codex_client_free(win->codex_client);
    codex_panel_free(win->codex_panel);
    cleaf_config_save(win);
    cleaf_source_cancel(&win->auto_save_timeout);
    if (win->syntaxes) g_ptr_array_free(win->syntaxes, TRUE);
    if (win->project_expanded) g_hash_table_destroy(win->project_expanded);
    if (win->project_roots) g_ptr_array_free(win->project_roots, TRUE);
    if (win->locked_paths) g_hash_table_destroy(win->locked_paths);
    if (win->git_file_status) g_hash_table_destroy(win->git_file_status);
    g_free(win->editor_bg_color);
    g_free(win->editor_fg_color);
    g_free(win->editor_gutter_bg_color);
    g_free(win->editor_gutter_fg_color);
    g_free(win->editor_current_line_bg_color);
    g_free(win->editor_selection_bg_color);
    g_free(win->editor_selection_fg_color);
    g_free(win->editor_cursor_color);
    g_free(win->sidebar_bg_color);
    g_free(win->tabbar_bg_color);
    g_free(win->tabbar_fg_color);
    g_free(win->tab_active_bg_color);
    g_free(win->tab_active_fg_color);
    g_free(win->topbar_bg_color);
    g_free(win->topbar_fg_color);
    g_free(win->bottombar_bg_color);
    g_free(win->bottombar_fg_color);
    g_free(win->button_bg_color);
    g_free(win->button_fg_color);
    g_free(win->button_hover_bg_color);
    g_free(win->button_active_bg_color);
    g_free(win->input_bg_color);
    g_free(win->input_fg_color);
    g_free(win->input_border_color);
    g_free(win->project_tree_fg_color);
    g_free(win->project_tree_selected_bg_color);
    g_free(win->project_tree_selected_fg_color);
    g_free(win->scroll_preview_bg_color);
    g_free(win->scroll_preview_fg_color);
    g_free(win->popover_bg_color);
    g_free(win->ref_popover_bg_color);
    g_free(win->ref_popover_fg_color);
    g_free(win->ref_popover_heading_color);
    g_free(win->ref_popover_title_color);
    g_free(win->ref_popover_kind_color);
    g_free(win->ref_popover_snippet_color);
    g_free(win->completion_popover_bg_color);
    g_free(win->completion_popover_fg_color);
    g_free(win->completion_popover_detail_color);
    g_free(win->completion_selection_bg_color);
    g_free(win->completion_selection_fg_color);
    g_free(win->dialog_bg_color);
    g_free(win->dialog_fg_color);
    g_free(win->dialog_border_color);
    g_free(win->dialog_title_color);
    g_free(win->search_match_bg_color);
    g_free(win->search_match_fg_color);
    g_free(win->diagnostic_warning_bg_color);
    g_free(win->diagnostic_warning_fg_color);
    g_free(win->codex_preview_bg_color);
    g_free(win->codex_preview_fg_color);
    g_free(win->codex_prompt_bg_color);
    g_free(win->project_root);
    g_free(win);
}
