/**
 * @file src/editor/editor_tab_lifecycle.inc.c
 * @brief Cleaf editor tab lifecycle module.
 */

char *editor_tab_basename(EditorTab *tab) {
    if (!tab || !tab->file_path) return g_strdup("Untitled");

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
 */
static void cancel_timeout_id(guint *id) {
    // Keep timeout cancellation behind one helper in case source handling changes.
    cleaf_source_cancel(id);
}


/**
 * @brief Editor tab large file mode.
 */
gboolean editor_tab_large_file_mode(EditorTab *tab) {
    if (!tab || !tab->buffer) return FALSE;

    /*
     * Large buffers skip expensive live features so typing and scrolling stay
     * responsive.
     */
    return (guint)gtk_text_buffer_get_char_count(tab->buffer)
        > CLEAF_LIVE_FEATURE_MAX_CHARS;
}


/**
 * @brief Editor tab live features allowed.
 */
gboolean editor_tab_live_features_allowed(EditorTab *tab) {
    // Live features are disabled for large files to avoid editor lag.
    return tab && tab->buffer && !editor_tab_large_file_mode(tab);
}


/**
 * @brief Editor tab highlighting allowed.
 */
gboolean editor_tab_highlighting_allowed(EditorTab *tab) {
    // Highlighting needs an active syntax and at least one loaded rule.
    return tab && tab->buffer && tab->active_syntax && tab->active_syntax->rules;
}


/**
 * @brief Editor tab reference features allowed.
 */
gboolean editor_tab_reference_features_allowed(EditorTab *tab) {
    // Reference lookup only needs a valid text buffer for now.
    return tab && tab->buffer;
}


/**
 * @brief Editor tab cancel live work.
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
    cancel_timeout_id(&tab->hover_timeout);
    cancel_timeout_id(&tab->hover_hide_timeout);

    if (tab->completion_popover) cleaf_popover_hide(tab->completion_popover);

    tab->completion_manual = FALSE;
    g_clear_pointer(&tab->completion_prefix, g_free);
}


/**
 * @brief Editor tab lightweight ui timeout cb.
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
 */
void popup_append_item(GtkWidget *menu,
                       const char *label,
                       GCallback callback,
                       gpointer data) {
    if (!menu) return;

    // Popover menus use Cleaf flat buttons instead of native menu rows.
    GtkWidget *item = cleaf_flat_button_new(label, NULL, callback, data);
    gtk_box_append(GTK_BOX(menu), item);
}


/**
 * @brief On close button clicked.
 */
void on_close_button_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;

    EditorTab *tab = user_data;

    // Route closing through the window so unsaved checks and tab replacement run.
    if (tab && tab->win) app_window_close_tab(tab->win, tab);
}


/**
 * @brief Editor tab text rect to popover parent.
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
 * @brief Editor tab destroy popovers.
 */
void editor_tab_destroy_popovers(EditorTab *tab) {
    if (!tab) return;

    /*
     * Popovers can outlive focus changes, so destroy them explicitly before the
     * tab or its text view goes away.
     */
    if (tab->completion_popover) {
        cleaf_widget_destroy(tab->completion_popover);
        tab->completion_popover = NULL;
    }

    if (tab->hover_popover) {
        cleaf_widget_destroy(tab->hover_popover);
        tab->hover_popover = NULL;
    }
}


/**
 * @brief Editor tab free.
 */
void editor_tab_free(EditorTab *tab) {
    if (!tab) return;

    /*
     * Cancel every pending source before freeing tab memory. Any one of these
     * callbacks may still hold the tab as user_data.
     */
    cleaf_source_cancel(&tab->highlight_timeout);
    cleaf_source_cancel(&tab->completion_timeout);
    cleaf_source_cancel(&tab->minimap_timeout);
    cleaf_source_cancel(&tab->preview_timeout);
    cleaf_source_cancel(&tab->diagnostics_timeout);
    cleaf_source_cancel(&tab->selection_match_timeout);
    cleaf_source_cancel(&tab->hover_timeout);
    cleaf_source_cancel(&tab->hover_hide_timeout);
    cleaf_source_cancel(&tab->ui_refresh_timeout);

    editor_tab_destroy_popovers(tab);

    g_free(tab->file_path);
    g_free(tab->last_snapshot);
    g_free(tab->kill_buffer);
    g_free(tab->search_text);
    g_free(tab->completion_prefix);
    g_free(tab->hover_word);

    if (tab->undo_stack) g_ptr_array_free(tab->undo_stack, TRUE);
    if (tab->redo_stack) g_ptr_array_free(tab->redo_stack, TRUE);

    g_free(tab);
}


/**
 * @brief Editor tab is locked.
 */
gboolean editor_tab_is_locked(EditorTab *tab) {
    return tab ? tab->locked : FALSE;
}


/**
 * @brief Editor tab set locked.
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
 */
void editor_tab_update_title(EditorTab *tab) {
    if (!tab) return;

    char *base = editor_tab_basename(tab);

    /*
     * Keep the tab label compact: modified marker first, then lock marker, then
     * the display basename.
     */
    char *label = g_strdup_printf("%s%s%s",
                                  tab->modified ? "*" : "",
                                  tab->locked ? "🔒 " : "",
                                  base);

    if (tab->tab_title) gtk_label_set_text(GTK_LABEL(tab->tab_title), label);

    g_free(label);
    g_free(base);
}


/**
 * @brief Editor tab update status.
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

    /*
     * Show which highlighting path is active because YAML overrides can affect
     * colours differently from plain GtkSourceView.
     */
    const char *hl_engine = tab->win && tab->win->use_yaml_style_overrides
        ? "GtkSource + YAML overrides"
        : "GtkSource";

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
        "%s | Ln %d, Col %d | %d lines | %d chars | %s | HL %s | %s%s%s%s",
        base,
        line,
        col,
        lines,
        chars,
        syntax,
        hl_engine,
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