/**
 * @file src/editor/editor_tab_highlight_scheduling.inc.c
 * @brief Graptoς editor tab highlight scheduling module.
 * @details Editor tabs hold the real editing surface. We split the implementation by
 *          lifecycle, input, rendering, preview, and transient UI because each part has
 *          different timing and cleanup pressure.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */

void editor_tab_apply_highlight(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    /* Graptoς no longer applies regex YAML highlighting to the editor buffer.
     * Highlighting is owned by GtkSourceView; YAML rules only generate optional
     * GtkSourceView style-scheme overrides when the user enables them.
     */
    editor_tab_update_highlight_engine(tab);
}


/**
 * @brief Minimap timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean minimap_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->minimap_timeout = 0u;
    update_minimap_text(tab);
    return G_SOURCE_REMOVE;
}


/**
 * @brief Schedule a minimap refresh.
 * @details Minimap refresh is delayed because it follows the main source buffer
 *          and can cause extra layout work. Keeping it low-priority preserves
 *          typing latency while still making the preview catch up after the
 *          user pauses.
 * @param tab The editor tab whose minimap should be refreshed.
 */
void editor_tab_schedule_minimap_update(EditorTab *tab) {
    if (!tab || !tab->win || !tab->win->minimap_enabled) return;
    graptos_source_cancel(&tab->minimap_timeout);
    tab->minimap_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                              80u,
                                              minimap_timeout_cb,
                                              tab,
                                              NULL);
}


/**
 * @brief Preview timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean preview_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->preview_timeout = 0u;
    editor_tab_update_preview(tab);
    return G_SOURCE_REMOVE;
}
/**
 * @brief Tab schedule preview update.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */


/**
 * @brief Schedule a markdown or LaTeX preview refresh.
 * @details Markdown and LaTeX previews rebuild derived buffers. Scheduling them
 *          after idle time keeps prose and code typing responsive while still
 *          making the preview catch up after the user pauses.
 * @param tab The editor tab whose preview should be refreshed.
 *
 * Markdown/LaTeX previews rebuild derived buffers.  Schedule them after idle
 * time instead of synchronously from buffer changes so prose/code typing stays
 * responsive.
 */
void editor_tab_schedule_preview_update(EditorTab *tab) {
    if (!tab || !tab->win || !tab->win->preview_enabled) return;
    graptos_source_cancel(&tab->preview_timeout);
    if (!editor_tab_live_features_allowed(tab)) {
        tab->preview_timeout = 0u;
        return;
    }
    tab->preview_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                             GRAPTOS_PREVIEW_DELAY_MS,
                                             preview_timeout_cb,
                                             tab,
                                             NULL);
}


/**
 * @brief Highlight timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
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
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_highlight(EditorTab *tab) {
    if (!tab) return;
    /* GtkSourceView performs syntax highlighting internally. There is no
     * Graptoς regex highlighter to schedule; keep this as a redraw compatibility
     * hook for undo/load paths that still call it.
     */
    if (tab->text_view) gtk_widget_queue_draw(tab->text_view);
    if (tab->minimap_view) gtk_widget_queue_draw(tab->minimap_view);
}
