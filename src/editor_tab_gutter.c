/**
 * @file src/editor_tab_gutter.c
 * @brief Graptoς editor tab gutter module.
 * @details The gutter and minimap mirror the editor; they are not a second editor. We
 *          schedule their refreshes because typing should win over decorative updates every
 *          time.
 */

#include "editor_tab_private.h"

/**
 * @brief Decimal digits.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param value The value being parsed, stored, or applied.
 * @return The computed value requested by the caller.
 */
int decimal_digits(int value) {
    int digits = 1;
    while (value >= 10) {
        value /= 10;
        digits++;
    }
    return digits;
}


/**
 * @brief Update gutter width.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void update_gutter_width(EditorTab *tab) {
    if (!tab || !tab->buffer || !tab->gutter) return;
    if (!gtk_widget_get_visible(tab->gutter)) return;
    int lines = gtk_text_buffer_get_line_count(tab->buffer);
    int width = decimal_digits(lines) * 9 + 18;
    if (width < 42) width = 42;
    if (width > 110) width = 110;
    if (tab->cached_gutter_width != (guint)width) {
        tab->cached_gutter_width = (guint)width;
        gtk_widget_set_size_request(tab->gutter, width, -1);
    }
    gtk_widget_queue_draw(tab->gutter);
}


/**
 * @brief Gutter draw background.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param cr The cr supplied by the caller.
 * @param width The width supplied by the caller.
 * @param height The height supplied by the caller.
 */
static void gutter_draw_background(GtkWidget *widget, cairo_t *cr,
                                   gint width, gint height) {
    (void)widget;
    cairo_set_source_rgb(cr, 0.095, 0.102, 0.113);
    cairo_rectangle(cr, 0.0, 0.0, (double)width, (double)height);
    cairo_fill(cr);

    cairo_set_source_rgb(cr, 0.235, 0.247, 0.270);
    cairo_rectangle(cr, (double)width - 1.0, 0.0, 1.0, (double)height);
    cairo_fill(cr);
}

/**
 * @brief Gutter draw line number.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param cr The cr supplied by the caller.
 * @param layout The layout supplied by the caller.
 * @param gutter_width The gutter width supplied by the caller.
 * @param draw_y The draw y supplied by the caller.
 * @param line_height The line height supplied by the caller.
 * @param line_no The line no supplied by the caller.
 * @param current_line The current line supplied by the caller.
 */
static void gutter_draw_line_number(cairo_t *cr, PangoLayout *layout,
                                    gint gutter_width, gint draw_y,
                                    gint line_height, int line_no,
                                    gboolean current_line) {
    if (current_line) {
        cairo_set_source_rgb(cr, 0.145, 0.154, 0.170);
        cairo_rectangle(cr, 0.0, (double)draw_y,
                        (double)gutter_width, (double)line_height);
        cairo_fill(cr);
    }

    char *num = g_strdup_printf("%d", line_no + 1);
    pango_layout_set_text(layout, num, -1);
    gint text_width = 0;
    gint text_height = 0;
    pango_layout_get_pixel_size(layout, &text_width, &text_height);

    gint draw_x = gutter_width - text_width - 8;
    if (draw_x < 2) draw_x = 2;
    gint text_y = draw_y + ((line_height - text_height) / 2);

    if (current_line) cairo_set_source_rgb(cr, 0.770, 0.790, 0.830);
    else cairo_set_source_rgb(cr, 0.540, 0.565, 0.600);
    cairo_move_to(cr, (double)draw_x, (double)text_y);
    pango_cairo_show_layout(cr, layout);
    g_free(num);
}

/**
 * @brief On gutter draw.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param area The area supplied by the caller.
 * @param cr The cr supplied by the caller.
 * @param draw_width The draw width supplied by the caller.
 * @param draw_height The draw height supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_gutter_draw(GtkDrawingArea *area, cairo_t *cr, int draw_width,
                    int draw_height, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(area);
    (void)draw_width;
    (void)draw_height;
    EditorTab *tab = user_data;
    if (!tab || !tab->text_view || !tab->buffer) return;

    GtkTextView *view = GTK_TEXT_VIEW(tab->text_view);
    GdkRectangle visible;
    gtk_text_view_get_visible_rect(view, &visible);

    gint width = gtk_widget_get_width(widget);
    gint height = gtk_widget_get_height(widget);
    gutter_draw_background(widget, cr, width, height);

    GtkTextIter iter;
    gint top_y = 0;
    gtk_text_view_get_line_at_y(view, &iter, visible.y, &top_y);

    GtkTextIter end_iter;
    gtk_text_buffer_get_end_iter(tab->buffer, &end_iter);
    int max_line = gtk_text_iter_get_line(&end_iter);

    GtkTextIter cursor;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, mark);
    int cursor_line = gtk_text_iter_get_line(&cursor);

    PangoLayout *layout = gtk_widget_create_pango_layout(widget, NULL);
    while (TRUE) {
        int line_no = gtk_text_iter_get_line(&iter);
        gint y = 0;
        gint line_height = 0;
        gtk_text_view_get_line_yrange(view, &iter, &y, &line_height);
        if (y > visible.y + visible.height) break;

        gutter_draw_line_number(cr, layout, width, y - visible.y, line_height,
                                line_no, line_no == cursor_line);
        if (line_no >= max_line) break;
        if (!gtk_text_iter_forward_line(&iter)) break;
    }

    g_object_unref(layout);
    return;
}


/**
 * @brief Sync minimap scroll.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param main_adj The main adj supplied by the caller.
 */
static void sync_minimap_scroll(EditorTab *tab, GtkAdjustment *main_adj) {
    if (!tab || !tab->minimap_scrolled || !main_adj) return;
    GtkAdjustment *mini_adj = gtk_scrolled_window_get_vadjustment(
        GTK_SCROLLED_WINDOW(tab->minimap_scrolled));
    if (!mini_adj) return;

    double main_upper = gtk_adjustment_get_upper(main_adj);
    double main_page = gtk_adjustment_get_page_size(main_adj);
    double main_max = main_upper - main_page;
    if (main_max <= 1.0) return;

    double mini_upper = gtk_adjustment_get_upper(mini_adj);
    double mini_page = gtk_adjustment_get_page_size(mini_adj);
    double mini_max = mini_upper - mini_page;
    if (mini_max <= 1.0) return;

    double ratio = gtk_adjustment_get_value(main_adj) / main_max;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    gtk_adjustment_set_value(mini_adj, ratio * mini_max);
}

/**
 * @brief On vadjustment value changed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param adjustment The adjustment supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_vadjustment_value_changed(GtkAdjustment *adjustment, gpointer user_data) {
    EditorTab *tab = user_data;
    if (tab && tab->gutter) gtk_widget_queue_draw(tab->gutter);
    if (tab) {
        sync_minimap_scroll(tab, adjustment);
        if (tab->minimap_view) gtk_widget_queue_draw(tab->minimap_view);
        editor_tab_reposition_visible_cursor_popovers(tab);
    }
}


/**
 * @brief Ensure match tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void ensure_match_tag(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    const char *bg = tab->win && tab->win->search_match_bg_color ?
        tab->win->search_match_bg_color : "#515c7a";
    const char *fg = tab->win && tab->win->search_match_fg_color ?
        tab->win->search_match_fg_color : "#ffffff";
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(tab->buffer);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, "graptos-selection-match");
    if (tag) {
        g_object_set(tag, "background", bg, "foreground", fg,
                     "underline", PANGO_UNDERLINE_SINGLE, NULL);
        return;
    }
    gtk_text_buffer_create_tag(tab->buffer, "graptos-selection-match",
                               "background", bg,
                               "foreground", fg,
                               "underline", PANGO_UNDERLINE_SINGLE,
                               NULL);
}


/**
 * @brief Ensure minimap match tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void ensure_minimap_match_tag(EditorTab *tab) {
    (void)tab;
}


/**
 * @brief Clear minimap matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void clear_minimap_matches(EditorTab *tab) {
    if (tab && tab->minimap_view) gtk_widget_queue_draw(tab->minimap_view);
}


/**
 * @brief Clear selection matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void clear_selection_matches(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    if (tab->selection_matches_active) {
        GtkTextTagTable *table = gtk_text_buffer_get_tag_table(tab->buffer);
        if (gtk_text_tag_table_lookup(table, "graptos-selection-match")) {
            GtkTextIter start;
            GtkTextIter end;
            gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
            gtk_text_buffer_remove_tag_by_name(tab->buffer, "graptos-selection-match", &start, &end);
        }
    }
    clear_minimap_matches(tab);
    tab->selection_matches_active = FALSE;
}


/**
 * @brief Apply minimap match.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param start The start supplied by the caller.
 * @param end The end supplied by the caller.
 */
void apply_minimap_match(EditorTab *tab, GtkTextIter *start, GtkTextIter *end) {
    (void)start;
    (void)end;
    if (tab && tab->minimap_view) gtk_widget_queue_draw(tab->minimap_view);
}


/**
 * @brief Update selection matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void update_selection_matches(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    ensure_match_tag(tab);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
    gtk_text_buffer_remove_tag_by_name(tab->buffer, "graptos-selection-match", &start, &end);
    clear_minimap_matches(tab);

    if ((guint)gtk_text_buffer_get_char_count(tab->buffer) > GRAPTOS_SELECTION_MATCH_MAX_CHARS) {
        return;
    }

    GtkTextIter sel_start;
    GtkTextIter sel_end;
    if (!gtk_text_buffer_get_selection_bounds(tab->buffer, &sel_start, &sel_end)) return;
    char *needle = gtk_text_buffer_get_text(tab->buffer, &sel_start, &sel_end, FALSE);
    if (!needle) return;
    if (needle[0] == '\0' || strlen(needle) > 128u || strchr(needle, '\n') || strchr(needle, '\r')) {
        g_free(needle);
        return;
    }

    GtkTextIter search = start;
    GtkTextIter match_start;
    GtkTextIter match_end;
    guint count = 0u;
    while (gtk_text_iter_forward_search(&search, needle, GTK_TEXT_SEARCH_TEXT_ONLY, &match_start, &match_end, &end)) {
        gtk_text_buffer_apply_tag_by_name(tab->buffer, "graptos-selection-match", &match_start, &match_end);
        tab->selection_matches_active = TRUE;
        apply_minimap_match(tab, &match_start, &match_end);
        search = match_end;
        count++;
        if (count > 3000u) break;
    }
    g_free(needle);
}


/**
 * @brief Selection match timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean selection_match_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->selection_match_timeout = 0u;
    update_selection_matches(tab);
    return G_SOURCE_REMOVE;
}


/**
 * @brief Editor tab schedule selection matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_selection_matches(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    graptos_source_cancel(&tab->selection_match_timeout);
    if (!editor_tab_live_features_allowed(tab)) {
        tab->selection_match_timeout = 0u;
        return;
    }

    GtkTextIter sel_start;
    GtkTextIter sel_end;
    if (!gtk_text_buffer_get_selection_bounds(tab->buffer, &sel_start, &sel_end)) {
        return;
    }
    if (gtk_text_iter_get_line(&sel_start) != gtk_text_iter_get_line(&sel_end)) {
        return;
    }
    gint start_offset = gtk_text_iter_get_offset(&sel_start);
    gint end_offset = gtk_text_iter_get_offset(&sel_end);
    if (end_offset <= start_offset || end_offset - start_offset > 128) {
        return;
    }

    tab->selection_match_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                     GRAPTOS_SELECTION_MATCH_DELAY_MS,
                                                     selection_match_timeout_cb,
                                                     tab,
                                                     NULL);
}


/**
 * @brief Clamp color channel.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param value The value being parsed, stored, or applied.
 * @return The computed value requested by the caller.
 */
static int clamp_color_channel(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return value;
}


/**
 * @brief Normalize color literal.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param literal The literal supplied by the caller.
 * @param bg_out Output storage filled when the operation can provide a value.
 * @param fg_out Output storage filled when the operation can provide a value.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean normalize_color_literal(const char *literal, char **bg_out, char **fg_out) {
    if (!literal || !bg_out || !fg_out) return FALSE;

    GdkRGBA rgba;
    gboolean parsed = gdk_rgba_parse(&rgba, literal);

    if (!parsed && literal[0] == '#') {
        guint digits = 0u;
        while (g_ascii_isxdigit(literal[1u + digits]) && digits < 8u) digits++;
        if (digits >= 3u) {
            char hex[7] = {0};
            for (guint i = 0u; i < 6u; i++) {
                hex[i] = i < digits ? literal[1u + i] : '0';
            }
            unsigned int rgb = 0u;
            if (sscanf(hex, "%06x", &rgb) == 1) {
                int red = (int)((rgb >> 16) & 0xffu);
                int green = (int)((rgb >> 8) & 0xffu);
                int blue = (int)(rgb & 0xffu);
                rgba.red = (float)((double)red / 255.0);
                rgba.green = (float)((double)green / 255.0);
                rgba.blue = (float)((double)blue / 255.0);
                rgba.alpha = 1.0;
                parsed = TRUE;
            }
        }
    }

    if (!parsed) return FALSE;

    int red = clamp_color_channel((int)(rgba.red * 255.0 + 0.5));
    int green = clamp_color_channel((int)(rgba.green * 255.0 + 0.5));
    int blue = clamp_color_channel((int)(rgba.blue * 255.0 + 0.5));
    double luminance = 0.2126 * rgba.red + 0.7152 * rgba.green + 0.0722 * rgba.blue;

    *bg_out = g_strdup_printf("#%02x%02x%02x", red, green, blue);
    *fg_out = g_strdup(luminance > 0.55 ? "#111111" : "#ffffff");
    return *bg_out && *fg_out;
}


/**
 * @brief Apply color literal tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param start_offset The start offset supplied by the caller.
 * @param end_offset The end offset supplied by the caller.
 * @param background The background supplied by the caller.
 * @param foreground The foreground supplied by the caller.
 * @param index The index supplied by the caller.
 */
static void apply_color_literal_tag(EditorTab *tab,
                                    gint start_offset,
                                    gint end_offset,
                                    const char *background,
                                    const char *foreground,
                                    guint index) {
    if (!tab || !tab->buffer || start_offset >= end_offset || !background || !foreground) return;
    if (!tab->color_literal_tag_names) {
        tab->color_literal_tag_names = g_ptr_array_new_with_free_func(g_free);
    }

    char *name = g_strdup_printf("graptos-color-literal-%u", index);
    if (!name) return;

    gtk_text_buffer_create_tag(tab->buffer, name,
                               "background", background,
                               "foreground", foreground,
                               NULL);

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_iter_at_offset(tab->buffer, &start, start_offset);
    gtk_text_buffer_get_iter_at_offset(tab->buffer, &end, end_offset);
    gtk_text_buffer_apply_tag_by_name(tab->buffer, name, &start, &end);
    g_ptr_array_add(tab->color_literal_tag_names, name);
    tab->color_literals_active = TRUE;
}


/**
 * @brief Clear color literals.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void clear_color_literals(EditorTab *tab) {
    if (!tab || !tab->buffer || !tab->color_literal_tag_names) {
        if (tab) tab->color_literals_active = FALSE;
        return;
    }

    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(tab->buffer);
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
    for (guint i = 0u; i < tab->color_literal_tag_names->len; i++) {
        const char *name = g_ptr_array_index(tab->color_literal_tag_names, i);
        GtkTextTag *tag = gtk_text_tag_table_lookup(table, name);
        if (!tag) continue;
        gtk_text_buffer_remove_tag(tab->buffer, tag, &start, &end);
        gtk_text_tag_table_remove(table, tag);
    }
    g_ptr_array_set_size(tab->color_literal_tag_names, 0u);
    tab->color_literals_active = FALSE;
}


/**
 * @brief Check color token boundary.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param c The c supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean color_token_boundary(char c) {
    return c == '\0' || !(g_ascii_isalnum(c) || c == '_' || c == '-' || c == '#');
}


/**
 * @brief Maybe apply color token.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param token The token supplied by the caller.
 * @param start_offset The start offset supplied by the caller.
 * @param end_offset The end offset supplied by the caller.
 * @param index The index supplied by the caller.
 * @return The computed value requested by the caller.
 */
static guint maybe_apply_color_token(EditorTab *tab,
                                     const char *token,
                                     gint start_offset,
                                     gint end_offset,
                                     guint index) {
    char *background = NULL;
    char *foreground = NULL;
    gboolean ok = normalize_color_literal(token, &background, &foreground);
    if (ok) {
        apply_color_literal_tag(tab, start_offset, end_offset, background, foreground, index);
        index++;
    }
    g_free(background);
    g_free(foreground);
    return index;
}


/**
 * @brief Update color literals.
 * @details Color highlighting is useful only if it stays cheap. We scan bounded
 *          buffers for common CSS-style forms and paint just the literal token
 *          so theme/code previews do not need a separate color picker mode.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void update_color_literals(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    clear_color_literals(tab);
    if ((guint)gtk_text_buffer_get_char_count(tab->buffer) > GRAPTOS_COLOR_LITERAL_MAX_CHARS) return;

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
    if (!text) return;

    const char *p = text;
    gint offset = 0;
    guint tag_index = 0u;
    while (*p != '\0') {
        if (*p == '#' && color_token_boundary(p > text ? p[-1] : '\0')) {
            const char *token_start = p;
            const char *token_end = p + 1;
            guint digits = 0u;
            while (g_ascii_isxdigit(*token_end) && digits < 8u) {
                token_end++;
                digits++;
            }
            if (digits >= 3u && color_token_boundary(*token_end)) {
                char *token = g_strndup(token_start, (gsize)(token_end - token_start));
                tag_index = maybe_apply_color_token(tab, token, offset,
                                                    offset + (gint)(token_end - token_start),
                                                    tag_index);
                g_free(token);
                offset += (gint)(token_end - token_start);
                p = token_end;
                continue;
            }
        } else if (g_ascii_isalpha(*p) && color_token_boundary(p > text ? p[-1] : '\0')) {
            const char *token_start = p;
            const char *token_end = p + 1;
            while (g_ascii_isalnum(*token_end) || *token_end == '_' || *token_end == '-') token_end++;
            if (*token_end == '(' &&
                (g_ascii_strncasecmp(token_start, "rgb", 3) == 0 ||
                 g_ascii_strncasecmp(token_start, "rgba", 4) == 0 ||
                 g_ascii_strncasecmp(token_start, "hsl", 3) == 0 ||
                 g_ascii_strncasecmp(token_start, "hsla", 4) == 0)) {
                const char *func_end = token_end + 1;
                while (*func_end != '\0' && *func_end != ')' && *func_end != '\n') func_end++;
                if (*func_end == ')') token_end = func_end + 1;
            }
            if (color_token_boundary(*token_end)) {
                char *token = g_strndup(token_start, (gsize)(token_end - token_start));
                tag_index = maybe_apply_color_token(tab, token, offset,
                                                    offset + (gint)(token_end - token_start),
                                                    tag_index);
                g_free(token);
                offset += (gint)(token_end - token_start);
                p = token_end;
                continue;
            }
        }

        p = g_utf8_next_char(p);
        offset++;
    }

    g_free(text);
}


/**
 * @brief Color literal timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean color_literal_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->color_literal_timeout = 0u;
    update_color_literals(tab);
    return G_SOURCE_REMOVE;
}


/**
 * @brief Editor tab schedule color literals.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_color_literals(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    graptos_source_cancel(&tab->color_literal_timeout);
    if (!editor_tab_live_features_allowed(tab)) {
        tab->color_literal_timeout = 0u;
        return;
    }

    tab->color_literal_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                   GRAPTOS_COLOR_LITERAL_DELAY_MS,
                                                   color_literal_timeout_cb,
                                                   tab,
                                                   NULL);
}


/**
 * @brief Update minimap text.
 * @details The minimap is painted from the real buffer state instead of holding
 *          a second editable copy. We queue drawing and sync scrolling here so
 *          it tracks the editor without becoming another source of truth.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void update_minimap_text(EditorTab *tab) {
    if (!tab || !tab->minimap_view || tab->minimap_updating) return;
    if (!tab->win || !tab->win->minimap_enabled) return;
    tab->minimap_updating = TRUE;
    gtk_widget_queue_draw(tab->minimap_view);
    if (tab->scrolled) {
        GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(tab->scrolled));
        sync_minimap_scroll(tab, vadj);
    }
    tab->minimap_updating = FALSE;
}


/**
 * @brief Minimap line is comment.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean minimap_line_is_comment(const char *text) {
    if (!text) return FALSE;
    while (*text == ' ' || *text == '\t') text++;
    return g_str_has_prefix(text, "//") || g_str_has_prefix(text, "/*") ||
           g_str_has_prefix(text, "*") || g_str_has_prefix(text, "#");
}

/**
 * @brief Minimap line has string.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean minimap_line_has_string(const char *text) {
    return text && (strchr(text, '"') || strchr(text, '\''));
}

/**
 * @brief Minimap line is preprocessor.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean minimap_line_is_preprocessor(const char *text) {
    if (!text) return FALSE;
    while (*text == ' ' || *text == '\t') text++;
    return text[0] == '#';
}

/**
 * @brief Minimap set line colour.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param cr The cr supplied by the caller.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 */
static void minimap_set_line_colour(cairo_t *cr, const char *line) {
    if (minimap_line_is_preprocessor(line)) {
        cairo_set_source_rgb(cr, 0.77, 0.53, 0.78);
    } else if (minimap_line_is_comment(line)) {
        cairo_set_source_rgb(cr, 0.42, 0.60, 0.36);
    } else if (minimap_line_has_string(line)) {
        cairo_set_source_rgb(cr, 0.77, 0.54, 0.44);
    } else {
        cairo_set_source_rgb(cr, 0.52, 0.57, 0.66);
    }
}

/**
 * @brief On minimap draw.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param area The area supplied by the caller.
 * @param cr The cr supplied by the caller.
 * @param width The width supplied by the caller.
 * @param height The height supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_minimap_draw(GtkDrawingArea *area, cairo_t *cr, int width,
                     int height, gpointer user_data) {
    (void)area;
    EditorTab *tab = user_data;
    if (!tab || !tab->buffer || width <= 0 || height <= 0) return;

    cairo_set_source_rgb(cr, 0.075, 0.082, 0.094);
    cairo_rectangle(cr, 0.0, 0.0, (double)width, (double)height);
    cairo_fill(cr);

    gint line_count = gtk_text_buffer_get_line_count(tab->buffer);
    if (line_count <= 0) return;

    gint rows = height > 2 ? height - 2 : height;
    if (rows <= 0) return;

    for (gint y = 1; y < rows; y++) {
        gint line = (gint)(((gint64)y * line_count) / rows);
        if (line >= line_count) line = line_count - 1;

        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_iter_at_line(tab->buffer, &start, line);
        end = start;
        if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
        char *line_text = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
        gsize len = line_text ? strlen(line_text) : 0u;
        if (len == 0u) {
            g_free(line_text);
            continue;
        }

        minimap_set_line_colour(cr, line_text);
        double stroke_width = 8.0 + ((double)(len > 180u ? 180u : len) / 180.0) *
                                      ((double)width - 14.0);
        cairo_rectangle(cr, 4.0, (double)y, stroke_width, 1.0);
        cairo_fill(cr);
        g_free(line_text);
    }

    if (tab->text_view && gtk_widget_get_mapped(tab->text_view)) {
        GtkTextView *view = GTK_TEXT_VIEW(tab->text_view);
        GdkRectangle visible;
        gtk_text_view_get_visible_rect(view, &visible);
        GtkTextIter top;
        GtkTextIter bottom;
        gint ignored = 0;
        gtk_text_view_get_line_at_y(view, &top, visible.y, &ignored);
        gtk_text_view_get_line_at_y(view, &bottom, visible.y + visible.height, &ignored);
        double y1 = ((double)gtk_text_iter_get_line(&top) / (double)line_count) * (double)height;
        double y2 = ((double)(gtk_text_iter_get_line(&bottom) + 1) / (double)line_count) * (double)height;
        if (y2 < y1 + 8.0) y2 = y1 + 8.0;
        cairo_set_source_rgba(cr, 0.85, 0.88, 0.95, 0.22);
        cairo_rectangle(cr, 0.0, y1, (double)width, y2 - y1);
        cairo_fill(cr);
        cairo_set_source_rgba(cr, 0.85, 0.88, 0.95, 0.55);
        cairo_rectangle(cr, 0.5, y1 + 0.5, (double)width - 1.0, y2 - y1);
        cairo_stroke(cr);
    }
}


/**
 * @brief On minimap click.
 * @details The minimap click maps a vertical ratio back to a buffer line. That
 *          keeps the behavior independent of font size and lets the minimap act
 *          like a compact scrollbar rather than a fragile text widget.
 * @param gesture The gesture supplied by the caller.
 * @param n_press The n press supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_minimap_click(GtkGestureClick *gesture, int n_press, double x,
                      double y, gpointer user_data) {
    (void)n_press;
    (void)x;
    EditorTab *tab = user_data;
    GtkWidget *widget = gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture));

    if (!tab || !widget || widget != tab->minimap_view || !tab->buffer) return;
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    gint height = gtk_widget_get_height(widget);
    if (height <= 0) return;
    gint lines = gtk_text_buffer_get_line_count(tab->buffer);
    if (lines <= 0) return;
    if (y < 0.0) y = 0.0;
    if (y > (double)height) y = (double)height;
    guint target = (guint)(((double)lines * y) / (double)height) + 1u;
    editor_tab_jump_to_line_internal(tab, target);
}
