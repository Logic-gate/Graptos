static void editor_tab_visible_or_cursor_range(EditorTab *tab, guint *start_line_out, guint *end_line_out) {
    if (start_line_out) *start_line_out = 0u;
    if (end_line_out) *end_line_out = 0u;
    if (!tab || !tab->buffer) return;

    gint line_count = gtk_text_buffer_get_line_count(tab->buffer);
    if (line_count <= 0) return;

    GtkTextIter cursor;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, mark);
    gint cursor_line = gtk_text_iter_get_line(&cursor);

    gint start_line = cursor_line - (gint)CLEAF_HIGHLIGHT_CONTEXT_LINES;
    gint end_line = cursor_line + (gint)CLEAF_HIGHLIGHT_CONTEXT_LINES;

    if (tab->text_view && gtk_widget_get_mapped(tab->text_view)) {
        GtkTextView *view = GTK_TEXT_VIEW(tab->text_view);
        GdkRectangle visible;
        gtk_text_view_get_visible_rect(view, &visible);

        GtkTextIter top;
        GtkTextIter bottom;
        gint ignored = 0;
        gtk_text_view_get_line_at_y(view, &top, visible.y, &ignored);
        gtk_text_view_get_line_at_y(view, &bottom, visible.y + visible.height, &ignored);

        gint visible_start = gtk_text_iter_get_line(&top) - (gint)CLEAF_HIGHLIGHT_CONTEXT_LINES;
        gint visible_end = gtk_text_iter_get_line(&bottom) + (gint)CLEAF_HIGHLIGHT_CONTEXT_LINES;
        if (visible_start < start_line) start_line = visible_start;
        if (visible_end > end_line) end_line = visible_end;
    }

    if (start_line < 0) start_line = 0;
    if (end_line >= line_count) end_line = line_count - 1;
    if (end_line < start_line) end_line = start_line;

    if (start_line_out) *start_line_out = (guint)start_line;
    if (end_line_out) *end_line_out = (guint)end_line;
}


void editor_tab_apply_highlight(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    /* Cleaf no longer applies regex YAML highlighting to the editor buffer.
     * Highlighting is owned by GtkSourceView; YAML rules only generate optional
     * GtkSourceView style-scheme overrides when the user enables them.
     */
    editor_tab_update_highlight_engine(tab);
}


gboolean minimap_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->minimap_timeout = 0u;
    update_minimap_text(tab);
    return G_SOURCE_REMOVE;
}


/*
 * Minimap refresh is delayed because it follows the main source buffer and can
 * cause extra layout work.  Keeping it low-priority preserves typing latency
 * while still making the preview catch up after the user pauses.
 */
void editor_tab_schedule_minimap_update(EditorTab *tab) {
    if (!tab || !tab->win || !tab->win->minimap_enabled) return;
    cleaf_source_cancel(&tab->minimap_timeout);
    tab->minimap_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                              80u,
                                              minimap_timeout_cb,
                                              tab,
                                              NULL);
}


gboolean preview_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->preview_timeout = 0u;
    editor_tab_update_preview(tab);
    return G_SOURCE_REMOVE;
}


/*
 * Markdown/LaTeX previews rebuild derived buffers.  Schedule them after idle
 * time instead of synchronously from buffer changes so prose/code typing stays
 * responsive.
 */
void editor_tab_schedule_preview_update(EditorTab *tab) {
    if (!tab || !tab->win || !tab->win->preview_enabled) return;
    cleaf_source_cancel(&tab->preview_timeout);
    if (!editor_tab_live_features_allowed(tab)) {
        tab->preview_timeout = 0u;
        return;
    }
    tab->preview_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                             CLEAF_PREVIEW_DELAY_MS,
                                             preview_timeout_cb,
                                             tab,
                                             NULL);
}


gboolean highlight_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->highlight_timeout = 0u;
    editor_tab_apply_highlight(tab);
    return G_SOURCE_REMOVE;
}


void editor_tab_schedule_highlight(EditorTab *tab) {
    if (!tab) return;
    /* GtkSourceView performs syntax highlighting internally. There is no
     * Cleaf regex highlighter to schedule; keep this as a redraw compatibility
     * hook for undo/load paths that still call it.
     */
    if (tab->text_view) gtk_widget_queue_draw(tab->text_view);
    if (tab->minimap_view) gtk_widget_queue_draw(tab->minimap_view);
}


