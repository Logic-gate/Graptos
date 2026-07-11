gboolean hex_to_rgba(const char *text, GdkRGBA *rgba) {
    if (!text || !rgba) return FALSE;

    /*
     * Work on a copy so callers can pass selected buffer text directly without
     * losing their original spacing.
     */
    char *copy = g_strdup(text);
    if (!copy) return FALSE;

    g_strstrip(copy);

    /*
     * Only accept normal hex colours here. Other GTK colour names are valid for
     * config, but hover preview should stay predictable inside code.
     */
    gsize len = strlen(copy);
    gboolean ok = FALSE;

    if ((len == 7u || len == 9u) && copy[0] == '#') {
        ok = gdk_rgba_parse(rgba, copy);
    }

    g_free(copy);
    return ok;
}


void on_color_swatch_draw(GtkDrawingArea *area,
                          cairo_t *cr,
                          int width,
                          int height,
                          gpointer user_data) {
    (void)area;
    (void)width;
    (void)height;

    EditorTab *tab = user_data;
    if (!tab || !tab->color_preview_valid) return;

    // Draw uses the cached preview colour so the row does not own colour state.
    cairo_set_source_rgba(cr,
                          tab->color_preview_rgba.red,
                          tab->color_preview_rgba.green,
                          tab->color_preview_rgba.blue,
                          tab->color_preview_rgba.alpha);
    cairo_paint(cr);

    return;
}


void hide_color_preview(EditorTab *tab) {
    if (!tab || !tab->color_preview_valid) return;

    // Colour preview is just a hover mode, so clear its state and hide the popover.
    tab->color_preview_valid = FALSE;
    tab->hover_pinned = FALSE;

    if (tab->hover_popover) cleaf_popover_hide(tab->hover_popover);
}


void cancel_hover_hide(EditorTab *tab) {
    if (!tab) return;
    if (tab->hover_hide_timeout != 0u) {
        cleaf_source_cancel(&tab->hover_hide_timeout);
    }
}


void hide_hover_preview(EditorTab *tab) {
    if (!tab) return;

    // Cancel pending lookup first so hidden hover state is not rebuilt later.
    if (tab->hover_timeout != 0u) {
        cleaf_source_cancel(&tab->hover_timeout);
    }

    cancel_hover_hide(tab);

    if (tab->hover_popover) cleaf_popover_hide(tab->hover_popover);

    tab->color_preview_valid = FALSE;
    tab->hover_pinned = FALSE;
    tab->hover_popover_locked = FALSE;

    g_clear_pointer(&tab->hover_word, g_free);
}


gboolean hover_transition_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;

    // Timeout has fired, so clear the stored source id.
    tab->hover_hide_timeout = 0u;

    /*
     * Only hide if the pointer did not enter the popover during the transition
     * delay.
     */
    if (!tab->hover_pointer_inside) hide_hover_preview(tab);

    return G_SOURCE_REMOVE;
}


void schedule_hover_transition_hide(EditorTab *tab) {
    if (!tab || tab->hover_pointer_inside) return;
    if (tab->hover_hide_timeout != 0u) return;

    /* Bridge the pointer move from the editor into the popover. */
    tab->hover_hide_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                 320u,
                                                 hover_transition_timeout_cb,
                                                 tab,
                                                 NULL);
}


char *word_at_iter(GtkTextBuffer *buffer, GtkTextIter *iter) {
    if (!buffer || !iter) return NULL;

    GtkTextIter start = *iter;
    GtkTextIter end = *iter;

    /*
     * Include # so colour literals such as #fffff can be handled by the same
     * hover path as references.
     */
    while (!gtk_text_iter_starts_line(&start)) {
        GtkTextIter prev = start;

        if (!gtk_text_iter_backward_char(&prev)) break;

        gunichar ch = gtk_text_iter_get_char(&prev);
        if (!completion_is_word_char(ch) && ch != '#') break;

        start = prev;
    }

    while (!gtk_text_iter_ends_line(&end) && !gtk_text_iter_is_end(&end)) {
        gunichar ch = gtk_text_iter_get_char(&end);
        if (!completion_is_word_char(ch) && ch != '#') break;

        if (!gtk_text_iter_forward_char(&end)) break;
    }

    if (gtk_text_iter_equal(&start, &end)) return NULL;

    char *word = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    // One-character terms are too noisy for reference lookup.
    if (!word || g_utf8_strlen(word, -1) < 2) {
        g_free(word);
        return NULL;
    }

    return word;
}


static void set_hover_anchor_from_iter(EditorTab *tab, GtkTextIter *iter) {
    if (!tab || !iter || !tab->text_view) return;

    /*
     * Store the anchor under the iter so delayed hover lookup can position the
     * popover without keeping the iter alive.
     */
    GdkRectangle location;
    gtk_text_view_get_iter_location(GTK_TEXT_VIEW(tab->text_view), iter,
                                    &location);

    tab->hover_x = location.x;
    tab->hover_y = location.y + location.height;
}


static void schedule_reference_lookup_at_iter(EditorTab *tab,
                                              GtkTextIter *iter) {
    if (!tab || !iter || !tab->buffer) return;

    char *word = word_at_iter(tab->buffer, iter);
    if (!word) return;

    /*
     * If the same word already has a pending lookup, do not reset the timer on
     * every small pointer movement.
     */
    if (tab->hover_word && strcmp(tab->hover_word, word) == 0 &&
        tab->hover_timeout != 0u) {
        g_free(word);
        return;
    }

    cleaf_source_cancel(&tab->hover_timeout);

    tab->color_preview_valid = FALSE;
    tab->hover_pinned = FALSE;
    tab->hover_popover_locked = FALSE;

    g_free(tab->hover_word);
    tab->hover_word = word;

    set_hover_anchor_from_iter(tab, iter);

    /*
     * Delay the lookup slightly so normal mouse movement does not cause constant
     * indexing work.
     */
    tab->hover_timeout = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
                                            120u,
                                            hover_timeout_cb,
                                            tab,
                                            NULL);
}


void editor_tab_show_reference_at_pointer_or_cursor(EditorTab *tab) {
    if (!tab || !tab->text_view || !tab->buffer) return;

    GtkTextIter iter;

    /*
     * Prefer the latest pointer location. If there is no pointer state yet, use
     * the insertion cursor so keyboard-triggered inspection still works.
     */
    if (tab->pointer_valid) {
        gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(tab->text_view),
                                           &iter,
                                           (int)tab->pointer_x,
                                           (int)tab->pointer_y);
    } else {
        GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &iter, mark);
    }

    schedule_reference_lookup_at_iter(tab, &iter);
}


void hover_clear_rows(EditorTab *tab) {
    if (!tab || !tab->hover_list) return;

    // Hover content is rebuilt from scratch for each inspected word.
    cleaf_list_box_clear(tab->hover_list);
}


void editor_tab_jump_to_line_internal(EditorTab *tab, guint line) {
    if (!tab || !tab->buffer || line == 0u) return;

    /*
     * Clamp to the buffer line count so references from stale indexes do not
     * request an invalid line.
     */
    int max_lines = gtk_text_buffer_get_line_count(tab->buffer);
    if ((guint)max_lines < line) line = (guint)max_lines;

    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line(tab->buffer, &iter, (int)line - 1);
    gtk_text_buffer_place_cursor(tab->buffer, &iter);

    /*
     * Scroll with a little context above the target line instead of placing it
     * at the very top of the editor.
     */
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(tab->text_view),
                                 &iter,
                                 0.22,
                                 FALSE,
                                 0.0,
                                 0.0);

    gtk_widget_grab_focus(tab->text_view);
}


void editor_tab_jump_to_line(EditorTab *tab, guint line) {
    // Public wrapper keeps callers away from the internal implementation name.
    editor_tab_jump_to_line_internal(tab, line);
}


void hover_row_activated(GtkListBox *box,
                         GtkListBoxRow *row,
                         gpointer user_data) {
    (void)box;

    EditorTab *tab = user_data;
    if (!tab || !row) return;

    /*
     * Reference rows store path and line as row data so activation does not need
     * to keep the whole IndexReference object alive.
     */
    const char *path = g_object_get_data(G_OBJECT(row), "cleaf-ref-path");
    guint line = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row),
                                                    "cleaf-ref-line"));

    hide_hover_preview(tab);

    /*
     * Cross-file references open the target file first, then jump in the newly
     * active tab.
     */
    if (path && tab->file_path && strcmp(path, tab->file_path) != 0 && tab->win) {
        if (app_window_open_file(tab->win, path)) {
            EditorTab *opened = app_window_current_tab(tab->win);
            editor_tab_jump_to_line_internal(opened, line);
            return;
        }
    }

    editor_tab_jump_to_line_internal(tab, line);
}


GtkWidget *reference_row_new(IndexReference *ref) {
    GtkWidget *row = gtk_list_box_row_new();

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 6);
    gtk_widget_set_margin_bottom(box, 6);

    // Top line shows a compact location. The snippet stays on its own row.
    char *top = g_strdup_printf("%s:%u",
                                ref->display ? ref->display : "buffer",
                                ref->line);

    GtkWidget *title = gtk_label_new(top);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "cleaf-ref-title");

    GtkWidget *kind = gtk_label_new(ref->kind ? ref->kind : "reference");
    gtk_label_set_xalign(GTK_LABEL(kind), 0.0f);
    gtk_widget_add_css_class(kind, "cleaf-ref-kind");

    GtkWidget *snippet = gtk_label_new(ref->snippet ? ref->snippet : "");
    gtk_label_set_xalign(GTK_LABEL(snippet), 0.0f);

    // Reference snippets should not make the popover wider than the editor.
    gtk_label_set_ellipsize(GTK_LABEL(snippet), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(snippet, "cleaf-ref-snippet");

    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), kind);
    gtk_box_append(GTK_BOX(box), snippet);

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

    /*
     * Store only the data needed for activation. The path is owned by the row;
     * the line number is stored as pointer-sized integer data.
     */
    if (ref->path) {
        g_object_set_data_full(G_OBJECT(row),
                               "cleaf-ref-path",
                               g_strdup(ref->path),
                               g_free);
    }

    g_object_set_data(G_OBJECT(row),
                      "cleaf-ref-line",
                      GUINT_TO_POINTER(ref->line));

    g_free(top);

    return row;
}