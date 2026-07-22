/**
 * @file src/editor/editor_tab_lifecycle.inc.c
 * @brief Graptoς editor tab lifecycle module.
 * @details Editor tabs hold the real editing surface. We split the implementation by
 *          lifecycle, input, rendering, preview, and transient UI because each part has
 *          different timing and cleanup pressure.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */

char *editor_tab_basename(EditorTab *tab) {
    if (!tab) return g_strdup("Untitled");
    if (!tab->file_path) {
        return g_strdup(tab->display_title && tab->display_title[0]
                            ? tab->display_title
                            : "Untitled");
    }

    /*
     * Use GTK filename display conversion so non-ASCII paths are shown in a
     * user-readable form.
     */
    char *display = g_filename_display_basename(tab->file_path);

    // Fall back to Untitled if the display name is not valid UTF-8.
    if (!display || !g_utf8_validate(display, -1, NULL)) {
        g_free(display);
        return g_strdup("Untitled");
    }

    return display;
}
/**
 * @brief Buffer text.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *buffer_text(EditorTab *tab) {
    GtkTextIter start;
    GtkTextIter end;

    // Return an owned copy of the full buffer text.
    gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
    return gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
}
/**
 * @brief Insert text tagless.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param where The where supplied by the caller.
 * @param text The text fragment supplied by the caller.
 */
void insert_text_tagless(GtkTextBuffer *buffer,
                         GtkTextIter *where,
                         const char *text) {
    if (!buffer || !where || !text) return;

    // Insert plain text without applying syntax or search tags manually.
    gtk_text_buffer_insert(buffer, where, text, -1);
}
/**
 * @brief Cancel timeout id.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param id The id supplied by the caller.
 */
static void cancel_timeout_id(guint *id) {
    // Keep timeout cancellation behind one helper in case source handling changes.
    graptos_source_cancel(id);
}
/**
 * @brief Editor tab large file mode.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_large_file_mode(EditorTab *tab) {
    if (!tab || !tab->buffer) return FALSE;

    /*
     * Large buffers skip expensive live features so typing and scrolling stay
     * responsive.
     */
    return (guint)gtk_text_buffer_get_char_count(tab->buffer)
        > GRAPTOS_LIVE_FEATURE_MAX_CHARS;
}
/**
 * @brief Editor tab live features allowed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_live_features_allowed(EditorTab *tab) {
    // Live features are disabled for large files to avoid editor lag.
    return tab && tab->buffer && !editor_tab_large_file_mode(tab);
}
/**
 * @brief Editor tab highlighting allowed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_highlighting_allowed(EditorTab *tab) {
    // Highlighting needs an active syntax and at least one loaded rule.
    return tab && tab->buffer && tab->active_syntax && tab->active_syntax->rules;
}
/**
 * @brief Editor tab reference features allowed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_reference_features_allowed(EditorTab *tab) {
    // Reference lookup only needs a valid text buffer for now.
    return tab && tab->buffer;
}
/**
 * @brief Editor tab cancel live work.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_cancel_live_work(EditorTab *tab) {
    if (!tab) return;

    /*
     * Cancel delayed work before changing tab state or destroying widgets.
     * Otherwise stale callbacks can run against old buffer/UI state.
     */
    cancel_timeout_id(&tab->completion_timeout);
    cancel_timeout_id(&tab->minimap_timeout);
    cancel_timeout_id(&tab->preview_timeout);
    cancel_timeout_id(&tab->diagnostics_timeout);
    cancel_timeout_id(&tab->selection_match_timeout);
    cancel_timeout_id(&tab->color_literal_timeout);
    cancel_timeout_id(&tab->regex_tester_timeout);
    cancel_timeout_id(&tab->lsp_change_timeout);
    cancel_timeout_id(&tab->lsp_completion_retry_timeout);
    cancel_timeout_id(&tab->preview_reparent_idle);
    cancel_timeout_id(&tab->hover_timeout);
    cancel_timeout_id(&tab->hover_hide_timeout);
    cancel_timeout_id(&tab->hover_lsp_fallback_timeout);
    cancel_timeout_id(&tab->ui_refresh_timeout);

    if (tab->completion_popover) graptos_popover_hide(tab->completion_popover);

    tab->completion_manual = FALSE;
    g_clear_pointer(&tab->completion_prefix, g_free);
}
/**
 * @brief Editor tab lightweight ui timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_lightweight_ui_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;

    // Timeout has fired, so clear the stored source id.
    tab->ui_refresh_timeout = 0u;

    /*
     * These updates are cheap but noisy during typing, so they are batched
     * through a short timeout.
     */
    editor_tab_update_title(tab);
    update_gutter_width(tab);
    editor_tab_update_status(tab);

    return G_SOURCE_REMOVE;
}


/**
 * @brief Editor tab schedule lightweight ui refresh.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_lightweight_ui_refresh(EditorTab *tab) {
    if (!tab) return;

    // Do not stack multiple refresh timers for the same tab.
    if (tab->ui_refresh_timeout != 0u) return;

    tab->ui_refresh_timeout = g_timeout_add(60u,
                                            editor_tab_lightweight_ui_timeout_cb,
                                            tab);
}


/**
 * @brief Popup append item.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param menu The menu supplied by the caller.
 * @param label The label supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param data The callback context passed by the caller.
 */
void popup_append_item(GtkWidget *menu,
                       const char *label,
                       GCallback callback,
                       gpointer data) {
    if (!menu) return;

    // Popover menus use Graptoς flat buttons instead of native menu rows.
    GtkWidget *item = graptos_flat_button_new(label, NULL, callback, data);
    gtk_box_append(GTK_BOX(menu), item);
}


/**
 * @brief On close button clicked.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_close_button_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;

    EditorTab *tab = user_data;

    // Route closing through the window so unsaved checks and tab replacement run.
    if (tab && tab->win) app_window_close_tab(tab->win, tab);
}


/**
 * @brief Editor tab text rect to popover parent.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param rect The rect supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_text_rect_to_popover_parent(EditorTab *tab,
                                                GdkRectangle *rect) {
    if (!tab || !rect || !tab->text_view || !tab->popover_parent) {
        return FALSE;
    }

    /*
     * TextView coordinates are not always the same coordinate space used by the
     * popover parent. Convert before passing the rectangle to GtkPopover.
     */
    graphene_point_t source = GRAPHENE_POINT_INIT((float)rect->x,
                                                  (float)rect->y);
    graphene_point_t target = GRAPHENE_POINT_INIT(0.0f, 0.0f);

    if (!gtk_widget_compute_point(tab->text_view,
                                  tab->popover_parent,
                                  &source,
                                  &target)) {
        return FALSE;
    }

    rect->x = (gint)target.x;
    rect->y = (gint)target.y;

    return TRUE;
}


/**
 * @brief Editor tab cursor rect to popover parent.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param rect The rect supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_cursor_rect_to_popover_parent(EditorTab *tab,
                                                  GdkRectangle *rect) {
    if (!tab || !rect || !tab->buffer || !tab->text_view) return FALSE;

    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    if (!mark) return FALSE;

    GtkTextIter cursor;
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, mark);
    gtk_text_view_get_iter_location(GTK_TEXT_VIEW(tab->text_view), &cursor,
                                    rect);
    rect->y += rect->height;
    rect->width = 1;
    if (rect->height < 1) rect->height = 1;

    /*
     * gtk_text_view_get_iter_location() returns buffer coordinates.  Popover
     * placement needs widget coordinates before converting to the shared
     * popover parent.  Without this conversion, cursor popovers drift or clamp
     * incorrectly after scrolling through large files.
     */
    int window_x = 0;
    int window_y = 0;
    gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(tab->text_view),
                                          GTK_TEXT_WINDOW_WIDGET,
                                          rect->x,
                                          rect->y,
                                          &window_x,
                                          &window_y);
    rect->x = window_x;
    rect->y = window_y;

    return editor_tab_text_rect_to_popover_parent(tab, rect);
}


/**
 * @brief Popover estimated width.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param popover The popover supplied by the caller.
 * @return The computed value requested by the caller.
 */
static gint popover_estimated_width(GtkWidget *popover) {
    if (!popover) return 1;

    gint width = gtk_widget_get_width(popover);
    if (width > 1) return width;

    GtkWidget *child = GTK_IS_POPOVER(popover) ?
        gtk_popover_get_child(GTK_POPOVER(popover)) : NULL;
    if (child) {
        width = gtk_widget_get_width(child);
        if (width > 1) return width;

        gint requested_width = -1;
        gint requested_height = -1;
        gtk_widget_get_size_request(child, &requested_width,
                                    &requested_height);
        if (requested_width > 1) return requested_width;
    }

    return 460;
}


/**
 * @brief Editor tab place popover at cursor.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param popover The popover supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_place_popover_at_cursor(EditorTab *tab,
                                            GtkWidget *popover) {
    if (!tab || !popover || !GTK_IS_POPOVER(popover)) return FALSE;

    GdkRectangle rect;
    if (!editor_tab_cursor_rect_to_popover_parent(tab, &rect)) return FALSE;

    /*
     * GtkPopover normally centers itself on the pointing rectangle.  Offset by
     * half the current/expected width so the visible top-left corner begins at
     * the insertion cursor instead.
     */
    gint width = popover_estimated_width(popover);
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
    gtk_popover_set_offset(GTK_POPOVER(popover), width / 2, 0);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    return TRUE;
}


/**
 * @brief Editor tab reposition visible cursor popovers.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_reposition_visible_cursor_popovers(EditorTab *tab) {
    if (!tab) return;

    if (tab->completion_popover &&
        gtk_widget_get_visible(tab->completion_popover)) {
        (void)editor_tab_place_popover_at_cursor(tab,
                                                 tab->completion_popover);
    }

    if (tab->hover_popover && gtk_widget_get_visible(tab->hover_popover)) {
        (void)editor_tab_place_popover_at_cursor(tab, tab->hover_popover);
    }
}


/**
 * @brief Editor tab destroy popovers.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_destroy_popovers(EditorTab *tab) {
    if (!tab) return;

    /*
     * Popovers can outlive focus changes, so destroy them explicitly before the
     * tab or its text view goes away.
     */
    if (tab->completion_popover) {
        graptos_widget_destroy(tab->completion_popover);
        tab->completion_popover = NULL;
    }

    if (tab->hover_popover) {
        graptos_widget_destroy(tab->hover_popover);
        tab->hover_popover = NULL;
    }
}


/**
 * @brief Editor tab free.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_free(EditorTab *tab) {
    if (!tab) return;
    tab->disposing = TRUE;

    /*
     * Cancel every pending source before freeing tab memory. Any one of these
     * callbacks may still hold the tab as user_data.
     */
    graptos_source_cancel(&tab->highlight_timeout);
    graptos_source_cancel(&tab->completion_timeout);
    graptos_source_cancel(&tab->minimap_timeout);
    graptos_source_cancel(&tab->preview_timeout);
    graptos_source_cancel(&tab->diagnostics_timeout);
    graptos_source_cancel(&tab->selection_match_timeout);
    graptos_source_cancel(&tab->color_literal_timeout);
    graptos_source_cancel(&tab->regex_tester_timeout);
    graptos_source_cancel(&tab->lsp_change_timeout);
    graptos_source_cancel(&tab->lsp_completion_retry_timeout);
    graptos_source_cancel(&tab->preview_reparent_idle);
    graptos_source_cancel(&tab->hover_timeout);
    graptos_source_cancel(&tab->hover_hide_timeout);
    graptos_source_cancel(&tab->hover_lsp_fallback_timeout);
    graptos_source_cancel(&tab->ui_refresh_timeout);

    if (tab->buffer) {
        g_signal_handlers_disconnect_by_data(tab->buffer, tab);
    }
    editor_tab_preview_close_detached(tab);
    editor_tab_destroy_popovers(tab);
    if (tab->win && tab->win->lsp_client) lsp_client_document_closed(tab->win->lsp_client, tab);
    clear_color_literals(tab);

    g_free(tab->file_path);
    g_free(tab->display_title);
    g_free(tab->display_title_markup);
    g_free(tab->last_snapshot);
    g_free(tab->kill_buffer);
    g_free(tab->search_text);
    g_free(tab->completion_prefix);
    g_free(tab->lsp_completion_retry_key);
    g_free(tab->hover_word);

    if (tab->undo_stack) g_ptr_array_free(tab->undo_stack, TRUE);
    if (tab->redo_stack) g_ptr_array_free(tab->redo_stack, TRUE);
    if (tab->tile_group) g_ptr_array_free(tab->tile_group, TRUE);
    if (tab->color_literal_tag_names) g_ptr_array_free(tab->color_literal_tag_names, TRUE);
    if (tab->diagnostics) g_ptr_array_free(tab->diagnostics, TRUE);

    if (tab->notebook_refs_held) {
        if (tab->tab_widget && GTK_IS_WIDGET(tab->tab_widget)) {
            g_object_unref(tab->tab_widget);
        }
        if (tab->box && GTK_IS_WIDGET(tab->box)) {
            g_object_unref(tab->box);
        }
    }

    g_free(tab);
}


/**
 * @brief Editor tab is locked.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_is_locked(EditorTab *tab) {
    return tab ? tab->locked : FALSE;
}


/**
 * @brief Editor tab set locked.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param locked The locked supplied by the caller.
 */
void editor_tab_set_locked(EditorTab *tab, gboolean locked) {
    if (!tab) return;

    tab->locked = locked;

    // Locked files can be viewed but not edited.
    if (tab->text_view) {
        gtk_text_view_set_editable(GTK_TEXT_VIEW(tab->text_view), !locked);
    }

    editor_tab_update_title(tab);
    editor_tab_update_status(tab);
}


/**
 * @brief Editor tab update title.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_update_title(EditorTab *tab) {
    if (!tab) return;

    char *base = editor_tab_basename(tab);

    /*
     * Keep the tab label compact: modified marker first, then the display
     * basename.  The lock state is shown by a symbolic icon in the tab widget.
     */
    guint tile_group_count = tab->tile_group && tab->tile_group->len > 1u
        ? tab->tile_group->len
        : 0u;
    char *label = NULL;
    if (tile_group_count > 0u) {
        g_autofree char *escaped_base = g_markup_escape_text(base ? base : "Untitled", -1);
        label = g_strdup_printf("%s<i>%u</i> %s",
                                tab->modified ? "*" : "",
                                tile_group_count,
                                escaped_base);
        if (tab->tab_title) gtk_label_set_markup(GTK_LABEL(tab->tab_title), label);
    } else if (!tab->file_path && tab->display_title_markup &&
               tab->display_title_markup[0]) {
        label = g_strdup_printf("%s%s",
                                tab->modified ? "*" : "",
                                tab->display_title_markup);
        if (tab->tab_title) gtk_label_set_markup(GTK_LABEL(tab->tab_title), label);
    } else {
        label = g_strdup_printf("%s%s", tab->modified ? "*" : "", base);
        if (tab->tab_title) gtk_label_set_text(GTK_LABEL(tab->tab_title), label);
    }
    if (tab->tab_lock_icon) gtk_widget_set_visible(tab->tab_lock_icon, tab->locked);

    g_free(label);
    g_free(base);
}

/**
 * @brief Set a generated display title for a tab.
 * @details Generated tabs do not have paths for editor_tab_basename() to read.
 *          Storing the title here keeps later status, tile, lock, and notebook
 *          refreshes from collapsing those tabs back to Untitled.
 * @param tab The editor tab whose generated title should be stored.
 * @param plain_title Plain text title used outside the tab label.
 * @param markup_title Optional Pango markup used for the visible tab label.
 */
void editor_tab_set_display_title(EditorTab *tab,
                                  const char *plain_title,
                                  const char *markup_title) {
    if (!tab) return;

    g_free(tab->display_title);
    g_free(tab->display_title_markup);
    tab->display_title = g_strdup(plain_title && plain_title[0]
                                      ? plain_title
                                      : "Untitled");
    tab->display_title_markup = g_strdup(markup_title && markup_title[0]
                                             ? markup_title
                                             : NULL);
    editor_tab_update_title(tab);
}


/**
 * @brief Editor tab update status.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_update_status(EditorTab *tab) {
    if (!tab || !tab->win) return;

    GtkTextIter iter;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &iter, mark);

    int line = gtk_text_iter_get_line(&iter) + 1;
    int col = gtk_text_iter_get_line_offset(&iter) + 1;
    int lines = gtk_text_buffer_get_line_count(tab->buffer);
    int chars = gtk_text_buffer_get_char_count(tab->buffer);

    char *base = editor_tab_basename(tab);

    const char *syntax = tab->active_syntax && tab->active_syntax->name
        ? tab->active_syntax->name
        : "Plain Text";

    char *indent = tab->insert_spaces
        ? g_strdup_printf("Indent %u spaces",
                          tab->tab_width ? tab->tab_width : 4u)
        : g_strdup("Indent hard tabs");

    // Only include warning text when diagnostics found something.
    char *warning_part = tab->diagnostic_warnings > 0u
        ? g_strdup_printf(" | %u warning%s",
                          tab->diagnostic_warnings,
                          tab->diagnostic_warnings == 1u ? "" : "s")
        : g_strdup("");

    char *status = g_strdup_printf(
        "%s | Ln %d, Col %d | %d lines | %d chars | %s | %s%s%s%s",
        base,
        line,
        col,
        lines,
        chars,
        syntax,
        indent,
        warning_part ? warning_part : "",
        tab->modified ? " | modified" : "",
        tab->locked ? " | locked" : "");

    g_free(warning_part);
    g_free(indent);

    app_window_set_status(tab->win, status);

    g_free(status);
    g_free(base);
}
