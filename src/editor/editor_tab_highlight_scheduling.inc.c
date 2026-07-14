/**
 * @file src/editor/editor_tab_highlight_scheduling.inc.c
 * @brief Cleaf editor tab highlight scheduling module.
 */

void editor_tab_apply_highlight(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    /* Cleaf no longer applies regex YAML highlighting to the editor buffer.
     * This change came when moving to GtkSourceView.
     * Highlighting is owned by GtkSourceView; YAML rules only generate optional
     * GtkSourceView style-scheme overrides when the user enables them.
     */
    editor_tab_update_highlight_engine(tab);
}


/**
 * @brief Minimap timeout cb.
 */
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


/**
 * @brief Preview timeout cb.
 */
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


/**
 * @brief Highlight timeout cb.
 */
gboolean highlight_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->highlight_timeout = 0u;
    editor_tab_apply_highlight(tab);
    return G_SOURCE_REMOVE;
}


/**
 * @brief Editor tab schedule highlight.
 */
void editor_tab_schedule_highlight(EditorTab *tab) {
    if (!tab) return;
    /* GtkSourceView performs syntax highlighting internally. There is no
     * Cleaf regex highlighter to schedule; keep this as a redraw compatibility
     * hook for undo/load paths that still call it.
     */
    if (tab->text_view) gtk_widget_queue_draw(tab->text_view);
    if (tab->minimap_view) gtk_widget_queue_draw(tab->minimap_view);
}

