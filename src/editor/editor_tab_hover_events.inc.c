/**
 * @file src/editor/editor_tab_hover_events.inc.c
 * @brief Cleaf editor tab hover events module.
 */

gboolean hover_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;

    // Timeout: clear the stored source id.
    tab->hover_timeout = 0u;

    if (!editor_tab_reference_features_allowed(tab)) return G_SOURCE_REMOVE;
    if (!tab->hover_word || !tab->hover_popover || !tab->hover_list) return G_SOURCE_REMOVE;

    /*
     * Hex colours are handled before reference lookup.
     */
    GdkRGBA rgba;
    if (hex_to_rgba(tab->hover_word, &rgba)) {
        tab->color_preview_rgba = rgba;

        GtkTextIter cursor;
        GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, mark);

        show_color_preview_in_hover(tab, tab->hover_word, &cursor);
        return G_SOURCE_REMOVE;
    }

    /*
     * Limit reference results so the hover popover stays quick and does not
     * become a project-wide search window.
     */
    GPtrArray *refs = index_references_for_word(tab, tab->hover_word, 30u);
    if (!refs || refs->len == 0u) {
        if (refs) g_ptr_array_free(refs, TRUE);
        return G_SOURCE_REMOVE;
    }

    tab->color_preview_valid = FALSE;
    tab->hover_pinned = FALSE;

    // Rebuild the hover list for the current word only.
    hover_clear_rows(tab);

    GtkWidget *header = gtk_list_box_row_new();
    gtk_widget_set_sensitive(header, FALSE);

    char *heading = g_strdup_printf("References to %s", tab->hover_word);
    GtkWidget *heading_label = gtk_label_new(heading);
    gtk_label_set_xalign(GTK_LABEL(heading_label), 0.0f);
    gtk_widget_set_margin_start(heading_label, 10);
    gtk_widget_set_margin_end(heading_label, 10);
    gtk_widget_set_margin_top(heading_label, 8);
    gtk_widget_set_margin_bottom(heading_label, 6);
    gtk_widget_add_css_class(heading_label, "cleaf-ref-heading");

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(header), heading_label);
    gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list), header, -1);
    g_free(heading);

    for (guint i = 0u; i < refs->len; i++) {
        gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list),
                            reference_row_new(g_ptr_array_index(refs, i)),
                            -1);
    }

    g_ptr_array_free(refs, TRUE);

    /*
     * Point slightly below the pointer. Fixes issue where word is covered. 
     * Sometime this fails. Need to investigate.
     */
    GdkRectangle rect;
    rect.x = tab->hover_x;
    rect.y = tab->hover_y + 18;
    rect.width = 1;
    rect.height = 1;

    if (!editor_tab_text_rect_to_popover_parent(tab, &rect)) {
        return G_SOURCE_REMOVE;
    }

    gtk_popover_set_pointing_to(GTK_POPOVER(tab->hover_popover), &rect);

    /*
     * Lock while visible so normal pointer motion does not immediatley replace
     * the contents.
     */
    tab->hover_popover_locked = TRUE;
    cleaf_popover_show(tab->hover_popover);

    // Return focus to the editor so keyboard editing continues naturally.
    gtk_widget_grab_focus(tab->text_view);

    return G_SOURCE_REMOVE;
}


/**
 * @brief On text view motion.
 */
void on_text_view_motion(GtkEventControllerMotion *controller,
                         double x,
                         double y,
                         gpointer user_data) {
    EditorTab *tab = user_data;
    GtkWidget *widget = gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(controller));

    if (!tab || !widget) return;

    // Keep the latest pointer position for delayed hover placement.
    tab->pointer_x = x;
    tab->pointer_y = y;
    tab->pointer_valid = TRUE;

    // Do not reschedule lookups while the current hover popover is visible.
    if (tab->hover_popover && gtk_widget_get_visible(tab->hover_popover)) {
        return;
    }

    /*
     * Reference inspection is opt-in. Outside that mode, motion only hides stale
     * previews when the pointer is not inside the hover.
     */
    if (!tab->inspect_reference_active) {
        if (!tab->hover_pointer_inside) hide_hover_preview(tab);
        return;
    }

    cancel_hover_hide(tab);

    GtkTextIter iter;
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget), &iter,
                                       (int)x, (int)y);

    // Actual lookup is delayed so normal mouse movement does not spam indexing.
    schedule_reference_lookup_at_iter(tab, &iter);
}


/**
 * @brief On text view leave.
 */
void on_text_view_leave(GtkEventControllerMotion *controller,
                        gpointer user_data) {
    (void)controller;

    EditorTab *tab = user_data;
    if (!tab) return;

    /*
     * Give the pointer a chance to enter the popover. Without this delay the
     * hover can disappear while moving from the editor into the reference list.
     */
    if (tab->inspect_reference_active && tab->hover_popover &&
        gtk_widget_get_visible(tab->hover_popover)) {
        schedule_hover_transition_hide(tab);
    } else if (!tab->hover_pointer_inside) {
        hide_hover_preview(tab);
    }
}


/**
 * @brief On hover popover enter.
 */
void on_hover_popover_enter(GtkEventControllerMotion *controller,
                            double x,
                            double y,
                            gpointer user_data) {
    (void)controller;
    (void)x;
    (void)y;

    EditorTab *tab = user_data;

    // Pointer is inside the popover, so do not hide it from text-view leave.
    if (tab) tab->hover_pointer_inside = TRUE;

    cancel_hover_hide(tab);
}


/**
 * @brief On hover popover leave.
 */
void on_hover_popover_leave(GtkEventControllerMotion *controller,
                            gpointer user_data) {
    (void)controller;

    EditorTab *tab = user_data;
    if (!tab) return;

    // Once the pointer leaves the popover, normal hover cleanup can happen.
    tab->hover_pointer_inside = FALSE;
    hide_hover_preview(tab);
}


/**
 * @brief Color preview row new.
 */
GtkWidget *color_preview_row_new(EditorTab *tab, const char *hex) {
    GtkWidget *row = gtk_list_box_row_new();

    // Colour preview rows are informational, not selectable actions.
    gtk_widget_set_sensitive(row, FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 8);

    GtkWidget *swatch = gtk_drawing_area_new();
    gtk_widget_set_size_request(swatch, 48, 28);

    GtkWidget *label = gtk_label_new(hex ? hex : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "cleaf-color-label");

    gtk_box_append(GTK_BOX(box), swatch);
    gtk_box_append(GTK_BOX(box), label);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

    /*
     * The draw callback reads tab->color_preview_rgba, so the row only needs
     * the tab pointer and does not keep a separate colour copy.
     */
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(swatch),
                                   on_color_swatch_draw, tab, NULL);

    return row;
}


/**
 * @brief Show color preview in hover.
 */
void show_color_preview_in_hover(EditorTab *tab,
                                 const char *hex,
                                 GtkTextIter *where) {
    if (!tab || !tab->hover_popover || !tab->hover_list || !hex || !where) return;

    // Colour preview reuses the hover popover but replaces reference rows.
    hover_clear_rows(tab);

    GtkWidget *header = gtk_list_box_row_new();
    gtk_widget_set_sensitive(header, FALSE);

    GtkWidget *heading = gtk_label_new("Colour preview");
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);
    gtk_widget_set_margin_start(heading, 10);
    gtk_widget_set_margin_end(heading, 10);
    gtk_widget_set_margin_top(heading, 8);
    gtk_widget_set_margin_bottom(heading, 4);
    gtk_widget_add_css_class(heading, "cleaf-ref-heading");

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(header), heading);
    gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list), header, -1);
    gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list),
                        color_preview_row_new(tab, hex),
                        -1);

    /*
     * Place the preview under the colour text rather than at the raw pointer
     * position. This is more stable for selected text and keyboard movement.
     */
    GdkRectangle location;
    gtk_text_view_get_iter_location(GTK_TEXT_VIEW(tab->text_view), where,
                                    &location);
    location.y += location.height;
    location.width = 1;

    if (location.height < 1) location.height = 1;

    tab->color_preview_valid = TRUE;
    tab->hover_pinned = FALSE;
    tab->hover_popover_locked = TRUE;

    if (!editor_tab_text_rect_to_popover_parent(tab, &location)) return;

    gtk_popover_set_pointing_to(GTK_POPOVER(tab->hover_popover), &location);
    cleaf_popover_show(tab->hover_popover);

    // Keep typing focus in the editor after showing the preview.
    gtk_widget_grab_focus(tab->text_view);
}


/**
 * @brief Maybe show color preview.
 */
void maybe_show_color_preview(EditorTab *tab) {
    if (!tab || !tab->buffer || !tab->hover_popover || !tab->hover_list) return;
    if (!tab->inspect_reference_active) return;

    GtkTextIter start;
    GtkTextIter end;

    /*
     * Colour preview from selection only makes sense when text is actually
     * selected. Otherwise hide stale colour previews.
     */
    if (!gtk_text_buffer_get_selection_bounds(tab->buffer, &start, &end)) {
        hide_color_preview(tab);
        return;
    }

    char *selected = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
    if (!selected) {
        hide_color_preview(tab);
        return;
    }

    /*
     * Avoid treating large selections as hover words. This keeps accidental
     * multi-line selections from triggering slow reference lookups.
     */
    if (g_utf8_strlen(selected, -1) > 80) {
        g_free(selected);
        hide_color_preview(tab);
        return;
    }

    GdkRGBA rgba;

    if (!hex_to_rgba(selected, &rgba)) {
        /*
         * Non-colour selections can still be reference terms. Strip whitespace
         * and schedule the normal delayed lookup.
         */
        if (g_utf8_strlen(selected, -1) >= 2 &&
            g_utf8_strlen(selected, -1) <= 80) {
            g_strstrip(selected);

            if (selected[0] != '\0') {
                GdkRectangle location;
                gtk_text_view_get_iter_location(GTK_TEXT_VIEW(tab->text_view),
                                                &end,
                                                &location);

                tab->hover_x = location.x;
                tab->hover_y = location.y + location.height;

                g_free(tab->hover_word);
                tab->hover_word = g_strdup(selected);

                cleaf_source_cancel(&tab->hover_timeout);
                tab->hover_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                        CLEAF_HOVER_DELAY_MS,
                                                        hover_timeout_cb,
                                                        tab,
                                                        NULL);
            }
        }

        g_free(selected);
        return;
    }

    // Valid hex selection shows the swatch immediately.
    tab->color_preview_rgba = rgba;
    show_color_preview_in_hover(tab, selected, &end);

    g_free(selected);
}