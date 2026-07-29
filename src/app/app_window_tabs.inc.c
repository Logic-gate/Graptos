/**
 * @file src/app/app_window_tabs.inc.c
 * @brief Graptoς app window tabs module.
 * @details Tabs are where ownership bugs usually show up first. We keep open, close, and
 *          close-all behavior together so notebook pages, EditorTab memory, popovers, and
 *          saved tile groups are released in a predictable order.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param switch_to_tab The switch to tab supplied by the caller.
 */

void app_window_add_tab(EditorWindow *win,
                        EditorTab *tab,
                        gboolean switch_to_tab) {
    if (!win || !tab) return;
    app_window_register_tab(win, tab);

    /*
     * Store the EditorTab on the notebook page widget so later notebook events
     * can recover the tab without keeping a parallel page map.
     */
    g_object_set_data(G_OBJECT(tab->box), "graptos-tab", tab);

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
        win->active_tab = tab;
    }

    app_window_update_ui(win);
}

/**
 * @brief Tab close menu scope.
 * @details The context menu closes tabs by browser-style groups. The enum keeps
 *          callbacks small and makes the warning text match the operation.
 */
typedef enum {
    APP_TAB_CLOSE_ALL, /**< Close every tab. */
    APP_TAB_CLOSE_OTHERS, /**< Close every tab except the clicked tab. */
    APP_TAB_CLOSE_RIGHT, /**< Close visible tabs to the right. */
    APP_TAB_CLOSE_LEFT /**< Close visible tabs to the left. */
} AppTabCloseScope;

/**
 * @brief One tab context-menu action.
 * @details Menu buttons outlive the stack frame that builds the popover, so the
 *          callback keeps the clicked tab, operation, and popover together.
 */
typedef struct {
    EditorWindow *win; /**< Window that owns the notebook. */
    EditorTab *tab; /**< Tab whose label opened the menu. */
    GtkWidget *popover; /**< One-shot context popover. */
    AppTabCloseScope scope; /**< Close operation requested by the user. */
} AppTabCloseAction;

/**
 * @brief Return whether a tab is still registered.
 * @details Batch closes can remove saved tile groups at once. Checking the
 *          registry before each pointer use avoids touching a tab closed by an
 *          earlier operation in the same batch.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose registry state is being inspected.
 * @return TRUE when the tab is still open.
 */
static gboolean app_window_tab_is_registered(EditorWindow *win,
                                             EditorTab *tab) {
    if (!win || !tab) return FALSE;
    guint count = app_window_tab_count(win);
    for (guint i = 0u; i < count; i++) {
        if (app_window_tab_at(win, i) == tab) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Append a visible notebook tab to a pointer array.
 * @details Visible left/right close operations follow notebook order, not the
 *          internal registry order that also contains folded tile members.
 * @param win The win supplied by the caller.
 * @param targets Target array that receives the tab pointer.
 * @param page_index Notebook page index.
 */
static void app_window_append_visible_tab(EditorWindow *win,
                                          GPtrArray *targets,
                                          gint page_index) {
    if (!win || !targets || page_index < 0) return;
    GtkWidget *child =
        gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), page_index);
    EditorTab *tab = child
        ? g_object_get_data(G_OBJECT(child), "graptos-tab")
        : NULL;
    if (tab) g_ptr_array_add(targets, tab);
}

/**
 * @brief Collect tabs affected by a context-menu close operation.
 * @details Close-all uses the registry through its existing path. Other scopes
 *          return explicit targets so the caller can confirm and close them in
 *          a stable order.
 * @param win The win supplied by the caller.
 * @param anchor The tab whose label opened the menu.
 * @param scope The close operation requested by the user.
 * @return Newly allocated target array.
 */
static GPtrArray *app_window_collect_close_targets(EditorWindow *win,
                                                  EditorTab *anchor,
                                                  AppTabCloseScope scope) {
    GPtrArray *targets = g_ptr_array_new();
    if (!win || !win->notebook || !anchor) return targets;

    if (scope == APP_TAB_CLOSE_OTHERS) {
        guint count = app_window_tab_count(win);
        for (guint i = 0u; i < count; i++) {
            EditorTab *tab = app_window_tab_at(win, i);
            if (tab && tab != anchor) g_ptr_array_add(targets, tab);
        }
        return targets;
    }

    gint anchor_page = app_window_page_index_for_tab(win, anchor);
    if (anchor_page < 0) return targets;

    if (scope == APP_TAB_CLOSE_LEFT) {
        for (gint i = 0; i < anchor_page; i++) {
            app_window_append_visible_tab(win, targets, i);
        }
    } else if (scope == APP_TAB_CLOSE_RIGHT) {
        gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
        for (gint i = anchor_page + 1; i < pages; i++) {
            app_window_append_visible_tab(win, targets, i);
        }
    }

    return targets;
}

/**
 * @brief Return a human-readable close scope name.
 * @details The warning dialog and status messages should use the same operation
 *          names that appear in the context menu.
 * @param scope The close operation requested by the user.
 * @return Static label for the operation.
 */
static const char *app_window_close_scope_label(AppTabCloseScope scope) {
    switch (scope) {
        case APP_TAB_CLOSE_ALL: return "Close All Tabs";
        case APP_TAB_CLOSE_OTHERS: return "Close Other Tabs";
        case APP_TAB_CLOSE_RIGHT: return "Close Tabs to the Right";
        case APP_TAB_CLOSE_LEFT: return "Close Tabs to the Left";
    }
    return "Close Tabs";
}

/**
 * @brief Confirm a multi-tab close operation.
 * @details This is the browser-style bulk warning. Individual dirty tabs still
 *          run their normal save/discard prompt after this succeeds.
 * @param win The win supplied by the caller.
 * @param scope The close operation requested by the user.
 * @param count Number of tabs that will be closed.
 * @return TRUE when the user wants to continue.
 */
static gboolean app_window_confirm_bulk_close(EditorWindow *win,
                                              AppTabCloseScope scope,
                                              guint count) {
    if (!win || count <= 1u) return TRUE;
    g_autofree char *message = g_strdup_printf(
        "This will close %u tabs. Unsaved tabs will still ask whether to save, "
        "discard, or cancel.",
        count);
    return dialog_confirm_yes_no(app_window_gtk(win),
                                 app_window_close_scope_label(scope),
                                 message);
}

/**
 * @brief Ensure the window still has one editable tab.
 * @details Close-all is also exposed from an in-app menu now. Unlike window
 *          shutdown, the editor should remain usable after the operation.
 * @param win The win supplied by the caller.
 */
static void app_window_ensure_editable_tab(EditorWindow *win) {
    if (!win || app_window_tab_count(win) > 0u) return;
    EditorTab *new_tab = editor_tab_new(win);
    app_window_add_tab(win, new_tab, TRUE);
}

/**
 * @brief Run a context-menu tab close action.
 * @details The action first shows a bulk warning, then delegates each tab to
 *          the existing close path so saved tiles and dirty buffers keep their
 *          normal behavior.
 * @param win The win supplied by the caller.
 * @param anchor The tab whose label opened the menu.
 * @param scope The close operation requested by the user.
 */
static void app_window_run_tab_close_scope(EditorWindow *win,
                                           EditorTab *anchor,
                                           AppTabCloseScope scope) {
    if (!win || !anchor) return;

    if (scope == APP_TAB_CLOSE_ALL) {
        guint count = app_window_tab_count(win);
        if (!app_window_confirm_bulk_close(win, scope, count)) return;
        if (app_window_close_all_tabs(win)) {
            app_window_ensure_editable_tab(win);
            app_window_update_ui(win);
        }
        return;
    }

    g_autoptr(GPtrArray) targets =
        app_window_collect_close_targets(win, anchor, scope);
    if (!targets || targets->len == 0u) return;
    if (!app_window_confirm_bulk_close(win, scope, targets->len)) return;

    for (guint i = 0u; i < targets->len; i++) {
        EditorTab *tab = g_ptr_array_index(targets, i);
        if (!app_window_tab_is_registered(win, tab)) continue;
        if (!app_window_close_tab(win, tab)) break;
    }

    app_window_update_ui(win);
}

/**
 * @brief Handle a tab context-menu button click.
 * @details The menu is closed before any tab removal starts so GTK does not
 *          keep a popover attached to a tab label that may be destroyed.
 * @param button The button that emitted the callback.
 * @param user_data Callback data describing the requested close action.
 */
static void app_window_tab_context_close_clicked(GtkWidget *button,
                                                 gpointer user_data) {
    (void)button;
    AppTabCloseAction *action = user_data;
    if (!action) return;
    if (action->popover && GTK_IS_POPOVER(action->popover)) {
        graptos_popover_hide(action->popover);
    }
    app_window_run_tab_close_scope(action->win, action->tab, action->scope);
}

/**
 * @brief Clear and destroy a tab context popover.
 * @details Tab context popovers are one-shot widgets. The identity check avoids
 *          clearing a newer menu that replaced this one.
 * @param popover The popover supplied by GTK.
 * @param user_data The tab label that owns the stored popover pointer.
 */
static void app_window_tab_context_closed(GtkPopover *popover,
                                          gpointer user_data) {
    GtkWidget *parent = user_data;
    if (parent) {
        GtkWidget *stored =
            g_object_get_data(G_OBJECT(parent), "graptos-tab-context-popover");
        if (stored == GTK_WIDGET(popover)) {
            g_object_set_data(G_OBJECT(parent),
                              "graptos-tab-context-popover",
                              NULL);
        }
    }
    graptos_widget_destroy(GTK_WIDGET(popover));
}

/**
 * @brief Create a tab context-menu button.
 * @details Each button owns its action data for as long as the popover keeps
 *          the button alive.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose label opened the menu.
 * @param popover The popover that owns the button.
 * @param label The label shown to the user.
 * @param scope The close operation requested by the button.
 * @param sensitive TRUE when the button can be activated.
 * @return Newly created menu button.
 */
static GtkWidget *app_window_tab_context_button(EditorWindow *win,
                                                EditorTab *tab,
                                                GtkWidget *popover,
                                                const char *label,
                                                AppTabCloseScope scope,
                                                gboolean sensitive) {
    AppTabCloseAction *action = g_new0(AppTabCloseAction, 1);
    action->win = win;
    action->tab = tab;
    action->popover = popover;
    action->scope = scope;

    GtkWidget *button =
        graptos_flat_button_new(label, NULL,
                                G_CALLBACK(app_window_tab_context_close_clicked),
                                action);
    g_object_set_data_full(G_OBJECT(button),
                           "graptos-tab-close-action",
                           action,
                           g_free);
    gtk_widget_set_sensitive(button, sensitive);
    return button;
}

/**
 * @brief Show the tab close context menu.
 * @details The menu lives at the window level because it needs notebook order,
 *          registry order, dirty-tab confirmation, and tile cleanup policy.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose label was clicked.
 * @param parent The widget that should own the popover.
 * @param x The click x coordinate in parent coordinates.
 * @param y The click y coordinate in parent coordinates.
 */
void app_window_show_tab_context_menu(EditorWindow *win,
                                      EditorTab *tab,
                                      GtkWidget *parent,
                                      double x,
                                      double y) {
    if (!win || !tab || !parent) return;

    GtkWidget *old_popover =
        g_object_get_data(G_OBJECT(parent), "graptos-tab-context-popover");
    if (old_popover && GTK_IS_POPOVER(old_popover)) {
        graptos_popover_hide(old_popover);
        graptos_widget_destroy(old_popover);
    }

    GtkWidget *popover = gtk_popover_new();
    graptos_popover_attach(popover, parent);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "graptos-context-popover");
    g_object_set_data(G_OBJECT(parent), "graptos-tab-context-popover", popover);

    guint count = app_window_tab_count(win);
    gint page = app_window_page_index_for_tab(win, tab);
    gint pages = win->notebook
        ? gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook))
        : 0;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    graptos_set_all_margins(box, 6);
    gtk_box_append(GTK_BOX(box),
                   app_window_tab_context_button(win, tab, popover,
                                                 "Close All Tabs",
                                                 APP_TAB_CLOSE_ALL,
                                                 count > 0u));
    gtk_box_append(GTK_BOX(box),
                   app_window_tab_context_button(win, tab, popover,
                                                 "Close Other Tabs",
                                                 APP_TAB_CLOSE_OTHERS,
                                                 count > 1u));
    gtk_box_append(GTK_BOX(box),
                   app_window_tab_context_button(win, tab, popover,
                                                 "Close Tabs to the Right",
                                                 APP_TAB_CLOSE_RIGHT,
                                                 page >= 0 &&
                                                 page + 1 < pages));
    gtk_box_append(GTK_BOX(box),
                   app_window_tab_context_button(win, tab, popover,
                                                 "Close Tabs to the Left",
                                                 APP_TAB_CLOSE_LEFT,
                                                 page > 0));

    GdkRectangle rect = {
        .x = (int)x,
        .y = (int)y,
        .width = 1,
        .height = 1
    };
    gtk_popover_set_child(GTK_POPOVER(popover), box);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    g_signal_connect(popover, "closed",
                     G_CALLBACK(app_window_tab_context_closed), parent);
    gtk_popover_popup(GTK_POPOVER(popover));
}


/**
 * @brief App window close tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
gboolean app_window_close_tab(EditorWindow *win, EditorTab *tab) {
    if (!win || !tab) return FALSE;

    // Give the tab a chance to stop closing if it has unsaved changes.
    if (!editor_tab_confirm_close(tab)) return FALSE;
    GPtrArray *group_to_close = NULL;
    if (tab->tile_group && tab->tile_group->len > 1u) {
        group_to_close = g_ptr_array_new();
        for (guint i = 0u; i < tab->tile_group->len; i++) {
            EditorTab *member = g_ptr_array_index(tab->tile_group, i);
            if (member && member != tab) g_ptr_array_add(group_to_close, member);
        }
        for (guint i = 0u; i < group_to_close->len; i++) {
            EditorTab *member = g_ptr_array_index(group_to_close, i);
            if (member && !editor_tab_confirm_close(member)) {
                g_ptr_array_free(group_to_close, TRUE);
                return FALSE;
            }
        }
    }

    /*
     * Remove saved-group and active-tile references before GTK page removal
     * starts emitting switch signals. This keeps later tile restoration from
     * walking a tab that is already on the close path.
     */
    app_window_forget_tile_tab(win, tab);
    if (win->tile_mode) app_window_clear_tiles(win);

    if (group_to_close) {
        for (guint i = 0u; i < group_to_close->len; i++) {
            EditorTab *member = g_ptr_array_index(group_to_close, i);
            if (!member) continue;
            gint member_page = app_window_page_index_for_tab(win, member);
            editor_tab_destroy_popovers(member);
            app_window_forget_tile_tab(win, member);
            if (member_page >= 0) {
                gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook), member_page);
            }
            app_window_unregister_tab(win, member);
            editor_tab_free(member);
        }
        g_ptr_array_free(group_to_close, TRUE);
    }

    if (tab->folded_tile_member) {
        app_window_unregister_tab(win, tab);
        editor_tab_free(tab);
    } else {
    gint n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));

    for (gint i = 0; i < n; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);

        if (child && g_object_get_data(G_OBJECT(child), "graptos-tab") == tab) {
            /*
             * Popovers can outlive normal widget focus changes, so destroy them
             * before removing the page and freeing the tab.
             */
            editor_tab_destroy_popovers(tab);

            gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook), i);
            app_window_unregister_tab(win, tab);
            editor_tab_free(tab);
            break;
        }
    }
    }

    /*
         * Keep the editor usable after closing the last tab. Graptoς always has at
     * least one editable buffer.
     */
    if (app_window_tab_count(win) == 0u) {
        EditorTab *new_tab = editor_tab_new(win);
        app_window_add_tab(win, new_tab, TRUE);
    }

    app_window_update_ui(win);
    return TRUE;
}


/**
 * @brief App window close all tabs.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean app_window_close_all_tabs(EditorWindow *win) {
    if (!win || !win->notebook) return TRUE;

    /*
     * Ask every tab first before destroying anything. This avoids closing some
     * tabs before another tab cancels because of unsaved changes.
     */
    guint count = app_window_tab_count(win);
    for (guint i = 0u; i < count; i++) {
        EditorTab *tab = app_window_tab_at(win, i);
        if (tab && !editor_tab_confirm_close(tab)) return FALSE;
    }

    app_window_clear_tiles(win);

    /*
     * Remove visible pages from the front until the notebook is empty. Folded
     * tile members are not notebook pages, but they remain in the registry and
     * are freed by the registry loop below.
     */
    while (gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook)) > 0) {
        gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook), 0);
    }

    while (app_window_tab_count(win) > 0u) {
        EditorTab *tab = app_window_tab_at(win, 0u);
        if (!tab) {
            g_ptr_array_remove_index(win->tabs, 0u);
            continue;
        }
        editor_tab_destroy_popovers(tab);
        app_window_forget_tile_tab(win, tab);
        app_window_unregister_tab(win, tab);
        editor_tab_free(tab);
    }

    return TRUE;
}


/**
 * @brief Combo index for syntax.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param syntax The syntax definition used by the editor path.
 * @return The computed value requested by the caller.
 */
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


/**
 * @brief Populate syntax combo.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void populate_syntax_combo(EditorWindow *win, EditorTab *tab) {
    if (!win || !win->syntax_combo) return;

    /*
     * Rebuilding the combo would normally emit the same changed signal used for
     * user selection. Block it so UI refresh does not change the tab syntax.
    */
    win->syntax_combo_updating = TRUE;

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

    win->syntax_combo_updating = FALSE;
}


/**
 * @brief App window update ui.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
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
            gtk_label_set_text(GTK_LABEL(win->top_title_label),
                               GRAPTOS_DISPLAY_NAME);
        }

        gtk_window_set_title(GTK_WINDOW(win->window), GRAPTOS_DISPLAY_NAME);
        app_window_set_status(win, "No open document");
        populate_syntax_combo(win, NULL);
        return;
    }

    char *base = editor_tab_basename(tab);
    char *title = g_strdup_printf("%s%s",
                                  tab->modified ? "*" : "",
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


/**
 * @brief App window reload syntaxes.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_reload_syntaxes(EditorWindow *win) {
    if (!win) return;

    /*
     * Syntax definitions are runtime YAML files. Reload the registry and make
     * open tabs re-detect their language against the new definitions.
     */
    if (win->syntaxes) {
        g_ptr_array_free(win->syntaxes, TRUE);
    }

    win->syntaxes = syntax_load_all_for_roots(win->project_roots);

    guint count = app_window_tab_count(win);
    for (guint i = 0u; i < count; i++) {
        EditorTab *tab = app_window_tab_at(win, i);

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
