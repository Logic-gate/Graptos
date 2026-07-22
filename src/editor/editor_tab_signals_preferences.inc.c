/**
 * @file src/editor/editor_tab_signals_preferences.inc.c
 * @brief Graptoς editor tab signals preferences module.
 * @details Editor tabs hold the real editing surface. We split the implementation by
 *          lifecycle, input, rendering, preview, and transient UI because each part has
 *          different timing and cleanup pressure.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param syntax The syntax definition used by the editor path.
 * @param manual The manual supplied by the caller.
 */

void editor_tab_set_syntax(EditorTab *tab, SyntaxDef *syntax, gboolean manual) {
    if (!tab) return;
    tab->active_syntax = syntax;
    tab->manual_syntax_override = manual;
    editor_tab_update_highlight_engine(tab);

    if (editor_tab_live_features_allowed(tab)) {
        tab->low_latency_mode_active = FALSE;
        editor_tab_schedule_minimap_update(tab);
        editor_tab_schedule_preview_update(tab);
        editor_tab_schedule_syntax_diagnostics(tab);
    } else {
        editor_tab_cancel_live_work(tab);
        tab->diagnostic_warnings = 0u;
        tab->low_latency_mode_active = TRUE;
        editor_tab_schedule_minimap_update(tab);
    }
    editor_tab_update_status(tab);
}


/**
 * @brief Editor tab auto select syntax.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_auto_select_syntax(EditorTab *tab) {
    if (!tab || !tab->win || tab->manual_syntax_override) return;
    SyntaxDef *syntax = syntax_for_path(tab->win->syntaxes, tab->file_path);
    if (!syntax) syntax = syntax_for_content(tab->win->syntaxes, tab->buffer);
    editor_tab_set_syntax(tab, syntax, FALSE);
}



/**
 * @brief On mark set.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param location The location supplied by the caller.
 * @param mark The mark supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_mark_set(GtkTextBuffer *buffer, GtkTextIter *location, GtkTextMark *mark, gpointer user_data) {
    (void)location;
    EditorTab *tab = user_data;
    if (!tab || tab->disposing || !tab->buffer || buffer != tab->buffer) return;

    const char *name = mark ? gtk_text_mark_get_name(mark) : NULL;
    gboolean relevant_mark = name &&
        (strcmp(name, "insert") == 0 || strcmp(name, "selection_bound") == 0);
    gboolean has_selection = FALSE;
    if (relevant_mark || tab->regex_tester_active) {
        has_selection = gtk_text_buffer_get_has_selection(buffer);
    }

    if (tab && tab->win && tab->win->debug_mode) {
        g_message("Regex tester: mark-set name=%s has-selection=%d",
                  name ? name : "(anonymous)",
                  has_selection);
    }
    if (relevant_mark || has_selection) {
        if (name && strcmp(name, "insert") == 0) {
            editor_tab_reposition_visible_cursor_popovers(tab);
            editor_tab_schedule_lightweight_ui_refresh(tab);
            if (tab->gutter) gtk_widget_queue_draw(tab->gutter);
        }
        if (!editor_tab_live_features_allowed(tab)) {
            if (!tab->low_latency_mode_active) {
                editor_tab_cancel_live_work(tab);
                tab->low_latency_mode_active = TRUE;
            }
            /* GtkSourceView owns highlighting; no per-cursor YAML regex work. */
        }
/**
 * @brief On buffer changed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param user_data The callback context passed through GTK signal data.
 */
        if (has_selection) {
            editor_tab_schedule_regex_tester(tab);
        } else {
            graptos_source_cancel(&tab->regex_tester_timeout);
            if (tab->regex_tester_active) hide_hover_preview(tab);
        }
    }
}


/**
 * @brief React to editor buffer changes.
 * @details This callback is on the text-input critical path. It must not run
 *          project searches, regex syntax passes, preview rendering, or other
 *          whole-buffer work before GTK has a chance to paint the inserted
 *          character.
 * @param buffer The buffer that emitted the changed signal.
 * @param user_data The editor tab passed through signal data.
 *
 * This callback is on the text-input critical path.  It must not run project
 * searches, regex syntax passes, preview rendering, or other whole-buffer work
 * before GTK has a chance to paint the inserted character.
 */
void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
    (void)buffer;
    EditorTab *tab = user_data;
    if (!tab || tab->applying_change) return;

    gint char_count = gtk_text_buffer_get_char_count(tab->buffer);
    gboolean large_file = (guint)char_count > GRAPTOS_LIVE_FEATURE_MAX_CHARS;

    /*
     * The snapshot undo implementation copies the buffer, so keep it only for
     * small files.  Large files rely on reduced live work rather than spending
     * time duplicating megabytes of text after each edit.
     */
    if (!large_file && (guint)char_count <= GRAPTOS_MAX_UNDO_CAPTURE_BYTES) {
        char *current = buffer_text(tab);
        if (!current) return;
        if (tab->last_snapshot && strcmp(tab->last_snapshot, current) != 0) {
            push_limited(tab->undo_stack, tab->last_snapshot);
            tab->last_snapshot = g_strdup(current);
            clear_stack(tab->redo_stack);
        } else if (!tab->last_snapshot) {
            tab->last_snapshot = g_strdup(current);
        }
        g_free(current);
    } else {
        g_clear_pointer(&tab->last_snapshot, g_free);
        if (tab->undo_stack) g_ptr_array_set_size(tab->undo_stack, 0);
        if (tab->redo_stack) g_ptr_array_set_size(tab->redo_stack, 0);
    }

    tab->modified = TRUE;
    if (tab->text_view) {
        gtk_widget_queue_resize(tab->text_view);
        gtk_widget_queue_draw(tab->text_view);
    }
    if (tab->scrolled) {
        gtk_widget_queue_resize(tab->scrolled);
        gtk_widget_queue_draw(tab->scrolled);
    }

    if (large_file) {
        if (!tab->low_latency_mode_active) {
            editor_tab_cancel_live_work(tab);
            tab->diagnostic_warnings = 0u;
            tab->low_latency_mode_active = TRUE;
        }
        editor_tab_schedule_minimap_update(tab);
        editor_tab_schedule_lightweight_ui_refresh(tab);
        if (!tab->manual_syntax_override && !tab->file_path) editor_tab_auto_select_syntax(tab);
        return;
    }

    tab->low_latency_mode_active = FALSE;

    /* Clear transient editor tags only when they are known to be active.
     * Removing tags across a full GtkTextBuffer on every keystroke is a
     * visible latency source on large source files.
     */
    if (tab->selection_matches_active) clear_selection_matches(tab);
    if (tab->color_literals_active) clear_color_literals(tab);
    /*
     * Diagnostics may come from LSP. Do not clear them on every keypress;
     * syntax diagnostics and LSP publishDiagnostics replace them on their own
     * schedules. Clearing here made LSP warnings disappear before replacement.
     */

/**
 * @brief Change timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
    editor_tab_schedule_minimap_update(tab);
    editor_tab_schedule_preview_update(tab);
    editor_tab_schedule_selection_matches(tab);
    editor_tab_schedule_color_literals(tab);
    editor_tab_schedule_syntax_diagnostics(tab);
    if (!tab->manual_syntax_override && !tab->file_path) editor_tab_auto_select_syntax(tab);
    editor_tab_schedule_completion(tab);
    editor_tab_schedule_lsp_change(tab);
    editor_tab_schedule_lightweight_ui_refresh(tab);
}

/**
 * @brief Tab schedule lsp change.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
gboolean lsp_change_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->lsp_change_timeout = 0u;
    if (tab->win && tab->win->lsp_client && tab->file_path &&
        editor_tab_live_features_allowed(tab)) {
        lsp_client_document_changed(tab->win->lsp_client, tab);
    }
    return G_SOURCE_REMOVE;
}

/**
 * @brief Schedule a delayed LSP didChange notification.
 * @details LSP servers should see the final buffer after a short typing burst,
 *          not every intermediate keystroke. The delay keeps typing responsive
 *          and still lets diagnostics/completions catch up quickly.
 * @param tab The editor tab whose document changed.
 */
void editor_tab_schedule_lsp_change(EditorTab *tab) {
    if (!tab || !tab->win || !tab->win->lsp_client || !tab->file_path) return;
    graptos_source_cancel(&tab->lsp_change_timeout);
    if (!editor_tab_live_features_allowed(tab)) return;
    tab->lsp_change_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                 tab->win->lsp_change_delay_ms > 0u
                                                     ? tab->win->lsp_change_delay_ms
                                                     : 450u,
                                                 lsp_change_timeout_cb,
                                                 tab,
                                                 NULL);
}


/**
 * @brief Editor tab set tab policy.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param width The width supplied by the caller.
 * @param insert_spaces The insert spaces supplied by the caller.
 */
void editor_tab_set_tab_policy(EditorTab *tab, guint width, gboolean insert_spaces) {
    if (!tab) return;
    if (width == 0u) width = 4u;
    if (width > 16u) width = 16u;
    tab->tab_width = width;
    tab->insert_spaces = insert_spaces;

    PangoTabArray *tabs = pango_tab_array_new(1, TRUE);
    if (tabs) {
        pango_tab_array_set_tab(tabs, 0, PANGO_TAB_LEFT, (gint)(width * 8u));
        gtk_text_view_set_tabs(GTK_TEXT_VIEW(tab->text_view), tabs);
        pango_tab_array_free(tabs);
    }
    if (tab->text_view) {
        gtk_source_view_set_tab_width(GTK_SOURCE_VIEW(tab->text_view), width);
        gtk_source_view_set_insert_spaces_instead_of_tabs(
            GTK_SOURCE_VIEW(tab->text_view), insert_spaces);
    }
}


/**
 * @brief Editor tab apply preferences.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_apply_preferences(EditorTab *tab) {
    if (!tab || !tab->win || !tab->text_view) return;
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tab->text_view), !tab->win->use_system_interface_font);
    editor_tab_set_minimap_visible(tab, tab->win->minimap_enabled);
    editor_tab_set_preview_visible(tab, tab->win->preview_enabled);
    gtk_widget_queue_draw(tab->text_view);
}


/**
 * @brief Editor tab set minimap visible.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param visible The visible supplied by the caller.
 */
void editor_tab_set_minimap_visible(EditorTab *tab, gboolean visible) {
    if (!tab || !tab->minimap_scrolled) return;
    if (visible) {
        gtk_widget_set_visible(tab->minimap_scrolled, TRUE);
        update_minimap_text(tab);
        if (editor_tab_live_features_allowed(tab)) update_selection_matches(tab);
    } else {
        gtk_widget_set_visible(tab->minimap_scrolled, FALSE);
    }
}



/**
 * @brief Editor tab set preview visible.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param visible The visible supplied by the caller.
 */
void editor_tab_set_preview_visible(EditorTab *tab, gboolean visible) {
    if (!tab || !tab->preview_box) return;
    if (visible && preview_is_supported(tab)) {
        if (tab->preview_detached && tab->preview_window) {
            gtk_widget_set_visible(tab->preview_window, TRUE);
        } else {
            gtk_widget_set_visible(tab->preview_box, TRUE);
        }
        if (editor_tab_live_features_allowed(tab)) editor_tab_update_preview(tab);
    } else {
        if (tab->preview_window) gtk_widget_set_visible(tab->preview_window, FALSE);
        gtk_widget_set_visible(tab->preview_box, FALSE);
    }
}

/**
 * @brief Editor tab set backup enabled.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param enabled The enabled supplied by the caller.
 */
void editor_tab_set_backup_enabled(EditorTab *tab, gboolean enabled) {
    if (!tab) return;
    tab->backup_enabled = enabled;
}
