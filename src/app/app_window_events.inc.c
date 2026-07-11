static EditorTab *find_tab_for_path(EditorWindow *win, const char *canonical_path) {
    if (!win || !win->notebook || !canonical_path) return NULL;

    /*
     * Compare canonical paths so the same file is not opened twice through
     * different relative paths. Fixes duplicate issue.
     */
    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        EditorTab *tab = child ? g_object_get_data(G_OBJECT(child), "cleaf-tab") : NULL;

        if (!tab || !tab->file_path) continue;

        char *existing = g_canonicalize_filename(tab->file_path, NULL);
        gboolean same = existing && g_strcmp0(existing, canonical_path) == 0;
        g_free(existing);

        if (same) {
            // Existing tab wins. Move focus there instead of opening a duplicate.
            gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), i);
            return tab;
        }
    }

    return NULL;
}


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


void on_switch_page(GtkNotebook *notebook,
                    GtkWidget *page,
                    guint page_num,
                    gpointer user_data) {
    (void)notebook;
    (void)page;
    (void)page_num;

    // Page switches change the active tab, so refresh title, buttons, and status.
    app_window_update_ui(user_data);
}


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


gboolean on_window_close_request(GtkWindow *window, gpointer user_data) {
    (void)window;

    EditorWindow *win = user_data;

    /*
     * Returning TRUE cancels the close request. Keep the window alive if any tab
     * refuses to close because of unsaved changes.
     */
    if (!app_window_close_all_tabs(win)) return TRUE;

    // All tabs are closed, so the window-owned state can be released.
    app_window_free(win);
    return FALSE;
}


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


gboolean on_window_key_pressed(GtkEventControllerKey *controller,
                               guint keyval,
                               guint keycode,
                               GdkModifierType state,
                               gpointer user_data) {
    (void)controller;
    (void)keycode;

    EditorWindow *win = user_data;
    EditorTab *tab = app_window_current_tab(win);

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
    if (ctrl && key == GDK_KEY_s) { if (tab) editor_tab_save(tab, shift); return TRUE; }
    if (ctrl && key == GDK_KEY_w) { if (tab) app_window_close_tab(win, tab); return TRUE; }
    if (ctrl && shift && key == GDK_KEY_i) {
        action_toggle_codex_panel(NULL, win);
        return TRUE;
    }

    // Search uses one panel. Ctrl+F opens find mode and Ctrl+H opens replace mode.
    if (ctrl && key == GDK_KEY_f) { set_search_panel(win, TRUE, FALSE); return TRUE; }
    if (ctrl && key == GDK_KEY_h) { set_search_panel(win, TRUE, TRUE); return TRUE; }

    // Editing helpers are tab-scoped because they operate on the active buffer.
    if (ctrl && key == GDK_KEY_g) { if (tab) editor_tab_go_to_line(tab); return TRUE; }
    if (ctrl && key == GDK_KEY_j) { if (tab) editor_tab_justify_paragraph(tab); return TRUE; }
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
