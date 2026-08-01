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
    if (tab->active_syntax == syntax && tab->manual_syntax_override == manual) {
        return;
    }

    /**
     * @brief Apply syntax only when it actually changes.
     * @details GtkSourceView language and style refresh is expensive because it
     *          clears transient tags and rebuilds source highlighting state. Auto
     *          detection can run while typing, so unchanged syntax must not reset
     *          the highlighter on every buffer edit.
     */
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

    if (tab && tab->win && tab->win->debug_mode &&
        (relevant_mark || tab->regex_tester_active || has_selection)) {
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
 * @brief Milliseconds between monotonic timestamps.
 * @details Typing debug logs use small timing checkpoints around the editor's
 *          hot path. Keeping conversion in one helper keeps the logs readable
 *          without changing the code paths being measured.
 * @param start_us Start timestamp from g_get_monotonic_time().
 * @param end_us End timestamp from g_get_monotonic_time().
 * @return Elapsed milliseconds.
 */
static double typing_elapsed_ms(gint64 start_us, gint64 end_us) {
    if (end_us < start_us) return 0.0;
    return (double)(end_us - start_us) / 1000.0;
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

    gboolean debug_typing = tab->win && tab->win->debug_mode;
    gint64 typing_start_us = debug_typing ? g_get_monotonic_time() : 0;
    double notes_ms = 0.0;
    double undo_ms = 0.0;
    double draw_ms = 0.0;
    double schedule_ms = 0.0;
    double minimap_schedule_ms = 0.0;
    double preview_schedule_ms = 0.0;
    double selection_schedule_ms = 0.0;
    double color_schedule_ms = 0.0;
    double diagnostics_schedule_ms = 0.0;
    double syntax_schedule_ms = 0.0;
    double completion_schedule_ms = 0.0;
    double lsp_schedule_ms = 0.0;
    double ui_schedule_ms = 0.0;
    gboolean notes_saved = FALSE;
    gboolean undo_copied = FALSE;
    gboolean undo_changed = FALSE;
    gboolean undo_cleared = FALSE;
    gint line_delta = 0;

    gint char_count = gtk_text_buffer_get_char_count(tab->buffer);
    guint line_count = (guint)gtk_text_buffer_get_line_count(tab->buffer);
    if (tab->last_line_count != 0u && line_count != tab->last_line_count &&
        tab->notes && tab->notes->len > 0u) {
        gint64 notes_start_us = debug_typing ? g_get_monotonic_time() : 0;
        GtkTextIter cursor;
        GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, mark);
        gint edit_line = gtk_text_iter_get_line(&cursor);
        gint delta = (gint)line_count - (gint)tab->last_line_count;
        line_delta = delta;
        for (guint i = 0u; tab->notes && i < tab->notes->len; i++) {
            EditorNote *note = g_ptr_array_index(tab->notes, i);
            if (!note) continue;
            if (note->start_line > edit_line) {
                note->start_line = MAX(0, note->start_line + delta);
                note->end_line = MAX(note->start_line, note->end_line + delta);
            } else if (note->end_line >= edit_line) {
                note->end_line = MAX(note->start_line, note->end_line + delta);
            }
        }
        editor_tab_save_notes(tab);
        notes_saved = TRUE;
        if (tab->gutter) gtk_widget_queue_draw(tab->gutter);
        if (debug_typing) {
            notes_ms = typing_elapsed_ms(notes_start_us, g_get_monotonic_time());
        }
    } else if (tab->last_line_count != 0u) {
        line_delta = (gint)line_count - (gint)tab->last_line_count;
    }
    tab->last_line_count = line_count;
    gboolean large_file = !editor_tab_live_features_allowed(tab);

    /**
     * @brief Keep undo snapshots out of the large-buffer path.
     * @details The snapshot undo implementation copies the buffer, so keep it only for
     *          small files. Large files rely on reduced live work rather than spending
     *          time duplicating megabytes of text after each edit.
     */
    guint undo_capture_limit = tab->win && tab->win->max_undo_capture_bytes > 0u
        ? tab->win->max_undo_capture_bytes
        : GRAPTOS_MAX_UNDO_CAPTURE_BYTES;
    gint64 undo_start_us = debug_typing ? g_get_monotonic_time() : 0;
    if (!large_file && (guint)char_count <= undo_capture_limit) {
        char *current = buffer_text(tab);
        if (!current) return;
        undo_copied = TRUE;
        if (tab->last_snapshot && strcmp(tab->last_snapshot, current) != 0) {
            push_limited(tab, tab->undo_stack, tab->last_snapshot);
            tab->last_snapshot = g_strdup(current);
            clear_stack(tab->redo_stack);
            undo_changed = TRUE;
        } else if (!tab->last_snapshot) {
            tab->last_snapshot = g_strdup(current);
        }
        g_free(current);
    } else {
        g_clear_pointer(&tab->last_snapshot, g_free);
        if (tab->undo_stack) g_ptr_array_set_size(tab->undo_stack, 0);
        if (tab->redo_stack) g_ptr_array_set_size(tab->redo_stack, 0);
        undo_cleared = TRUE;
    }
    if (debug_typing) {
        undo_ms = typing_elapsed_ms(undo_start_us, g_get_monotonic_time());
    }

    tab->modified = TRUE;
    /**
     * @brief Keep text input out of GTK size negotiation.
     * @details Text insertion already invalidates the affected view. Queue a draw
     *          for dependent overlays, but do not force a size negotiation on every
     *          keypress.
     */
    gint64 draw_start_us = debug_typing ? g_get_monotonic_time() : 0;
    if (tab->text_view) {
        gtk_widget_queue_draw(tab->text_view);
    }
    if (debug_typing) {
        draw_ms = typing_elapsed_ms(draw_start_us, g_get_monotonic_time());
    }

    if (large_file) {
        gint64 schedule_start_us = debug_typing ? g_get_monotonic_time() : 0;
        if (!tab->low_latency_mode_active) {
            editor_tab_cancel_live_work(tab);
            tab->diagnostic_warnings = 0u;
            tab->low_latency_mode_active = TRUE;
        }
        gint64 part_start_us = debug_typing ? g_get_monotonic_time() : 0;
        editor_tab_schedule_lsp_change(tab);
        if (debug_typing) lsp_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
        part_start_us = debug_typing ? g_get_monotonic_time() : 0;
        editor_tab_schedule_lightweight_ui_refresh(tab);
        if (debug_typing) ui_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
        part_start_us = debug_typing ? g_get_monotonic_time() : 0;
        if (!tab->manual_syntax_override && !tab->file_path) editor_tab_auto_select_syntax(tab);
        if (debug_typing) syntax_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
        if (debug_typing) {
            schedule_ms = typing_elapsed_ms(schedule_start_us, g_get_monotonic_time());
            g_message("Typing: changed key=%s chars=%d lines=%u delta=%d large=1 notes_saved=%d undo_copy=%d undo_changed=%d undo_cleared=%d undo_len=%u redo_len=%u notes_ms=%.3f undo_ms=%.3f draw_ms=%.3f schedule_ms=%.3f syntax_ms=%.3f lsp_ms=%.3f ui_ms=%.3f total_ms=%.3f lsp_timeout=%u ui_timeout=%u",
                      tab->last_typing_debug_key ? tab->last_typing_debug_key : "(unknown)",
                      char_count,
                      line_count,
                      line_delta,
                      notes_saved,
                      undo_copied,
                      undo_changed,
                      undo_cleared,
                      tab->undo_stack ? tab->undo_stack->len : 0u,
                      tab->redo_stack ? tab->redo_stack->len : 0u,
                      notes_ms,
                      undo_ms,
                      draw_ms,
                      schedule_ms,
                      syntax_schedule_ms,
                      lsp_schedule_ms,
                      ui_schedule_ms,
                      typing_elapsed_ms(typing_start_us, g_get_monotonic_time()),
                      tab->lsp_change_timeout,
                      tab->ui_refresh_timeout);
        }
        return;
    }

    tab->low_latency_mode_active = FALSE;

    /**
     * @brief Clear transient tags only when they are active.
     * @details Removing tags across a full GtkTextBuffer on every keystroke is a
     *          visible latency source on large source files.
     */
    if (tab->selection_matches_active) clear_selection_matches(tab);
    if (tab->color_literals_active) clear_color_literals(tab);
    /**
     * @brief Let diagnostic producers replace their own tags.
     * @details Diagnostics may come from LSP. Do not clear them on every keypress;
     *          syntax diagnostics and LSP publishDiagnostics replace them on their
     *          own schedules. Clearing here made LSP warnings disappear before
     *          replacement.
     */

    /**
     * @brief Schedule full live editor work for small buffers.
     * @details These paths may scan or retag the buffer, so they stay behind the
     *          live-feature guard and never run in reduced-work mode.
     */
    gint64 schedule_start_us = debug_typing ? g_get_monotonic_time() : 0;
    gint64 part_start_us = debug_typing ? g_get_monotonic_time() : 0;
    editor_tab_schedule_minimap_update(tab);
    if (debug_typing) minimap_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
    part_start_us = debug_typing ? g_get_monotonic_time() : 0;
    editor_tab_schedule_preview_update(tab);
    if (debug_typing) preview_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
    part_start_us = debug_typing ? g_get_monotonic_time() : 0;
    editor_tab_schedule_selection_matches(tab);
    if (debug_typing) selection_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
    part_start_us = debug_typing ? g_get_monotonic_time() : 0;
    editor_tab_schedule_color_literals(tab);
    if (debug_typing) color_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
    part_start_us = debug_typing ? g_get_monotonic_time() : 0;
    editor_tab_schedule_syntax_diagnostics(tab);
    if (debug_typing) diagnostics_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
    part_start_us = debug_typing ? g_get_monotonic_time() : 0;
    if (!tab->manual_syntax_override && !tab->file_path) editor_tab_auto_select_syntax(tab);
    if (debug_typing) syntax_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
    part_start_us = debug_typing ? g_get_monotonic_time() : 0;
    editor_tab_schedule_completion(tab);
    if (debug_typing) completion_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
    part_start_us = debug_typing ? g_get_monotonic_time() : 0;
    editor_tab_schedule_lsp_change(tab);
    if (debug_typing) lsp_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
    part_start_us = debug_typing ? g_get_monotonic_time() : 0;
    editor_tab_schedule_lightweight_ui_refresh(tab);
    if (debug_typing) ui_schedule_ms = typing_elapsed_ms(part_start_us, g_get_monotonic_time());
    if (debug_typing) {
        schedule_ms = typing_elapsed_ms(schedule_start_us, g_get_monotonic_time());
        g_message("Typing: changed key=%s chars=%d lines=%u delta=%d large=0 notes_saved=%d undo_copy=%d undo_changed=%d undo_cleared=%d undo_len=%u redo_len=%u notes_ms=%.3f undo_ms=%.3f draw_ms=%.3f schedule_ms=%.3f minimap_ms=%.3f preview_ms=%.3f selection_ms=%.3f color_ms=%.3f diagnostics_ms=%.3f syntax_ms=%.3f completion_ms=%.3f lsp_ms=%.3f ui_ms=%.3f total_ms=%.3f minimap_timeout=%u preview_timeout=%u selection_timeout=%u color_timeout=%u diagnostics_timeout=%u completion_timeout=%u lsp_timeout=%u ui_timeout=%u",
                  tab->last_typing_debug_key ? tab->last_typing_debug_key : "(unknown)",
                  char_count,
                  line_count,
                  line_delta,
                  notes_saved,
                  undo_copied,
                  undo_changed,
                  undo_cleared,
                  tab->undo_stack ? tab->undo_stack->len : 0u,
                  tab->redo_stack ? tab->redo_stack->len : 0u,
                  notes_ms,
                  undo_ms,
                  draw_ms,
                  schedule_ms,
                  minimap_schedule_ms,
                  preview_schedule_ms,
                  selection_schedule_ms,
                  color_schedule_ms,
                  diagnostics_schedule_ms,
                  syntax_schedule_ms,
                  completion_schedule_ms,
                  lsp_schedule_ms,
                  ui_schedule_ms,
                  typing_elapsed_ms(typing_start_us, g_get_monotonic_time()),
                  tab->minimap_timeout,
                  tab->preview_timeout,
                  tab->selection_match_timeout,
                  tab->color_literal_timeout,
                  tab->diagnostics_timeout,
                  tab->completion_timeout,
                  tab->lsp_change_timeout,
                  tab->ui_refresh_timeout);
    }
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
        editor_tab_lsp_sync_allowed(tab)) {
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
    if (!editor_tab_lsp_sync_allowed(tab)) return;
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
