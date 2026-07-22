/**
 * @file src/app/app_window_tiles.inc.c
 * @brief Graptoς editor tile-mode implementation.
 * @details Tile mode moves the real editor widgets instead of making copies. Edits,
 *          diagnostics, scrolling, and minimap state stay attached to the actual tab; the
 *          careful reparenting is the price we pay for not cloning buffers.
 */

/**
 * @brief Emit a tile-mode debug log in debug builds.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param format The format supplied by the caller.
 */
static void app_window_tile_debug(EditorWindow *win, const char *format, ...) {
    if (!win || !win->debug_mode || !format) return;
    va_list args;
    va_start(args, format);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
    char *message = g_strdup_vprintf(format, args);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    va_end(args);
    if (message) {
        g_message("Tiles: %s", message);
        g_free(message);
    }
}

/**
 * @brief Return the notebook page index for a tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The computed value requested by the caller.
 */
static gint app_window_page_index_for_tab(EditorWindow *win, EditorTab *tab) {
    if (!win || !win->notebook || !tab) return -1;
    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        if (child && g_object_get_data(G_OBJECT(child), "graptos-tab") == tab) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Register a tab in the window-owned tab registry.
 * @details The notebook only tracks visible pages. Saved tile members can be
 *          folded out of the notebook, so this registry is the durable owner
 *          list used by whole-window operations.
 * @param win The window that owns the tab.
 * @param tab The tab to register.
 */
void app_window_register_tab(EditorWindow *win, EditorTab *tab) {
    if (!win || !tab) return;
    if (!win->tabs) win->tabs = g_ptr_array_new();
    for (guint i = 0u; i < win->tabs->len; i++) {
        if (g_ptr_array_index(win->tabs, i) == tab) return;
    }
    g_ptr_array_add(win->tabs, tab);
    app_window_tile_debug(win,
                          "registry add tab=%p count=%u",
                          (void *)tab,
                          win->tabs->len);
}

/**
 * @brief Remove a tab from the window-owned tab registry.
 * @details Unregistering is separate from freeing so notebook folding can move
 *          pages around without accidentally destroying the document model.
 * @param win The window that owns the tab.
 * @param tab The tab to unregister.
 */
void app_window_unregister_tab(EditorWindow *win, EditorTab *tab) {
    if (!win || !win->tabs || !tab) return;
    while (g_ptr_array_remove(win->tabs, tab)) {
        app_window_tile_debug(win,
                              "registry remove tab=%p count=%u",
                              (void *)tab,
                              win->tabs->len);
    }
}

/**
 * @brief Return the number of registered tabs.
 * @details Folded tile members remain open documents, so they stay in this
 *          count even while their notebook page is hidden.
 * @param win The window whose tab registry is being inspected.
 * @return Number of registered tabs.
 */
guint app_window_tab_count(EditorWindow *win) {
    return (win && win->tabs) ? win->tabs->len : 0u;
}

/**
 * @brief Return a registered tab by index.
 * @details Callers use this when they need all open tabs rather than only the
 *          pages GTK is currently presenting in the notebook.
 * @param win The window whose registry is being inspected.
 * @param index Registry index.
 * @return The tab at index, or NULL.
 */
EditorTab *app_window_tab_at(EditorWindow *win, guint index) {
    if (!win || !win->tabs || index >= win->tabs->len) return NULL;
    return g_ptr_array_index(win->tabs, index);
}

/**
 * @brief Return whether a tab is already selected for tile mode.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean app_window_tile_contains(EditorWindow *win, EditorTab *tab) {
    if (!win || !win->tile_tabs || !tab) return FALSE;
    for (guint i = 0u; i < win->tile_tabs->len; i++) {
        if (g_ptr_array_index(win->tile_tabs, i) == tab) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Return whether a saved tile group contains a tab.
 * @details Saved groups are stored on their owner tab but a user can focus any
 *          member pane. This helper keeps owner lookups explicit instead of
 *          treating the focused member as the group identity.
 * @param owner The possible saved-group owner.
 * @param tab The tab to look for inside the saved group.
 * @return TRUE when owner owns a group containing tab.
 */
static gboolean app_window_saved_group_contains(EditorTab *owner, EditorTab *tab) {
    if (!owner || !owner->tile_group || !tab) return FALSE;
    for (guint i = 0u; i < owner->tile_group->len; i++) {
        if (g_ptr_array_index(owner->tile_group, i) == tab) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Find the saved tile-group owner for a member tab.
 * @details The visible host must stay stable after a group is saved. Without
 *          this lookup, focusing a member pane can accidentally promote that
 *          member to host and leave its notebook tab visible.
 * @param win The application window that owns the notebook.
 * @param tab The focused or clicked tile member.
 * @return The owner tab for tab, or NULL when tab is not in a saved group.
 */
static EditorTab *app_window_group_owner_for_tab(EditorWindow *win, EditorTab *tab) {
    if (!win || !tab) return NULL;
    guint count = app_window_tab_count(win);
    for (guint i = 0u; i < count; i++) {
        EditorTab *owner = app_window_tab_at(win, i);
        if (app_window_saved_group_contains(owner, tab)) return owner;
    }
    return NULL;
}

/**
 * @brief Return whether a tab already belongs to a saved tile group.
 * @details Saved groups are already a compound tab. Feeding one back into a new
 *          Shift-tile selection creates nested ownership, which makes page
 *          folding ambiguous and leaves duplicate or black editor pages.
 * @param win The application window that owns the saved groups.
 * @param tab The tab being considered for a new tile selection.
 * @return TRUE when tab is a saved group host or member.
 */
static gboolean app_window_tab_is_saved_tile_group_part(EditorWindow *win,
                                                        EditorTab *tab) {
    EditorTab *owner = app_window_group_owner_for_tab(win, tab);
    return owner && owner->tile_group && owner->tile_group->len > 1u;
}

/**
 * @brief Reject nested tile-group creation.
 * @details One saved group maps to one notebook tab. Keeping that invariant
 *          prevents a visible grouped tab from being folded into another group,
 *          which was the source of the broken duplicate-tab and black-editor
 *          states.
 * @param win The application window that owns tile state.
 * @param first First tab in the attempted tile selection.
 * @param second Second tab in the attempted tile selection.
 * @return TRUE when the attempted selection should stop.
 */
static gboolean app_window_reject_saved_group_retile(EditorWindow *win,
                                                     EditorTab *first,
                                                     EditorTab *second) {
    if (!win) return TRUE;
    if (!app_window_tab_is_saved_tile_group_part(win, first) &&
        !app_window_tab_is_saved_tile_group_part(win, second)) {
        return FALSE;
    }
    app_window_set_status(win, "Saved tile groups cannot be tiled again; detach the group first");
    app_window_tile_debug(win,
                          "retile rejected first=%p second=%p",
                          (void *)first,
                          (void *)second);
    return TRUE;
}

/**
 * @brief Resolve the stable host for the current tile layout.
 * @details Unsaved tile selections can use the active pane as host, but saved
 *          groups need one owner tab. The owner keeps the notebook compact and
 *          lets repeated saves update the same group instead of creating a
 *          second group on a member tab.
 * @param win The application window that owns the tile state.
 * @param fallback The tab to use when no saved owner exists.
 * @return The saved group owner, or fallback for an unsaved tile selection.
 */
static EditorTab *app_window_stable_tile_host(EditorWindow *win, EditorTab *fallback) {
    EditorTab *owner = app_window_group_owner_for_tab(win, fallback);
    return owner ? owner : fallback;
}

/**
 * @brief Context carried by each tile header menu.
 * @details Tile header actions need both the group host and the pane tab. We
 *          keep the pair on the header widget so rebuilt tile layouts free the
 *          small context naturally with the old header.
 */
typedef struct {
    EditorTab *host; /**< Tab that owns the saved tile group. */
    EditorTab *tab;  /**< Pane tab under the right-click menu. */
} TileMenuData;

/**
 * @brief Move the active tile minimap into the visible tile strip.
 * @details Tile pane focus can happen before the minimap helper definition in
 *          this include file, so the forward declaration keeps the single
 *          implementation below without relying on implicit C declarations.
 * @param win The window that owns the tile state.
 * @param host The visible tile host page.
 */
static void app_window_place_tile_minimap(EditorWindow *win, EditorTab *host);

/**
 * @brief Remove a tab from the tile selection.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void app_window_tile_remove(EditorWindow *win, EditorTab *tab) {
    if (!win || !win->tile_tabs || !tab) return;
    for (guint i = 0u; i < win->tile_tabs->len; i++) {
        if (g_ptr_array_index(win->tile_tabs, i) == tab) {
            g_ptr_array_remove_index(win->tile_tabs, i);
            app_window_tile_debug(win, "remove tab=%p count=%u", (void *)tab, win->tile_tabs->len);
            return;
        }
    }
}

/**
 * @brief Show or hide notebook labels for tabs owned by a saved group.
 * @details A saved tile group should read as one notebook tab. The member tabs
 *          stay alive as notebook pages because their buffers, undo stacks, LSP
 *          state, and widgets belong to those EditorTab objects.
 * @param win The application window that owns the notebook.
 * @param host The tab whose tile_group owns the member tabs.
 * @param visible TRUE to show member labels again; FALSE to fold them into host.
 */
static void app_window_set_group_member_tabs_visible(EditorWindow *win,
                                                     EditorTab *host,
                                                     gboolean visible);

/**
 * @brief Move a widget between boxes while preserving its reference.
 * @details Tile mode reparents real editor widgets. GTK containers drop their
 *          child reference during removal, so we hold a temporary reference
 *          across the move rather than hoping the widget survives the gap.
 * @param child The child supplied by the caller.
 * @param old_parent The old parent supplied by the caller.
 * @param new_parent The new parent supplied by the caller.
 */
static void app_window_reparent_box_child(GtkWidget *child,
                                          GtkWidget *old_parent,
                                          GtkWidget *new_parent) {
    if (!child || !GTK_IS_WIDGET(child) ||
        !new_parent || !GTK_IS_WIDGET(new_parent) ||
        old_parent == new_parent) {
        return;
    }
    g_object_ref(child);
    if (old_parent && GTK_IS_BOX(old_parent)) gtk_box_remove(GTK_BOX(old_parent), child);
    gtk_box_append(GTK_BOX(new_parent), child);
    g_object_unref(child);
}

/**
 * @brief Fold one tile member out of the visible notebook.
 * @details A grouped tile should be represented by one notebook tab. Removing
 *          the member page is safe only while holding explicit references to
 *          both the page and its tab label; otherwise GTK may dispose the
 *          widgets that still belong to the EditorTab.
 * @param win The window that owns the notebook.
 * @param host The visible group host.
 * @param tab The member tab to fold out of the notebook.
 */
static void app_window_fold_tile_member(EditorWindow *win,
                                        EditorTab *host,
                                        EditorTab *tab) {
    if (!win || !win->notebook || !host || !tab || tab == host) return;
    if (tab->folded_tile_member) return;

    gint index = app_window_page_index_for_tab(win, tab);
    if (index < 0) return;

    if (tab->box && GTK_IS_WIDGET(tab->box) &&
        tab->tab_widget && GTK_IS_WIDGET(tab->tab_widget)) {
        g_object_ref(tab->box);
        g_object_ref(tab->tab_widget);
        tab->notebook_refs_held = TRUE;
        tab->folded_tile_member = TRUE;
        win->tile_internal_switch = TRUE;
        gtk_notebook_remove_page(GTK_NOTEBOOK(win->notebook), index);
        win->tile_internal_switch = FALSE;
        app_window_tile_debug(win,
                              "fold member tab=%p host=%p",
                              (void *)tab,
                              (void *)host);
    }
}

/**
 * @brief Reinsert one folded tile member as a normal notebook page.
 * @details Unfolding gives GTK ownership of the page and label again, so the
 *          temporary references taken during folding are released immediately
 *          after the page has been appended back to the notebook.
 * @param win The window that owns the notebook.
 * @param tab The folded tab to make visible again.
 * @param switch_to_tab TRUE when the notebook should select the restored page.
 */
static void app_window_unfold_tile_member(EditorWindow *win,
                                          EditorTab *tab,
                                          gboolean switch_to_tab) {
    if (!win || !win->notebook || !tab || !tab->folded_tile_member) return;
    if (!tab->box || !GTK_IS_WIDGET(tab->box) ||
        !tab->tab_widget || !GTK_IS_WIDGET(tab->tab_widget)) {
        return;
    }

    g_object_set_data(G_OBJECT(tab->box), "graptos-tab", tab);
    gint index = gtk_notebook_append_page(GTK_NOTEBOOK(win->notebook),
                                          tab->box,
                                          tab->tab_widget);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(win->notebook),
                                     tab->box,
                                     TRUE);
    gtk_widget_set_visible(tab->box, TRUE);
    gtk_widget_set_visible(tab->tab_widget, TRUE);
    tab->folded_tile_member = FALSE;
    if (tab->notebook_refs_held) {
        tab->notebook_refs_held = FALSE;
        g_object_unref(tab->tab_widget);
        g_object_unref(tab->box);
    }
    if (switch_to_tab && index >= 0) {
        win->tile_internal_switch = TRUE;
        gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), index);
        win->tile_internal_switch = FALSE;
        win->active_tab = tab;
    }
    app_window_tile_debug(win,
                          "unfold member tab=%p switch=%d",
                          (void *)tab,
                          switch_to_tab ? 1 : 0);
}

/**
 * @brief Fold or unfold saved tile-group members.
 * @details This replaces hidden tab labels with real notebook membership
 *          changes. The group host stays visible; every other member is either
 *          removed from or restored to the notebook with explicit references.
 * @param win The window that owns the notebook.
 * @param host The tab whose group is being presented.
 * @param visible TRUE to unfold members, FALSE to fold them.
 */
static void app_window_set_group_member_tabs_visible(EditorWindow *win,
                                                     EditorTab *host,
                                                     gboolean visible) {
    if (!win || !host || !host->tile_group) return;
    for (guint i = 0u; i < host->tile_group->len; i++) {
        EditorTab *tab = g_ptr_array_index(host->tile_group, i);
        if (!tab || tab == host) continue;
        if (visible) {
            app_window_unfold_tile_member(win, tab, FALSE);
        } else {
            app_window_fold_tile_member(win, host, tab);
        }
    }
}

/**
 * @brief Restore a tab minimap to its normal editor-area position.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void app_window_restore_tab_minimap(EditorTab *tab) {
    if (!tab || !tab->minimap_scrolled || !GTK_IS_WIDGET(tab->minimap_scrolled) ||
        !tab->editor_content || !GTK_IS_WIDGET(tab->editor_content)) {
        return;
    }
    GtkWidget *parent = gtk_widget_get_parent(tab->minimap_scrolled);
    if (parent == tab->editor_content) return;
    g_object_ref(tab->minimap_scrolled);
    if (parent && GTK_IS_BOX(parent)) gtk_box_remove(GTK_BOX(parent), tab->minimap_scrolled);
    gtk_box_insert_child_after(GTK_BOX(tab->editor_content),
                               tab->minimap_scrolled,
                               tab->popover_parent);
    g_object_unref(tab->minimap_scrolled);
}

/**
 * @brief Return the tile whose minimap should be shown.
 * @details Tile mode has one minimap, not one minimap per pane. The active
 *          tile owns that minimap so the preview follows the pane the user is
 *          editing instead of staying attached to the original host page.
 * @param win The application window that owns the tile state.
 * @param host The fallback host tab for the visible tile page.
 * @return The active tiled tab, or the host when no active tile is known.
 */
static EditorTab *app_window_active_tile_tab(EditorWindow *win, EditorTab *host) {
    if (win && win->active_tab && app_window_tile_contains(win, win->active_tab)) {
        return win->active_tab;
    }
    return host;
}

/**
 * @brief Add a tab to the tile selection if there is capacity.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean app_window_tile_add(EditorWindow *win, EditorTab *tab) {
    if (!win || !tab) return FALSE;
    if (!win->tile_tabs) win->tile_tabs = g_ptr_array_new();
    if (app_window_tile_contains(win, tab)) return TRUE;
    guint max_tabs = win->tile_max_tabs > 0u ? win->tile_max_tabs : 3u;
    if (win->tile_tabs->len >= max_tabs) {
        char *msg = g_strdup_printf("Tile limit reached (%u)", max_tabs);
        app_window_set_status(win, msg);
        g_free(msg);
        app_window_tile_debug(win,
                              "add rejected limit=%u tab=%p count=%u",
                              max_tabs,
                              (void *)tab,
                              win->tile_tabs->len);
        return FALSE;
    }
    g_ptr_array_add(win->tile_tabs, tab);
    app_window_tile_debug(win,
                          "add tab=%p count=%u max=%u",
                          (void *)tab,
                          win->tile_tabs->len,
                          max_tabs);
    return TRUE;
}

/**
 * @brief Mark tab labels that are part of the active tile selection.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
static void app_window_update_tile_tab_css(EditorWindow *win) {
    if (!win || !win->notebook) return;
    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        EditorTab *tab = child ? g_object_get_data(G_OBJECT(child), "graptos-tab") : NULL;
        if (!tab || !tab->tab_widget) continue;
        if (win->tile_mode && app_window_tile_contains(win, tab)) {
            gtk_widget_add_css_class(tab->tab_widget, "graptos-tab-tiled");
        } else {
            gtk_widget_remove_css_class(tab->tab_widget, "graptos-tab-tiled");
        }
    }
}

/**
 * @brief Restore the current tile host page to its normal editor layout.
 * @details Leaving tile mode has to undo every reparent done while the group
 *          was visible. We restore editor areas and minimaps before removing
 *          the tile box so later tab code sees the normal layout again.
 * @param win The win supplied by the caller.
 */
static void app_window_restore_tile_host(EditorWindow *win) {
    if (!win || !win->tile_box || !GTK_IS_WIDGET(win->tile_box)) return;
    app_window_tile_debug(win, "restore host=%p", (void *)win->tile_host_tab);
    if (win->tile_tabs) {
        for (guint i = 0u; i < win->tile_tabs->len; i++) {
            EditorTab *tab = g_ptr_array_index(win->tile_tabs, i);
            if (!tab || !tab->editor_area || !GTK_IS_WIDGET(tab->editor_area) ||
                !tab->box || !GTK_IS_WIDGET(tab->box)) {
                continue;
            }
            GtkWidget *editor_parent = gtk_widget_get_parent(tab->editor_area);
            if (editor_parent && editor_parent != tab->box) {
                app_window_reparent_box_child(tab->editor_area,
                                              editor_parent,
                                              tab->box);
            }
            app_window_restore_tab_minimap(tab);
            editor_tab_set_minimap_visible(tab, win->minimap_enabled);
        }
    }
    if (win->tile_host_tab && win->tile_host_tab->box &&
        GTK_IS_WIDGET(win->tile_host_tab->box) &&
        gtk_widget_get_parent(win->tile_box) == win->tile_host_tab->box) {
        gtk_box_remove(GTK_BOX(win->tile_host_tab->box), win->tile_box);
    }
    win->tile_box = NULL;
    win->tile_host_tab = NULL;
}

/**
 * @brief Focus a tiled tab and rebuild the tile host around it.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param gesture The gesture supplied by the caller.
 * @param n_press The n press supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void app_window_focus_tiled_tab(GtkGestureClick *gesture,
                                       int n_press,
                                       double x,
                                       double y,
                                       gpointer user_data) {
    (void)gesture;
    (void)n_press;
    (void)x;
    (void)y;
    EditorTab *tab = user_data;
    if (!tab || !tab->win) return;
    EditorWindow *win = tab->win;
    gboolean changed = win->active_tab != tab;
    win->active_tab = tab;
    if (tab->text_view && GTK_IS_WIDGET(tab->text_view)) {
        gtk_widget_grab_focus(tab->text_view);
    }
    if (changed) app_window_place_tile_minimap(win, win->tile_host_tab);
    app_window_update_ui(win);
}

/**
 * @brief Remove a tile pane from the current tile group.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param button The button that emitted the action signal.
 * @param user_data The callback context passed through GTK signal data.
 */
static void app_window_remove_tile_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    EditorTab *tab = user_data;
    if (!tab || !tab->win) return;
    EditorWindow *win = tab->win;
    app_window_tile_remove(win, tab);
    if (!win->tile_tabs || win->tile_tabs->len < 2u) {
        app_window_clear_tiles(win);
        return;
    }
    if (win->active_tab == tab) win->active_tab = g_ptr_array_index(win->tile_tabs, 0u);
    app_window_update_tiles(win);
}

/**
 * @brief Save the current tile group under the host tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The application window that owns the current tile selection.
 * @param host The tab that will own the saved tile group.
 */
static void app_window_save_tile_group(EditorWindow *win, EditorTab *host) {
    if (!host || !win || !win->tile_tabs || win->tile_tabs->len < 2u) return;
    host = app_window_stable_tile_host(win, host);
    if (!host) return;
    if (host->tile_group && host->tile_group->len > 1u) {
        app_window_set_group_member_tabs_visible(win, host, TRUE);
    }
    if (!host->tile_group) host->tile_group = g_ptr_array_new();
    g_ptr_array_set_size(host->tile_group, 0u);
    for (guint i = 0u; i < win->tile_tabs->len; i++) {
        g_ptr_array_add(host->tile_group, g_ptr_array_index(win->tile_tabs, i));
    }
    editor_tab_update_title(host);
    app_window_set_group_member_tabs_visible(win, host, FALSE);
    app_window_tile_debug(win,
                          "saved group host=%p count=%u",
                          (void *)host,
                          host->tile_group->len);
    app_window_set_status(win, "Tile group saved");
    win->active_tab = host;
    gint index = app_window_page_index_for_tab(win, host);
    if (index >= 0) {
        win->tile_internal_switch = TRUE;
        gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), index);
        win->tile_internal_switch = FALSE;
    }
    win->tile_mode = TRUE;
    app_window_update_tiles(win);
}

/**
 * @brief Save the current tile group from a tile context menu item.
 * @details Saving from the context menu avoids duplicate pane-level save
 *          buttons and makes the saved group feel like a tab operation. The
 *          real group is still stored on the host tab so it can reopen later
 *          from that single tab.
 * @param button The menu button activated by the user.
 * @param user_data The host editor tab that will own the group.
 */
static void app_window_save_tile_group_menu_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    TileMenuData *data = user_data;
    EditorTab *host = data ? data->host : NULL;
    EditorWindow *win = host ? host->win : NULL;
    host = win ? app_window_stable_tile_host(win, host) : host;
    app_window_save_tile_group(win, host);
}

/**
 * @brief Detach one tab from a saved tile group.
 * @details Detaching restores the tab's notebook label and removes it from
 *          both the saved group and the visible tile set. If fewer than two
 *          tabs remain, the group is no longer meaningful and is dissolved.
 * @param button The menu button activated by the user.
 * @param user_data Tile menu data containing the host and target tab.
 */
static void app_window_detach_tile_group_menu_clicked(GtkButton *button,
                                                      gpointer user_data) {
    (void)button;
    TileMenuData *data = user_data;
    EditorTab *host = data ? data->host : NULL;
    EditorTab *tab = data ? data->tab : NULL;
    EditorWindow *win = host ? host->win : NULL;
    if (!win || !host || !tab || !host->tile_group) return;

    if (tab == host) {
        app_window_set_group_member_tabs_visible(win, host, TRUE);
        g_ptr_array_set_size(host->tile_group, 0u);
        editor_tab_update_title(host);
        app_window_clear_tiles(win);
        app_window_set_status(win, "Tile group detached");
        return;
    }

    while (g_ptr_array_remove(host->tile_group, tab)) {
        app_window_tile_debug(win,
                              "detach saved tab=%p host=%p",
                              (void *)tab,
                              (void *)host);
    }
    app_window_restore_tab_minimap(tab);
    if (tab->editor_area && GTK_IS_WIDGET(tab->editor_area) &&
        tab->box && GTK_IS_WIDGET(tab->box)) {
        GtkWidget *editor_parent = gtk_widget_get_parent(tab->editor_area);
        if (editor_parent && editor_parent != tab->box) {
            app_window_reparent_box_child(tab->editor_area,
                                          editor_parent,
                                          tab->box);
        }
    }
    app_window_unfold_tile_member(win, tab, FALSE);

    if (host->tile_group->len < 2u) {
        app_window_set_group_member_tabs_visible(win, host, TRUE);
        g_ptr_array_set_size(host->tile_group, 0u);
        editor_tab_update_title(host);
        app_window_clear_tiles(win);
        app_window_set_status(win, "Tile group detached");
        return;
    }

    app_window_restore_tile_host(win);
    app_window_tile_remove(win, tab);
    editor_tab_update_title(host);
    win->active_tab = host;
    app_window_update_tiles(win);
    app_window_set_status(win, "Tab detached from tile group");
}

/**
 * @brief Open the tile context menu under a secondary click.
 * @details Tile group saving belongs to the tab grouping workflow, not to each
 *          editor pane. A small context menu keeps the pane header clean while
 *          still making the operation available from any visible tile.
 * @param gesture The secondary-click gesture that opened the menu.
 * @param n_press Number of presses in the current gesture.
 * @param x Click x-coordinate inside the tile header.
 * @param y Click y-coordinate inside the tile header.
 * @param user_data Tile menu data containing the host and clicked pane tab.
 */
static void app_window_tile_context_pressed(GtkGestureClick *gesture,
                                            int n_press,
                                            double x,
                                            double y,
                                            gpointer user_data) {
    (void)n_press;
    TileMenuData *data = user_data;
    EditorTab *host = data ? data->host : NULL;
    if (!gesture || !host || !host->win) return;

    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    if (!widget) return;

    GtkWidget *popover = gtk_popover_new();
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "graptos-popover");
    gtk_widget_set_parent(popover, widget);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *save = graptos_flat_button_new("Save Tile Group",
                                              "Save this tile group under one tab",
                                              NULL,
                                              NULL);
    gtk_widget_add_css_class(save, "graptos-menu-item");
    g_signal_connect(save, "clicked",
                     G_CALLBACK(app_window_save_tile_group_menu_clicked), data);
    gtk_box_append(GTK_BOX(box), save);
    host = app_window_stable_tile_host(host->win, host);
    if (host && host->tile_group && host->tile_group->len > 1u) {
        GtkWidget *detach = graptos_flat_button_new(
            data && data->tab == host ? "Detach Tile Group" : "Detach This Tab",
            data && data->tab == host
                ? "Dissolve this saved tile group"
                : "Remove this tab from the saved tile group",
            NULL,
            NULL);
        gtk_widget_add_css_class(detach, "graptos-menu-item");
        g_signal_connect(detach, "clicked",
                         G_CALLBACK(app_window_detach_tile_group_menu_clicked), data);
        gtk_box_append(GTK_BOX(box), detach);
    }
    gtk_popover_set_child(GTK_POPOVER(popover), box);

    GdkRectangle rect = { (int)x, (int)y, 1, 1 };
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    gtk_popover_popup(GTK_POPOVER(popover));
}

/**
 * @brief Create one tile pane for a real editor tab area.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param host The host supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *app_window_create_tile_pane(EditorWindow *win,
                                             EditorTab *host,
                                             EditorTab *tab) {
    if (!tab || !tab->editor_area) return gtk_label_new("");
    GtkWidget *pane = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(pane, "graptos-editor-tile-pane");
    gtk_widget_set_hexpand(pane, TRUE);
    gtk_widget_set_vexpand(pane, TRUE);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(header, "graptos-editor-tile-title");
    char *title = editor_tab_basename(tab);
    GtkWidget *label = gtk_label_new(title ? title : "Untitled");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(header), label);
    if (win->tile_tabs && win->tile_tabs->len > 1u) {
        GtkWidget *remove = graptos_flat_button_new(
            "×",
            "Remove this tab from the tile group",
            G_CALLBACK(app_window_remove_tile_clicked),
            tab);
        gtk_widget_add_css_class(remove, "graptos-tab-close");
        gtk_box_append(GTK_BOX(header), remove);
    }
    GtkGesture *click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_PRIMARY);
    g_signal_connect(click, "pressed", G_CALLBACK(app_window_focus_tiled_tab), tab);
    gtk_widget_add_controller(header, GTK_EVENT_CONTROLLER(click));
    GtkGesture *pane_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(pane_click), GDK_BUTTON_PRIMARY);
    g_signal_connect(pane_click, "pressed", G_CALLBACK(app_window_focus_tiled_tab), tab);
    gtk_widget_add_controller(pane, GTK_EVENT_CONTROLLER(pane_click));
    GtkGesture *context = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(context), GDK_BUTTON_SECONDARY);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(context),
                                               GTK_PHASE_CAPTURE);
    TileMenuData *menu_data = g_new0(TileMenuData, 1);
    menu_data->host = app_window_stable_tile_host(win, host);
    menu_data->tab = tab;
    g_object_set_data_full(G_OBJECT(header),
                           "graptos-tile-menu-data",
                           menu_data,
                           g_free);
    g_signal_connect(context, "pressed",
                     G_CALLBACK(app_window_tile_context_pressed), menu_data);
    gtk_widget_add_controller(header, GTK_EVENT_CONTROLLER(context));
    gtk_box_append(GTK_BOX(pane), header);
    g_free(title);

    if (tab->editor_area && GTK_IS_WIDGET(tab->editor_area)) {
        GtkWidget *editor_parent = gtk_widget_get_parent(tab->editor_area);
        app_window_reparent_box_child(tab->editor_area, editor_parent, pane);
        editor_tab_set_minimap_visible(tab, FALSE);
    }

    return pane;
}

/**
 * @brief Create adjustable splitters for tile panes.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param host The host supplied by the caller.
 * @param index The index supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *app_window_create_tile_split(EditorWindow *win,
                                              EditorTab *host,
                                              guint index) {
    if (!win || !win->tile_tabs || index >= win->tile_tabs->len) {
        return gtk_label_new("");
    }
    EditorTab *tab = g_ptr_array_index(win->tile_tabs, index);
    GtkWidget *pane = app_window_create_tile_pane(win, host, tab);
    if (index + 1u >= win->tile_tabs->len) return pane;

    GtkWidget *split = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_hexpand(split, TRUE);
    gtk_widget_set_vexpand(split, TRUE);
    gtk_paned_set_start_child(GTK_PANED(split), pane);
    gtk_paned_set_end_child(GTK_PANED(split),
                            app_window_create_tile_split(win, host, index + 1u));
    gtk_paned_set_resize_start_child(GTK_PANED(split), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(split), TRUE);
    gtk_paned_set_shrink_start_child(GTK_PANED(split), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(split), FALSE);
    return split;
}

/**
 * @brief Move the active minimap to the far right of tile mode.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param host The host supplied by the caller.
 */
static void app_window_place_tile_minimap(EditorWindow *win, EditorTab *host) {
    if (!win || !host) return;
    EditorTab *minimap_tab = app_window_active_tile_tab(win, host);
    if (win->tile_tabs) {
        for (guint i = 0u; i < win->tile_tabs->len; i++) {
            EditorTab *tab = g_ptr_array_index(win->tile_tabs, i);
            if (tab) editor_tab_set_minimap_visible(tab, FALSE);
        }
    }
    if (!minimap_tab || !minimap_tab->minimap_scrolled ||
        !GTK_IS_WIDGET(minimap_tab->minimap_scrolled)) {
        return;
    }
    if (!win->minimap_enabled) {
        editor_tab_set_minimap_visible(minimap_tab, FALSE);
        return;
    }
    GtkWidget *parent = gtk_widget_get_parent(minimap_tab->minimap_scrolled);
    app_window_reparent_box_child(minimap_tab->minimap_scrolled, parent, win->tile_box);
    editor_tab_set_minimap_visible(minimap_tab, TRUE);
    app_window_tile_debug(win,
                          "minimap tab=%p host=%p",
                          (void *)minimap_tab,
                          (void *)host);
}

/**
 * @brief Rebuild the visible tile layout in the current tab page.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_update_tiles(EditorWindow *win) {
    if (!win || !win->notebook || !win->tile_mode || !win->tile_tabs ||
        win->tile_tabs->len < 2u) {
        app_window_tile_debug(win,
                              "update clears mode=%d count=%u",
                              win && win->tile_mode ? 1 : 0,
                              win && win->tile_tabs ? win->tile_tabs->len : 0u);
        app_window_clear_tiles(win);
        return;
    }

    EditorTab *host = win->active_tab && app_window_tile_contains(win, win->active_tab)
        ? win->active_tab
        : app_window_current_tab(win);
    host = app_window_stable_tile_host(win, host);
    app_window_tile_debug(win,
                          "update requested current=%p count=%u",
                          (void *)host,
                          win->tile_tabs->len);
    if (!host || !app_window_tile_contains(win, host)) {
        host = g_ptr_array_index(win->tile_tabs, 0u);
        gint index = app_window_page_index_for_tab(win, host);
        if (index >= 0) {
            win->tile_internal_switch = TRUE;
            gtk_notebook_set_current_page(GTK_NOTEBOOK(win->notebook), index);
            win->tile_internal_switch = FALSE;
        }
    }
    if (!host) return;

    app_window_restore_tile_host(win);
    editor_tab_set_minimap_visible(host, win->minimap_enabled);

    win->tile_host_tab = host;
    win->tile_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);
    gtk_widget_add_css_class(win->tile_box, "graptos-editor-tiles");
    gtk_widget_set_hexpand(win->tile_box, TRUE);
    gtk_widget_set_vexpand(win->tile_box, TRUE);

    gtk_box_append(GTK_BOX(host->box), win->tile_box);

    gtk_box_append(GTK_BOX(win->tile_box), app_window_create_tile_split(win, host, 0u));
    app_window_place_tile_minimap(win, host);

    app_window_update_tile_tab_css(win);
    app_window_tile_debug(win, "updated host=%p count=%u", (void *)host, win->tile_tabs->len);
}

/**
 * @brief Leave tile mode and restore the host tab layout.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_clear_tiles(EditorWindow *win) {
    if (!win) return;
    app_window_tile_debug(win,
                          "clear mode=%d count=%u",
                          win->tile_mode ? 1 : 0,
                          win->tile_tabs ? win->tile_tabs->len : 0u);
    app_window_restore_tile_host(win);
    win->tile_mode = FALSE;
    if (win->tile_tabs) g_ptr_array_set_size(win->tile_tabs, 0u);
    app_window_update_tile_tab_css(win);
}

/**
 * @brief Restore a saved tile group for a host tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param host The host supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean app_window_restore_saved_tile_group(EditorWindow *win, EditorTab *host) {
    if (!win || !host) return FALSE;
    host = app_window_stable_tile_host(win, host);
    if (!host || !host->tile_group || host->tile_group->len < 2u) return FALSE;
    if (!win->tile_tabs) win->tile_tabs = g_ptr_array_new();
    g_ptr_array_set_size(win->tile_tabs, 0u);
    for (guint i = 0u; i < host->tile_group->len; i++) {
        EditorTab *tab = g_ptr_array_index(host->tile_group, i);
        if (tab) app_window_tile_add(win, tab);
    }
    win->tile_mode = win->tile_tabs->len >= 2u;
    win->active_tab = host;
    app_window_set_group_member_tabs_visible(win, host, FALSE);
    app_window_tile_debug(win,
                          "restore saved host=%p count=%u",
                          (void *)host,
                          win->tile_tabs ? win->tile_tabs->len : 0u);
    app_window_update_tiles(win);
    return win->tile_mode;
}

/**
 * @brief Remove a closing tab from the active tile set and saved groups.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param closing_tab The closing tab supplied by the caller.
 */
void app_window_forget_tile_tab(EditorWindow *win, EditorTab *closing_tab) {
    if (!win || !closing_tab) return;
    if (closing_tab->tile_group && closing_tab->tile_group->len > 1u) {
        app_window_set_group_member_tabs_visible(win, closing_tab, TRUE);
    }
    if (win->tile_tabs) {
        while (g_ptr_array_remove(win->tile_tabs, closing_tab)) {
            app_window_tile_debug(win,
                                  "forgot closing active tile=%p count=%u",
                                  (void *)closing_tab,
                                  win->tile_tabs->len);
        }
    }
    gint pages = win->notebook
        ? gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook))
        : 0;
    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        EditorTab *owner = child ? g_object_get_data(G_OBJECT(child), "graptos-tab") : NULL;
        if (!owner || owner == closing_tab || !owner->tile_group) continue;
        gboolean changed = FALSE;
        while (g_ptr_array_remove(owner->tile_group, closing_tab)) changed = TRUE;
        if (owner->tile_group->len < 2u) {
            app_window_set_group_member_tabs_visible(win, owner, TRUE);
            g_ptr_array_set_size(owner->tile_group, 0u);
        }
        if (changed) editor_tab_update_title(owner);
    }
    if (win->active_tab == closing_tab) win->active_tab = NULL;
    if (win->tile_host_tab == closing_tab) win->tile_host_tab = NULL;
    if (win->tile_suppress_label_tab == closing_tab) win->tile_suppress_label_tab = NULL;
}

/**
 * @brief Handle normal and Shift tab-label clicks.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param state The state supplied by the caller.
 */
void app_window_handle_tab_label_click(EditorWindow *win,
                                       EditorTab *tab,
                                       GdkModifierType state) {
    if (!win || !tab || !win->notebook) return;
    gboolean shift = (state & GDK_SHIFT_MASK) != 0;
    win->tile_shift_down = shift;
    app_window_tile_debug(win,
                          "label click tab=%p state=0x%x shift=%d current=%p",
                          (void *)tab,
                          (guint)state,
                          shift ? 1 : 0,
                          (void *)app_window_current_tab(win));
    if (win->tile_suppress_label_tab == tab) {
        app_window_tile_debug(win, "label click suppressed tab=%p", (void *)tab);
        win->tile_suppress_label_tab = NULL;
        return;
    }
    win->tile_suppress_label_tab = NULL;
    if (!shift) {
        app_window_clear_tiles(win);
        return;
    }

    EditorTab *current = app_window_current_tab(win);
    if (app_window_reject_saved_group_retile(win, current, tab)) {
        win->tile_shift_down = FALSE;
        return;
    }
    if (!win->tile_tabs) win->tile_tabs = g_ptr_array_new();
    if (!win->tile_mode && current && current != tab) {
        g_ptr_array_set_size(win->tile_tabs, 0u);
        app_window_tile_add(win, current);
    }

    if (app_window_tile_contains(win, tab)) {
        app_window_tile_remove(win, tab);
    } else {
        app_window_tile_add(win, tab);
    }

    win->tile_mode = win->tile_tabs && win->tile_tabs->len >= 2u;
    app_window_update_tiles(win);
}

/**
 * @brief Tile the previous active tab with a newly switched tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param previous The previous supplied by the caller.
 * @param switched The switched supplied by the caller.
 */
void app_window_handle_shift_tab_switch(EditorWindow *win,
                                        EditorTab *previous,
                                        EditorTab *switched) {
    if (!win || !previous || !switched || previous == switched) return;
    app_window_tile_debug(win,
                          "shift switch previous=%p switched=%p mode=%d count=%u",
                          (void *)previous,
                          (void *)switched,
                          win->tile_mode ? 1 : 0,
                          win->tile_tabs ? win->tile_tabs->len : 0u);
    if (app_window_reject_saved_group_retile(win, previous, switched)) {
        app_window_clear_tiles(win);
        win->active_tab = switched;
        win->tile_shift_down = FALSE;
        win->tile_suppress_label_tab = switched;
        return;
    }
    if (!win->tile_tabs) win->tile_tabs = g_ptr_array_new();
    if (!win->tile_mode) g_ptr_array_set_size(win->tile_tabs, 0u);
    app_window_tile_add(win, previous);
    app_window_tile_add(win, switched);
    win->tile_mode = win->tile_tabs->len >= 2u;
    win->active_tab = switched;
    win->tile_suppress_label_tab = switched;
    app_window_update_tiles(win);
}
