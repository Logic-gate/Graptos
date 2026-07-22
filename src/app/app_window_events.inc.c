/**
 * @file src/app/app_window_events.inc.c
 * @brief Graptoς app window events module.
 * @details Window events are not isolated in practice. A tab switch can affect the status
 *          bar, terminal directory, tile groups, and saved UI state, so this file
 *          coordinates those reactions instead of hiding them inside unrelated modules.
 * @param win The win supplied by the caller.
 * @param canonical_path The canonical path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */

static EditorTab *find_tab_for_path(EditorWindow *win, const char *canonical_path) {
    if (!win || !canonical_path) return NULL;

    /*
     * Compare canonical paths so the same file is not opened twice through
     * different relative paths. Fixes duplicate issue.
     */
    guint count = app_window_tab_count(win);
    for (guint i = 0u; i < count; i++) {
        EditorTab *tab = app_window_tab_at(win, i);
        if (!tab || !tab->file_path) continue;

        char *existing = g_canonicalize_filename(tab->file_path, NULL);
        gboolean same = existing && g_strcmp0(existing, canonical_path) == 0;
        g_free(existing);

        if (same) {
            // Existing tab wins. Move focus there instead of opening a duplicate.
            EditorTab *owner = app_window_restore_saved_tile_group(win, tab)
                ? app_window_current_tab(win)
                : NULL;
            gint page = app_window_page_index_for_tab(win, owner ? owner : tab);
            if (page >= 0) gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), page);
            win->active_tab = tab;
            return tab;
        }
    }

    return NULL;
}


/**
 * @brief App window open file.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean app_window_open_file(EditorWindow *win, const char *path) {
    if (!win || !path || path[0] == '\0') return FALSE;

    /*
     * Store and compare files by canonical path. This avoids duplicate tabs
     * when a file is opened through ./file, ../dir/file, or the project tree.
     */
    char *canonical = g_canonicalize_filename(path, NULL);
    if (!canonical) return FALSE;

    EditorTab *existing = find_tab_for_path(win, canonical);
    if (existing) {
        char *base = editor_tab_basename(existing);
        char *msg = g_strdup_printf("Already open: %s", base ? base : canonical);
        app_window_set_status(win, msg);

        g_free(msg);
        g_free(base);
        g_free(canonical);
        return TRUE;
    }

    EditorTab *tab = editor_tab_new(win);

    /*
     * Only add the tab after loading succeeds. Failed loads should not leave
     * empty broken tabs in the notebook.
     */
    if (editor_tab_load_file(tab, canonical)) {
        editor_tab_set_locked(tab, app_window_is_file_locked(win, canonical));
        app_window_add_tab(win, tab, TRUE);
        g_free(canonical);
        return TRUE;
    }

    editor_tab_free(tab);
    g_free(canonical);
    return FALSE;
}


/**
 * @brief Synchronize dynamic terminal state after GTK finishes tab switching.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean terminal_panel_sync_idle(gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win) return G_SOURCE_REMOVE;

    win->terminal_sync_timeout = 0u;
    terminal_panel_sync_to_active_file(win->terminal_panel);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Queue terminal-directory synchronization after an editor tab switch.
 * @details Notebook switching is still settling when this is called. We defer
 *          terminal sync to idle time so dynamic terminal tabs read the final
 *          active editor tab instead of the one GTK was halfway through leaving.
 * @param win The win supplied by the caller.
 */
static void queue_terminal_panel_sync(EditorWindow *win) {
    if (!win || !win->terminal_dynamic_directory) return;

    graptos_source_cancel(&win->terminal_sync_timeout);
    win->terminal_sync_timeout = g_idle_add(terminal_panel_sync_idle, win);
}

/**
 * @brief On switch page.
 * @details A tab switch is not just focus. Tile mode, saved tile groups,
 *          terminal directory tracking, and normal UI refresh all hang off the
 *          same GTK signal, so the branches here keep those side effects in one
 *          visible order.
 * @param notebook The notebook supplied by the caller.
 * @param page The page supplied by the caller.
 * @param page_num The page num supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_switch_page(GtkNotebook *notebook,
                    GtkWidget *page,
                    guint page_num,
                    gpointer user_data) {
    (void)notebook;
    (void)page_num;

    // Page switches change the active tab, so refresh title, buttons, and status.
    EditorWindow *win = user_data;
    EditorTab *previous_tab = win ? win->active_tab : NULL;
    EditorTab *switched_tab = page
        ? g_object_get_data(G_OBJECT(page), "graptos-tab") : NULL;
    if (win && win->debug_mode) {
        g_message("Tiles: switch-page previous=%p switched=%p shift=%d internal=%d mode=%d",
                  (void *)previous_tab,
                  (void *)switched_tab,
                  win->tile_shift_down ? 1 : 0,
                  win->tile_internal_switch ? 1 : 0,
                  win->tile_mode ? 1 : 0);
    }
    if (win && win->tile_shift_down && !win->tile_internal_switch &&
        previous_tab && switched_tab && previous_tab != switched_tab) {
        win->active_tab = switched_tab;
        app_window_handle_shift_tab_switch(win, previous_tab, switched_tab);
    } else if (win && win->tile_mode && !win->tile_internal_switch) {
        if (app_window_tile_contains(win, switched_tab)) {
            win->active_tab = switched_tab;
            app_window_update_tiles(win);
        } else {
            app_window_clear_tiles(win);
            win->active_tab = switched_tab;
            if (app_window_restore_saved_tile_group(win, switched_tab)) {
                win->tile_suppress_label_tab = switched_tab;
            }
        }
    } else if (win) {
        win->active_tab = switched_tab;
        if (app_window_restore_saved_tile_group(win, switched_tab)) {
            win->tile_suppress_label_tab = switched_tab;
        }
    }
    app_window_update_ui(win);
    queue_terminal_panel_sync(win);
}


/**
 * @brief On syntax changed.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param drop_down The drop down supplied by the caller.
 * @param pspec The pspec supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_syntax_changed(GtkDropDown *drop_down,
                       GParamSpec *pspec,
                       gpointer user_data) {
    (void)pspec;

    EditorWindow *win = user_data;
    codex_panel_refresh_context(win ? win->codex_panel : NULL);
    if (!win || win->syntax_combo_updating || !drop_down) return;

    EditorTab *tab = app_window_current_tab(win);
    if (!tab) return;

    /*
     * Index 0 is the automatic/no-forced-syntax option. Real syntaxes are
     * stored after it, so subtract one before reading from win->syntaxes.
     */
    guint selected = gtk_drop_down_get_selected(drop_down);
    if (selected == GTK_INVALID_LIST_POSITION || selected == 0u) {
        editor_tab_set_syntax(tab, NULL, TRUE);
    } else if (win->syntaxes && selected - 1u < win->syntaxes->len) {
        SyntaxDef *syntax = g_ptr_array_index(win->syntaxes, selected - 1u);
        editor_tab_set_syntax(tab, syntax, TRUE);
    }

    // Syntax affects the tab title/status when the language is manually changed.
    editor_tab_update_title(tab);
    editor_tab_update_status(tab);
}


/**
 * @brief On window close request.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param window The window supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean on_window_close_request(GtkWindow *window, gpointer user_data) {
    (void)window;

    EditorWindow *win = user_data;
    if (!win) return FALSE;
    win->closing = TRUE;

    /*
     * Returning TRUE cancels the close request. Keep the window alive if any tab
     * refuses to close because of unsaved changes.
     */
    if (!app_window_close_all_tabs(win)) {
        win->closing = FALSE;
        return TRUE;
    }

    // All tabs are closed, so the window-owned state can be released.
    app_window_free(win);
    return FALSE;
}


/**
 * @brief Switch page delta.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param delta The delta supplied by the caller.
 */
void switch_page_delta(EditorWindow *win, int delta) {
    if (!win || !win->notebook) return;

    gint n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    if (n <= 1) return;

    /*
     * Wrap around so Ctrl+PageUp/PageDown can cycle through tabs without
     * stopping at the first or last tab.
     */
    gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->notebook));
    gint next = (page + delta + n) % n;

    gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), next);
}


/**
 * @brief On window key pressed.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param controller The controller supplied by the caller.
 * @param keyval The keyval supplied by the caller.
 * @param keycode The keycode supplied by the caller.
 * @param state The state supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean on_window_key_pressed(GtkEventControllerKey *controller,
                               guint keyval,
                               guint keycode,
                               GdkModifierType state,
                               gpointer user_data) {
    (void)controller;
    (void)keycode;

    EditorWindow *win = user_data;
    EditorTab *tab = app_window_current_tab(win);
    if (win) {
        win->tile_shift_down =
            (state & GDK_SHIFT_MASK) != 0 ||
            keyval == GDK_KEY_Shift_L ||
            keyval == GDK_KEY_Shift_R;
    }

    /*
     * Normalize letter keys so shortcuts work the same with shifted letters.
     * Non-letter keys still use keyval below where keypad variants matter.
     */
    guint key = gdk_keyval_to_lower(keyval);
    gboolean ctrl = (state & GDK_CONTROL_MASK) != 0;
    gboolean shift = (state & GDK_SHIFT_MASK) != 0;

    // Keep shortcuts here so actions work even when toolbar buttons are hidden.
    // Inspired by nano
    if (ctrl && key == GDK_KEY_n) { action_new(NULL, win); return TRUE; }
    if (ctrl && key == GDK_KEY_t) { action_new(NULL, win); return TRUE; }
    if (ctrl && shift && key == GDK_KEY_o) { action_open_folder(NULL, win); return TRUE; }
    if (ctrl && key == GDK_KEY_o) { action_open(NULL, win); return TRUE; }
    if (ctrl && key == GDK_KEY_b) {
        action_toggle_project_tree(NULL, win);
        return TRUE;
    }
    if (ctrl && key == GDK_KEY_s) { if (tab) editor_tab_save(tab, shift); return TRUE; }
    if (ctrl && key == GDK_KEY_w) { if (tab) app_window_close_tab(win, tab); return TRUE; }
    if (ctrl && key == GDK_KEY_grave) {
        action_toggle_terminal_panel(NULL, win);
        return TRUE;
    }
    if (ctrl && shift && key == GDK_KEY_i) {
        action_toggle_codex_panel(NULL, win);
        return TRUE;
    }

    // Search uses one panel. Ctrl+F opens find mode and Ctrl+H opens replace mode.
    if (ctrl && key == GDK_KEY_f) { set_search_panel(win, TRUE, FALSE); return TRUE; }
    if (ctrl && key == GDK_KEY_h) { set_search_panel(win, TRUE, TRUE); return TRUE; }

    // Editing helpers are tab-scoped because they operate on the active buffer.
    if (ctrl && key == GDK_KEY_g) { if (tab) editor_tab_go_to_line(tab); return TRUE; }
    if (ctrl && key == GDK_KEY_j) { if (tab) editor_tab_format_code(tab); return TRUE; }
    if (ctrl && key == GDK_KEY_k) { if (tab) editor_tab_cut_line(tab); return TRUE; }
    if (ctrl && key == GDK_KEY_u) { if (tab) editor_tab_paste_cut_line(tab); return TRUE; }
    if (ctrl && key == GDK_KEY_slash) { if (tab) editor_tab_toggle_comment(tab); return TRUE; }
    if (ctrl && key == GDK_KEY_space) { if (tab) editor_tab_show_completion(tab, TRUE); return TRUE; }

    // Accept keypad PageUp/PageDown because some keyboards send KP variants.
    if (ctrl && (keyval == GDK_KEY_Page_Down || keyval == GDK_KEY_KP_Page_Down)) {
        switch_page_delta(win, 1);
        return TRUE;
    }

    if (ctrl && (keyval == GDK_KEY_Page_Up || keyval == GDK_KEY_KP_Page_Up)) {
        switch_page_delta(win, -1);
        return TRUE;
    }

    /*
     * Escape only closes the search panel when it is visible. Otherwise leave it
     * for GTK widgets to handle normally.
     */
    if (key == GDK_KEY_Escape &&
        win->search_revealer &&
        gtk_revealer_get_reveal_child(GTK_REVEALER(win->search_revealer))) {
        set_search_panel(win, FALSE, FALSE);
        return TRUE;
    }

    // F3 follows common editor behavior. Shift+F3 searches backwards.
    if (key == GDK_KEY_F3) {
        if (shift) action_find_prev(NULL, win);
        else action_find_next(NULL, win);
        return TRUE;
    }

    return FALSE;
}

/**
 * @brief On window key released.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param controller The controller supplied by the caller.
 * @param keyval The keyval supplied by the caller.
 * @param keycode The keycode supplied by the caller.
 * @param state The state supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_window_key_released(GtkEventControllerKey *controller,
                            guint keyval,
                            guint keycode,
                            GdkModifierType state,
                            gpointer user_data) {
    (void)controller;
    (void)keycode;
    EditorWindow *win = user_data;
    if (!win) return;
    if (keyval == GDK_KEY_Shift_L || keyval == GDK_KEY_Shift_R ||
        (state & GDK_SHIFT_MASK) == 0) {
        win->tile_shift_down = FALSE;
    }
}
