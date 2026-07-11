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


void editor_tab_auto_select_syntax(EditorTab *tab) {
    if (!tab || !tab->win || tab->manual_syntax_override) return;
    SyntaxDef *syntax = syntax_for_path(tab->win->syntaxes, tab->file_path);
    if (!syntax) syntax = syntax_for_content(tab->win->syntaxes, tab->buffer);
    editor_tab_set_syntax(tab, syntax, FALSE);
}



void on_mark_set(GtkTextBuffer *buffer, GtkTextIter *location, GtkTextMark *mark, gpointer user_data) {
    (void)buffer;
    (void)location;
    EditorTab *tab = user_data;
    const char *name = mark ? gtk_text_mark_get_name(mark) : NULL;
    if (tab && name && (strcmp(name, "insert") == 0 || strcmp(name, "selection_bound") == 0)) {
        if (strcmp(name, "insert") == 0) {
            editor_tab_schedule_lightweight_ui_refresh(tab);
            if (tab->gutter) gtk_widget_queue_draw(tab->gutter);
        }
        if (editor_tab_live_features_allowed(tab)) {
            maybe_show_color_preview(tab);
            editor_tab_schedule_selection_matches(tab);
        } else {
            if (!tab->low_latency_mode_active) {
                editor_tab_cancel_live_work(tab);
                tab->low_latency_mode_active = TRUE;
            }
            /* GtkSourceView owns highlighting; no per-cursor YAML regex work. */
        }
    }
}


/*
 * This callback is on the text-input critical path.  It must not run project
 * searches, regex syntax passes, preview rendering, or other whole-buffer work
 * before GTK has a chance to paint the inserted character.
 */
void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data) {
    (void)buffer;
    EditorTab *tab = user_data;
    if (!tab || tab->applying_change) return;

    gint char_count = gtk_text_buffer_get_char_count(tab->buffer);
    gboolean large_file = (guint)char_count > CLEAF_LIVE_FEATURE_MAX_CHARS;

    /*
     * The snapshot undo implementation copies the buffer, so keep it only for
     * small files.  Large files rely on reduced live work rather than spending
     * time duplicating megabytes of text after each edit.
     */
    if (!large_file && (guint)char_count <= CLEAF_MAX_UNDO_CAPTURE_BYTES) {
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
    if (tab->diagnostics_active) clear_syntax_diagnostics(tab);

    editor_tab_schedule_minimap_update(tab);
    editor_tab_schedule_preview_update(tab);
    editor_tab_schedule_selection_matches(tab);
    editor_tab_schedule_syntax_diagnostics(tab);
    if (!tab->manual_syntax_override && !tab->file_path) editor_tab_auto_select_syntax(tab);
    editor_tab_schedule_completion(tab);
    editor_tab_schedule_lightweight_ui_refresh(tab);
}


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


void editor_tab_apply_preferences(EditorTab *tab) {
    if (!tab || !tab->win || !tab->text_view) return;
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tab->text_view), !tab->win->use_system_interface_font);
    editor_tab_set_minimap_visible(tab, tab->win->minimap_enabled);
    editor_tab_set_preview_visible(tab, tab->win->preview_enabled);
    gtk_widget_queue_draw(tab->text_view);
}


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



void editor_tab_set_preview_visible(EditorTab *tab, gboolean visible) {
    if (!tab || !tab->preview_scrolled) return;
    if (visible && preview_is_supported(tab)) {
        gtk_widget_set_visible(tab->preview_scrolled, TRUE);
        if (editor_tab_live_features_allowed(tab)) editor_tab_update_preview(tab);
    } else {
        gtk_widget_set_visible(tab->preview_scrolled, FALSE);
    }
}

void editor_tab_set_backup_enabled(EditorTab *tab, gboolean enabled) {
    if (!tab) return;
    tab->backup_enabled = enabled;
}


