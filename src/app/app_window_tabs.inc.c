void app_window_add_tab(EditorWindow *win,
                        EditorTab *tab,
                        gboolean switch_to_tab) {
    if (!win || !tab) return;

    /*
     * Store the EditorTab on the notebook page widget so later notebook events
     * can recover the tab without keeping a parallel page map.
     */
    g_object_set_data(G_OBJECT(tab->box), "cleaf-tab", tab);

    gint index = gtk_notebook_append_page(GTK_NOTEBOOK(win->notebook),
                                          tab->box,
                                          tab->tab_widget);

    // Let users reorder tabs directly in the notebook.
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(win->notebook),
                                     tab->box,
                                     TRUE);

    gtk_widget_set_visible(tab->box, TRUE);

    /*
     * New tabs inherit window-level view preferences. Per-tab widgets still own
     * their actual visibility state.
     */
    editor_tab_set_minimap_visible(tab, win->minimap_enabled);
    editor_tab_set_preview_visible(tab, win->preview_enabled);

    gtk_widget_set_visible(tab->tab_widget, TRUE);

    if (switch_to_tab) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), index);
    }

    app_window_update_ui(win);
}


void app_window_close_tab(EditorWindow *win, EditorTab *tab) {
    if (!win || !tab) return;

    // Give the tab a chance to stop closing if it has unsaved changes.
    if (!editor_tab_confirm_close(tab)) return;

    gint n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));

    for (gint i = 0; i < n; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);

        if (child && g_object_get_data(G_OBJECT(child), "cleaf-tab") == tab) {
            /*
             * Popovers can outlive normal widget focus changes, so destroy them
             * before removing the page and freeing the tab.
             */
            editor_tab_destroy_popovers(tab);

            gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook), i);
            editor_tab_free(tab);
            break;
        }
    }

    /*
     * Keep the editor usable after closing the last tab. Cleaf always has at
     * least one editable buffer.
     */
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook)) == 0) {
        EditorTab *new_tab = editor_tab_new(win);
        app_window_add_tab(win, new_tab, TRUE);
    }

    app_window_update_ui(win);
}


gboolean app_window_close_all_tabs(EditorWindow *win) {
    if (!win || !win->notebook) return TRUE;

    /*
     * Ask every tab first before destroying anything. This avoids closing some
     * tabs before another tab cancels because of unsaved changes.
     */
    gint n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));

    for (gint i = 0; i < n; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        EditorTab *tab = child
            ? g_object_get_data(G_OBJECT(child), "cleaf-tab")
            : NULL;

        if (tab && !editor_tab_confirm_close(tab)) return FALSE;
    }

    /*
     * Remove pages from the front until the notebook is empty. Page indexes
     * shift after each removal.
     */
    while (gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook)) > 0) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), 0);
        EditorTab *tab = child
            ? g_object_get_data(G_OBJECT(child), "cleaf-tab")
            : NULL;

        if (tab) editor_tab_destroy_popovers(tab);

        gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook), 0);

        if (tab) editor_tab_free(tab);
    }

    return TRUE;
}


int combo_index_for_syntax(EditorWindow *win, SyntaxDef *syntax) {
    if (!win || !syntax) return 0;

    /*
     * Combo index 0 is Plain Text. Loaded syntaxes start at index 1, so the
     * syntax array index needs a +1 offset.
     */
    for (guint i = 0; win->syntaxes && i < win->syntaxes->len; i++) {
        if (g_ptr_array_index(win->syntaxes, i) == syntax) {
            return (int)i + 1;
        }
    }

    return 0;
}


void populate_syntax_combo(EditorWindow *win, EditorTab *tab) {
    if (!win || !win->syntax_combo) return;

    /*
     * Rebuilding the combo would normally emit the same changed signal used for
     * user selection. Block it so UI refresh does not change the tab syntax.
     */
    win->syntax_combo_updating = TRUE;
    g_signal_handlers_block_by_func(win->syntax_combo,
                                    G_CALLBACK(on_syntax_changed),
                                    win);

    GtkStringList *items = gtk_string_list_new(NULL);

    // Plain Text is always available even when no syntax files load.
    gtk_string_list_append(items, "Plain Text");

    for (guint i = 0; win->syntaxes && i < win->syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(win->syntaxes, i);

        if (syntax && syntax->name) {
            gtk_string_list_append(items, syntax->name);
        }
    }

    gtk_drop_down_set_model(GTK_DROP_DOWN(win->syntax_combo),
                            G_LIST_MODEL(items));

    /*
     * The dropdown owns a reference to the model after set_model(). Drop our
     * local reference after selecting the active item.
     */
    gtk_drop_down_set_selected(GTK_DROP_DOWN(win->syntax_combo),
                               (guint)(tab ?
                                   combo_index_for_syntax(win,
                                                          tab->active_syntax)
                                   : 0));

    g_object_unref(items);

    g_signal_handlers_unblock_by_func(win->syntax_combo,
                                      G_CALLBACK(on_syntax_changed),
                                      win);
    win->syntax_combo_updating = FALSE;
}


void app_window_update_ui(EditorWindow *win) {
    if (!win) return;

    // Policy buttons are global window state and should reflect config changes.
    update_policy_buttons(win);

    EditorTab *tab = app_window_current_tab(win);

    if (!tab) {
        /*
         * No active tab means the window should show neutral state instead of
         * leaving the previous document title/status visible.
         */
        if (win->top_title_label) {
            gtk_label_set_text(GTK_LABEL(win->top_title_label), "Cleaf");
        }

        gtk_window_set_title(GTK_WINDOW(win->window), "Cleaf");
        app_window_set_status(win, "No open document");
        populate_syntax_combo(win, NULL);
        return;
    }

    char *base = editor_tab_basename(tab);
    char *title = g_strdup_printf("%s%s%s",
                                  tab->modified ? "*" : "",
                                  tab->locked ? "🔒 " : "",
                                  base);

    if (win->top_title_label) {
        gtk_label_set_text(GTK_LABEL(win->top_title_label), title);

        // Full path belongs in the tooltip because the top bar is intentionally compact.
        gtk_widget_set_tooltip_text(win->top_title_label,
                                    tab->file_path
                                        ? tab->file_path
                                        : "Unsaved buffer");
    }

    gtk_window_set_title(GTK_WINDOW(win->window), title);

    /*
     * Active tab changes can change the syntax selector, tab title, and status
     * text, so refresh all three from the current tab state.
     */
    populate_syntax_combo(win, tab);
    editor_tab_update_title(tab);
    editor_tab_update_status(tab);

    g_free(title);
    g_free(base);
}


void app_window_reload_syntaxes(EditorWindow *win) {
    if (!win) return;

    /*
     * Syntax definitions are runtime YAML files. Reload the registry and make
     * open tabs re-detect their language against the new definitions.
     */
    if (win->syntaxes) {
        g_ptr_array_free(win->syntaxes, TRUE);
    }

    win->syntaxes = syntax_load_all();

    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));

    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        EditorTab *tab = child
            ? g_object_get_data(G_OBJECT(child), "cleaf-tab")
            : NULL;

        if (tab) {
            /*
             * Manual overrides are cleared on reload because the old SyntaxDef
             * pointers may no longer exist after the registry is rebuilt.
             */
            tab->active_syntax = NULL;
            tab->manual_syntax_override = FALSE;
            editor_tab_auto_select_syntax(tab);
        }
    }

    app_window_update_ui(win);
}