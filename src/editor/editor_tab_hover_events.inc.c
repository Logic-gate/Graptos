/**
 * @file src/editor/editor_tab_hover_events.inc.c
 * @brief Graptoς editor tab hover events module.
 * @details Hover UI is mostly timing and placement. We keep it away from the core tab
 *          lifecycle because tiny focus changes can make popovers feel stuck, too eager, or
 *          impossible to use.
 */

/**
 * @brief Log regex tester debug messages.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param format The format supplied by the caller.
 */
static void regex_tester_debug(EditorTab *tab, const char *format, ...) {
    if (!tab || !tab->win || !tab->win->debug_mode || !format) return;

    va_list args;
    va_start(args, format);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
    char *message = g_strdup_vprintf(format, args);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    va_end(args);

    if (message) {
        g_message("Regex tester: %s", message);
        g_free(message);
    }
}

/**
 * @brief Render reference results in the hover popover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 * @param refs The refs supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean hover_render_references(EditorTab *tab,
                                        const char *word,
                                        GPtrArray *refs);

/**
 * @brief Pending hover LSP request identity.
 */
typedef struct {
    EditorTab *tab; /**< Tab that requested hover locations. */
    char *word; /**< Hover word at request time. */
    GPtrArray *definitions; /**< LSP definition locations. */
    GPtrArray *references; /**< LSP reference locations. */
    guint ref_count; /**< Shared callback reference count. */
    guint pending; /**< Pending LSP location responses. */
    guint serial; /**< Hover request serial. */
    gboolean rendered; /**< Whether LSP results rendered. */
} HoverLspRequest;

/**
 * @brief Reference hover LSP request state.
 */
static HoverLspRequest *hover_lsp_request_ref(HoverLspRequest *request) {
    if (request) request->ref_count++;
    return request;
}

/**
 * @brief Unreference hover LSP request state.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param request The request supplied by the caller.
 */
static void hover_lsp_request_unref(HoverLspRequest *request) {
    if (!request) return;
    if (request->ref_count > 1u) {
        request->ref_count--;
        return;
    }
    if (request->definitions) g_ptr_array_free(request->definitions, TRUE);
    if (request->references) g_ptr_array_free(request->references, TRUE);
    g_free(request->word);
    g_free(request);
}

/**
 * @brief Return whether a hover LSP response still matches the active hover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param request The request supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean hover_lsp_request_current(EditorTab *tab,
                                          HoverLspRequest *request) {
    return tab && request &&
        tab->hover_lsp_serial == request->serial &&
        tab->hover_word &&
        strcmp(tab->hover_word, request->word) == 0;
}

/**
 * @brief Render collected LSP definition and reference results.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param request The request supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean hover_lsp_request_render(EditorTab *tab,
                                         HoverLspRequest *request) {
    if (!hover_lsp_request_current(tab, request) || request->rendered) {
        return FALSE;
    }

    GPtrArray *combined = g_ptr_array_new();
    if (!combined) return FALSE;
    for (guint i = 0u; request->definitions && i < request->definitions->len; i++) {
        g_ptr_array_add(combined, g_ptr_array_index(request->definitions, i));
    }
    for (guint i = 0u; request->references && i < request->references->len; i++) {
        g_ptr_array_add(combined, g_ptr_array_index(request->references, i));
    }
    GPtrArray *graptos_refs = index_references_for_word(tab, request->word, 30u);
    for (guint i = 0u; graptos_refs && i < graptos_refs->len; i++) {
        g_ptr_array_add(combined, g_ptr_array_index(graptos_refs, i));
    }
    guint graptos_count = graptos_refs ? graptos_refs->len : 0u;

    gboolean rendered = combined->len > 0u &&
        hover_render_references(tab, request->word, combined);
    g_ptr_array_free(combined, FALSE);
    if (graptos_refs) g_ptr_array_free(graptos_refs, TRUE);
    if (rendered) {
        request->rendered = TRUE;
        graptos_source_cancel(&tab->hover_lsp_fallback_timeout);
        if (tab->win && tab->win->debug_mode) {
            g_message("LSP references UI: combined result used word=%s defs=%u refs=%u graptos=%u",
                      request->word,
                      request->definitions ? request->definitions->len : 0u,
                      request->references ? request->references->len : 0u,
                      graptos_count);
        }
    }
    return rendered;
}

/**
 * @brief Render reference results in the hover popover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 * @param refs The refs supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean hover_render_references(EditorTab *tab,
                                        const char *word,
                                        GPtrArray *refs) {
    if (!tab || !word || !refs || refs->len == 0u ||
        !tab->hover_popover || !tab->hover_list) {
        return FALSE;
    }

    tab->color_preview_valid = FALSE;
    tab->hover_pinned = FALSE;

    hover_clear_rows(tab);

    GtkWidget *header = gtk_list_box_row_new();
    gtk_widget_set_sensitive(header, FALSE);

    char *heading = g_strdup_printf("References to %s", word);
    GtkWidget *heading_label = gtk_label_new(heading);
    gtk_label_set_xalign(GTK_LABEL(heading_label), 0.0f);
    gtk_widget_set_margin_start(heading_label, 10);
    gtk_widget_set_margin_end(heading_label, 10);
    gtk_widget_set_margin_top(heading_label, 8);
    gtk_widget_set_margin_bottom(heading_label, 6);
    gtk_widget_add_css_class(heading_label, "graptos-ref-heading");

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(header), heading_label);
    gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list), header, -1);
    g_free(heading);

    const char *last_source = NULL;
    for (guint i = 0u; i < refs->len; i++) {
        IndexReference *ref = g_ptr_array_index(refs, i);
        const char *source = ref && ref->kind && g_str_has_prefix(ref->kind, "LSP")
            ? "LSP"
            : "Graptoς";
        if (g_strcmp0(source, last_source) != 0) {
            GtkWidget *source_row = gtk_list_box_row_new();
            gtk_widget_set_sensitive(source_row, FALSE);
            GtkWidget *source_label = gtk_label_new(source);
            gtk_label_set_xalign(GTK_LABEL(source_label), 0.0f);
            gtk_widget_set_margin_start(source_label, 10);
            gtk_widget_set_margin_end(source_label, 10);
            gtk_widget_set_margin_top(source_label, 4);
            gtk_widget_set_margin_bottom(source_label, 3);
            gtk_widget_add_css_class(source_label, "graptos-ref-heading");
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(source_row), source_label);
            gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list), source_row, -1);
            last_source = source;
        }
        gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list),
                            reference_row_new(ref),
                            -1);
    }

    if (!editor_tab_place_popover_at_cursor(tab, tab->hover_popover)) {
        if (tab->win && tab->win->debug_mode) {
            g_message("LSP references UI: popover placement failed word=%s refs=%u",
                      word,
                      refs ? refs->len : 0u);
        }
        return FALSE;
    }

    if (tab->win && tab->win->debug_mode) {
        g_message("LSP references UI: render word=%s refs=%u",
                  word,
                  refs ? refs->len : 0u);
    }
    tab->hover_popover_locked = TRUE;
    graptos_popover_show(tab->hover_popover);
    gtk_widget_grab_focus(tab->text_view);
    return TRUE;
}

/**
 * @brief Render index references as LSP fallback.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean hover_render_index_references(EditorTab *tab,
                                             const char *word) {
    GPtrArray *refs = index_references_for_word(tab, word, 30u);
    if (tab && tab->win && tab->win->debug_mode) {
        g_message("LSP references UI: index fallback word=%s refs=%u",
                  word ? word : "",
                  refs ? refs->len : 0u);
    }
    gboolean rendered = hover_render_references(tab, word, refs);
    if (refs) g_ptr_array_free(refs, TRUE);
    return rendered;
}

/**
 * @brief Handle LSP references response for hover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param locations The locations supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void hover_lsp_references_cb(EditorTab *tab,
                                    GPtrArray *locations,
                                    gpointer user_data) {
    HoverLspRequest *request = user_data;
    if (request && request->pending > 0u) request->pending--;
    if (hover_lsp_request_current(tab, request)) {
        request->references = locations;
        locations = NULL;
        if (request->pending == 0u) hover_lsp_request_render(tab, request);
    } else if (tab && tab->win && tab->win->debug_mode) {
        g_message("LSP references UI: LSP references ignored word=%s refs=%u current=%d",
                  request && request->word ? request->word : "",
                  locations ? locations->len : 0u,
                  hover_lsp_request_current(tab, request) ? 1 : 0);
    }
    if (locations) g_ptr_array_free(locations, TRUE);
    hover_lsp_request_unref(request);
}

/**
 * @brief Handle LSP definition response for hover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param locations The locations supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
/**
 * @brief Hover timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static void hover_lsp_definition_cb(EditorTab *tab,
                                    GPtrArray *locations,
                                    gpointer user_data) {
    HoverLspRequest *request = user_data;
    if (request && request->pending > 0u) request->pending--;
    if (hover_lsp_request_current(tab, request)) {
        request->definitions = locations;
        locations = NULL;
        if (request->pending == 0u) hover_lsp_request_render(tab, request);
    } else if (tab && tab->win && tab->win->debug_mode) {
        g_message("LSP references UI: LSP definition ignored word=%s defs=%u current=%d",
                  request && request->word ? request->word : "",
                  locations ? locations->len : 0u,
                  hover_lsp_request_current(tab, request) ? 1 : 0);
    }
    if (locations) g_ptr_array_free(locations, TRUE);
    hover_lsp_request_unref(request);
}

/**
 * @brief Fallback to index references if LSP does not answer quickly.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean hover_lsp_fallback_timeout_cb(gpointer user_data) {
    HoverLspRequest *request = user_data;
    EditorTab *tab = request ? request->tab : NULL;
    if (tab) tab->hover_lsp_fallback_timeout = 0u;
    if (hover_lsp_request_current(tab, request) && !request->rendered) {
        if (tab && tab->win && tab->win->debug_mode) {
            g_message("LSP references UI: fallback timeout word=%s",
                      request->word ? request->word : "");
        }
        hover_render_index_references(tab, request->word);
    }
    return G_SOURCE_REMOVE;
}

/**
 * @brief Resolve hover content after the pointer settles.
 * @details Hover work can involve index scans, LSP requests, color previews, or
 *          regex testers. Delaying the lookup prevents pointer motion from
 *          flooding the UI with popover rebuilds while the user is still moving.
 * @param user_data The editor tab passed through the timeout.
 * @return G_SOURCE_REMOVE after the scheduled hover work has run.
 */
gboolean hover_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;

    // Timeout: clear the stored source id.
    tab->hover_timeout = 0u;

    if (!editor_tab_reference_features_allowed(tab)) return G_SOURCE_REMOVE;
    if (!tab->hover_word || !tab->hover_popover || !tab->hover_list) return G_SOURCE_REMOVE;

    GtkTextIter cursor;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, mark);
    if (editor_tab_show_diagnostic_hover_at_iter(tab, &cursor)) {
        return G_SOURCE_REMOVE;
    }

    /*
     * Hex colours are handled before reference lookup.
     */
    GdkRGBA rgba;
    if (hex_to_rgba(tab->hover_word, &rgba)) {
        tab->color_preview_rgba = rgba;

        show_color_preview_in_hover(tab, tab->hover_word, &cursor);
        return G_SOURCE_REMOVE;
    }

    if (tab->win && tab->win->lsp_client && tab->file_path &&
        tab->hover_lsp_position_valid) {
        tab->hover_lsp_serial++;
        HoverLspRequest *request = g_new0(HoverLspRequest, 1);
        if (request) {
            request->tab = tab;
            request->word = g_strdup(tab->hover_word);
            request->serial = tab->hover_lsp_serial;
            request->ref_count = 1u;

            request->pending++;
            if (!lsp_client_request_definition(tab->win->lsp_client,
                                               tab,
                                               tab->hover_lsp_line,
                                               tab->hover_lsp_character,
                                               hover_lsp_definition_cb,
                                               hover_lsp_request_ref(request))) {
                request->pending--;
                hover_lsp_request_unref(request);
            }

            request->pending++;
            if (!lsp_client_request_references(tab->win->lsp_client,
                                               tab,
                                               tab->hover_lsp_line,
                                               tab->hover_lsp_character,
                                               30u,
                                               hover_lsp_references_cb,
                                               hover_lsp_request_ref(request))) {
                request->pending--;
                hover_lsp_request_unref(request);
            }

            if (request->pending > 0u) {
                tab->hover_lsp_fallback_timeout =
                    g_timeout_add_full(G_PRIORITY_LOW,
                                       350u,
                                       hover_lsp_fallback_timeout_cb,
                                       hover_lsp_request_ref(request),
                                       (GDestroyNotify)hover_lsp_request_unref);
                hover_lsp_request_unref(request);
                return G_SOURCE_REMOVE;
            }
        }
        hover_lsp_request_unref(request);
    }

    if (!hover_render_index_references(tab, tab->hover_word)) {
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_REMOVE;
}


/**
 * @brief On text view motion.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
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

    GtkTextIter iter;
    int buffer_x = 0;
    int buffer_y = 0;
    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
                                          GTK_TEXT_WINDOW_WIDGET,
                                          (int)x,
                                          (int)y,
                                          &buffer_x,
                                          &buffer_y);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget), &iter,
                                       buffer_x, buffer_y);

    if (editor_tab_show_diagnostic_hover_at_iter(tab, &iter)) {
        return;
    }

    if (tab->hover_popover && gtk_widget_get_visible(tab->hover_popover) &&
        !tab->hover_pointer_inside && !tab->inspect_reference_active) {
        hide_hover_preview(tab);
        return;
    }

    /*
     * Reference inspection is opt-in. Outside that mode, motion only hides stale
     * previews when the pointer is not inside the hover.
     */
    if (!tab->inspect_reference_active) {
        if (!tab->regex_tester_active && !tab->hover_pointer_inside) {
            hide_hover_preview(tab);
        }
        return;
    }

    cancel_hover_hide(tab);

    // Actual lookup is delayed so normal mouse movement does not spam indexing.
    schedule_reference_lookup_at_iter(tab, &iter);
}


/**
 * @brief On text view leave.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
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
    if (tab->regex_tester_active) {
        return;
    }

    if (tab->inspect_reference_active && tab->hover_popover &&
        gtk_widget_get_visible(tab->hover_popover)) {
        schedule_hover_transition_hide(tab);
    } else if (!tab->hover_pointer_inside) {
        hide_hover_preview(tab);
    }
}


/**
 * @brief On hover popover enter.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
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
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_hover_popover_leave(GtkEventControllerMotion *controller,
                            gpointer user_data) {
    (void)controller;

    EditorTab *tab = user_data;
    if (!tab) return;

    // Once the pointer leaves the popover, normal hover cleanup can happen.
    tab->hover_pointer_inside = FALSE;
    if (tab->regex_tester_active) {
        cancel_hover_hide(tab);
        return;
    }
    hide_hover_preview(tab);
}


/**
 * @brief Color preview row new.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param hex The hex supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
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
    gtk_widget_add_css_class(label, "graptos-color-label");

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
 * @brief Estimate a hover popover width before allocation.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param popover The popover supplied by the caller.
 * @return The computed value requested by the caller.
 */
static gint hover_popover_estimated_width(GtkWidget *popover) {
    if (!popover) return 460;
    gint width = gtk_widget_get_width(popover);
    if (width > 1) return width;
    GtkWidget *child = GTK_IS_POPOVER(popover)
        ? gtk_popover_get_child(GTK_POPOVER(popover)) : NULL;
    if (child) {
        width = gtk_widget_get_width(child);
        if (width > 1) return width;
    }
    return 460;
}

/**
 * @brief Place a hover popover below an arbitrary text iterator.
 * @details References, diagnostics, color previews, and regex previews all need
 *          the same cursor-relative placement. Routing that geometry through
 *          one helper keeps the top-left corner tied to the text location even
 *          when the view scrolls or the file is large.
 * @param tab The editor tab that owns the text view.
 * @param iter The text iterator that anchors the popover.
 * @param popover The hover popover being moved.
 * @return TRUE when the popover was positioned; otherwise FALSE.
 */
static gboolean place_hover_popover_at_iter(EditorTab *tab,
                                            GtkTextIter *iter,
                                            GtkWidget *popover) {
    if (!tab || !iter || !popover || !GTK_IS_POPOVER(popover)) return FALSE;

    GdkRectangle rect;
    gtk_text_view_get_iter_location(GTK_TEXT_VIEW(tab->text_view), iter, &rect);
    rect.y += rect.height;
    rect.width = 1;
    if (rect.height < 1) rect.height = 1;

    int window_x = 0;
    int window_y = 0;
    gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(tab->text_view),
                                          GTK_TEXT_WINDOW_WIDGET,
                                          rect.x,
                                          rect.y,
                                          &window_x,
                                          &window_y);
    rect.x = window_x;
    rect.y = window_y;

    if (!editor_tab_text_rect_to_popover_parent(tab, &rect)) return FALSE;

    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
    gtk_popover_set_offset(GTK_POPOVER(popover),
                           hover_popover_estimated_width(popover) / 2,
                           0);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    return TRUE;
}

/**
 * @brief Create a diagnostic hover row.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param message The message supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *diagnostic_hover_row_new(const char *message) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_widget_set_sensitive(row, FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 10);

    GtkWidget *title = gtk_label_new("Warning");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "graptos-ref-heading");

    GtkWidget *body = gtk_label_new(message ? message : "");
    gtk_label_set_xalign(GTK_LABEL(body), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(body), TRUE);
    gtk_widget_add_css_class(body, "graptos-ref-snippet");

    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), body);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    return row;
}

/**
 * @brief Show a diagnostic hover at a text iterator.
 * @details Diagnostic hovers share the same hover list as references, so this
 *          path clears stale rows first and then renders only the message tied
 *          to the current diagnostic range.
 * @param tab The editor tab that owns the hover popover.
 * @param iter The iterator under the pointer or cursor.
 * @return TRUE when a diagnostic hover was shown; otherwise FALSE.
 */
gboolean editor_tab_show_diagnostic_hover_at_iter(EditorTab *tab, GtkTextIter *iter) {
    if (!tab || !tab->hover_popover || !tab->hover_list || !iter) return FALSE;

    EditorDiagnostic *diagnostic = editor_tab_diagnostic_at_iter(tab, iter);
    if (!diagnostic || !diagnostic->message) return FALSE;

    hover_clear_rows(tab);
    cancel_hover_hide(tab);
    tab->color_preview_valid = FALSE;
    tab->regex_tester_active = FALSE;
    tab->hover_pinned = FALSE;
    tab->hover_popover_locked = TRUE;
    g_clear_pointer(&tab->hover_word, g_free);

    gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list),
                        diagnostic_hover_row_new(diagnostic->message),
                        -1);

    if (!place_hover_popover_at_iter(tab, iter, tab->hover_popover)) return FALSE;
    graptos_popover_show(tab->hover_popover);
    return TRUE;
}


/**
 * @brief Show color preview in hover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param hex The hex supplied by the caller.
 * @param where The where supplied by the caller.
 */
void show_color_preview_in_hover(EditorTab *tab,
                                 const char *hex,
                                 GtkTextIter *where) {
    if (!tab || !tab->hover_popover || !tab->hover_list || !hex || !where) return;

    // Colour preview reuses the hover popover but replaces reference rows.
    hover_clear_rows(tab);
    tab->regex_tester_active = FALSE;

    GtkWidget *header = gtk_list_box_row_new();
    gtk_widget_set_sensitive(header, FALSE);

    GtkWidget *heading = gtk_label_new("Colour preview");
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);
    gtk_widget_set_margin_start(heading, 10);
    gtk_widget_set_margin_end(heading, 10);
    gtk_widget_set_margin_top(heading, 8);
    gtk_widget_set_margin_bottom(heading, 4);
    gtk_widget_add_css_class(heading, "graptos-ref-heading");

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(header), heading);
    gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list), header, -1);
    gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list),
                        color_preview_row_new(tab, hex),
                        -1);

    tab->color_preview_valid = TRUE;
    tab->hover_pinned = FALSE;
    tab->hover_popover_locked = TRUE;

    if (!editor_tab_place_popover_at_cursor(tab, tab->hover_popover)) return;
    graptos_popover_show(tab->hover_popover);

    // Keep typing focus in the editor after showing the preview.
    gtk_widget_grab_focus(tab->text_view);
}


/**
 * @brief Check whether a byte position follows an escaped regex token.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @param index The index supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean regex_prev_is_escape(const char *text, gsize index) {
    if (!text || index < 2u) return FALSE;
    return text[index - 2u] == '\\' && text[index - 1u] != '\\';
}

/**
 * @brief Check whether a counted quantifier follows a regex-like atom.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @param index The index supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean regex_quantifier_has_regex_atom(const char *text, gsize index) {
    if (!text || index == 0u) return FALSE;
    char prev = text[index - 1u];
    return prev == ')' || prev == ']' || prev == '.' ||
        regex_prev_is_escape(text, index);
}

/**
 * @brief Check whether a counted repetition block looks like `{1}` or `{1,3}`.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @param index The index supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean regex_has_counted_repetition(const char *text, gsize index) {
    if (!text || text[index] != '{') return FALSE;

    gboolean has_digit = FALSE;
    gboolean has_end = FALSE;
    for (gsize i = index + 1u; text[i] != '\0'; i++) {
        if (g_ascii_isdigit(text[i])) {
            has_digit = TRUE;
            continue;
        }
        if (text[i] == ',') continue;
        if (text[i] == '}') {
            has_end = TRUE;
            break;
        }
        return FALSE;
    }

    return has_digit && has_end;
}

/**
 * @brief Check whether selected text has enough regex structure to show tester.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean regex_selection_has_meta(const char *text) {
    if (!text) return FALSE;

    int score = 0;
    gboolean has_open_class = FALSE;
    gboolean has_open_group = FALSE;

    for (gsize i = 0u; text[i] != '\0'; i++) {
        char ch = text[i];

        if (ch == '\\' && text[i + 1u] != '\0') {
            score += 2;
            i++;
            continue;
        }

        if (ch == '[') {
            has_open_class = TRUE;
            continue;
        }
        if (ch == ']' && has_open_class) {
            score += 2;
            has_open_class = FALSE;
            continue;
        }

        if (ch == '(') {
            has_open_group = TRUE;
            if (text[i + 1u] == '?') score += 2;
            continue;
        }
        if (ch == ')' && has_open_group) {
            score++;
            has_open_group = FALSE;
            continue;
        }

        if (ch == '^' && (i == 0u || text[i - 1u] == '(' || text[i - 1u] == '|')) {
            score++;
            continue;
        }
        if (ch == '$' && (text[i + 1u] == '\0' || text[i + 1u] == ')' ||
                          text[i + 1u] == '|')) {
            score++;
            continue;
        }

        if (ch == '|') {
            score++;
            continue;
        }

        if ((ch == '*' || ch == '+' || ch == '?') &&
            regex_quantifier_has_regex_atom(text, i)) {
            score++;
            continue;
        }

        if (ch == '{' && regex_quantifier_has_regex_atom(text, i) &&
            regex_has_counted_repetition(text, i)) {
            score += 2;
        }
    }

    return score >= 2;
}


/**
 * @brief Compile regex pattern for tester.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param pattern The pattern supplied by the caller.
 * @param error Return location for an optional error; left untouched on success unless GTK sets it.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GRegex *regex_tester_compile(const char *pattern, GError **error) {
    if (!pattern || pattern[0] == '\0') return NULL;
    return g_regex_new(pattern,
                       G_REGEX_MULTILINE | G_REGEX_OPTIMIZE,
                       0,
                       error);
}


/**
 * @brief Check if selected text is usable as a regex.
 * @details A random selection can compile as a regex, but that does not mean
 *          the user wanted the tester. We require metacharacters, a sane length,
 *          one line, and a successful compile before opening UI on top of text.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param selected The selected supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean selected_text_is_regex(EditorTab *tab, const char *selected) {
    if (!selected) {
        regex_tester_debug(tab, "reject: selected text is NULL");
        return FALSE;
    }

    char *copy = g_strdup(selected);
    if (!copy) {
        regex_tester_debug(tab, "reject: could not copy selection");
        return FALSE;
    }
    g_strstrip(copy);

    gboolean ok = FALSE;
    gsize byte_len = strlen(copy);
    glong char_len = g_utf8_strlen(copy, -1);
    regex_tester_debug(tab, "selection candidate bytes=%zu chars=%ld text=\"%s\"",
                       byte_len,
                       char_len,
                       copy);

    if (byte_len == 0u) {
        regex_tester_debug(tab, "reject: empty selection");
    } else if (char_len < 3) {
        regex_tester_debug(tab, "reject: selection shorter than 3 chars");
    } else if (char_len > 240) {
        regex_tester_debug(tab, "reject: selection longer than 240 chars");
    } else if (strchr(copy, '\n') || strchr(copy, '\r')) {
        regex_tester_debug(tab, "reject: selection spans multiple lines");
    } else if (!regex_selection_has_meta(copy)) {
        regex_tester_debug(tab, "reject: no regex metacharacters found");
    } else {
        GError *error = NULL;
        GRegex *regex = regex_tester_compile(copy, &error);
        ok = regex != NULL;
        if (!ok) {
            regex_tester_debug(tab, "reject: compile failed: %s",
                               error ? error->message : "unknown error");
        }
        if (regex) g_regex_unref(regex);
        if (error) g_error_free(error);
    }

    regex_tester_debug(tab, "decision: %s", ok ? "show tester" : "do not show tester");
    g_free(copy);
    return ok;
}


/**
 * @brief Ensure regex tester match tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 */
static void ensure_regex_tester_tag(GtkTextBuffer *buffer) {
    if (!buffer) return;
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    if (gtk_text_tag_table_lookup(table, "graptos-regex-match")) return;
    gtk_text_buffer_create_tag(buffer, "graptos-regex-match",
                               "background", "#515c7a",
                               "foreground", "#ffffff",
                               "underline", PANGO_UNDERLINE_SINGLE,
                               NULL);
}


/**
 * @brief Update regex tester matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 */
static void regex_tester_update(GtkTextBuffer *buffer) {
    if (!buffer) return;
    const char *pattern = g_object_get_data(G_OBJECT(buffer),
                                            "graptos-regex-pattern");
    GtkWidget *status = g_object_get_data(G_OBJECT(buffer),
                                          "graptos-regex-status");
    ensure_regex_tester_tag(buffer);

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    gtk_text_buffer_remove_tag_by_name(buffer, "graptos-regex-match", &start, &end);

    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    if (!text) return;

    GError *error = NULL;
    GRegex *regex = regex_tester_compile(pattern, &error);
    if (!regex) {
        if (status) {
            gtk_label_set_text(GTK_LABEL(status),
                               error ? error->message : "Invalid regex");
        }
        if (error) g_error_free(error);
        g_free(text);
        return;
    }

    GMatchInfo *match_info = NULL;
    g_regex_match(regex, text, 0, &match_info);
    guint matches = 0u;
    while (match_info && g_match_info_matches(match_info)) {
        gint start_pos = -1;
        gint end_pos = -1;
        if (!g_match_info_fetch_pos(match_info, 0, &start_pos, &end_pos)) break;
        if (start_pos >= 0 && end_pos > start_pos) {
            gint start_offset = (gint)g_utf8_pointer_to_offset(text,
                                                               text + start_pos);
            gint end_offset = (gint)g_utf8_pointer_to_offset(text,
                                                             text + end_pos);
            GtkTextIter match_start;
            GtkTextIter match_end;
            gtk_text_buffer_get_iter_at_offset(buffer, &match_start, start_offset);
            gtk_text_buffer_get_iter_at_offset(buffer, &match_end, end_offset);
            gtk_text_buffer_apply_tag_by_name(buffer,
                                              "graptos-regex-match",
                                              &match_start,
                                              &match_end);
            matches++;
        }
        if (matches >= 500u) break;
        if (!g_match_info_next(match_info, NULL)) break;
    }

    if (status) {
        char *message = matches == 1u
            ? g_strdup("1 match")
            : g_strdup_printf("%u matches", matches);
        gtk_label_set_text(GTK_LABEL(status), message);
        g_free(message);
    }

    if (match_info) g_match_info_free(match_info);
    g_regex_unref(regex);
    g_free(text);
}


/**
 * @brief Regex tester text changed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param user_data The callback context passed through GTK signal data.
 */
static void regex_tester_text_changed(GtkTextBuffer *buffer, gpointer user_data) {
    (void)user_data;
    regex_tester_update(buffer);
}


/**
 * @brief Create regex tester row.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param pattern The pattern supplied by the caller.
 * @param test_view_out Output storage filled when the operation can provide a value.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *regex_tester_row_new(const char *pattern,
                                       GtkWidget **test_view_out) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(row), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(row), FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 8);
    gtk_widget_set_margin_bottom(box, 10);

    GtkWidget *pattern_label = gtk_label_new(pattern ? pattern : "");
    gtk_label_set_xalign(GTK_LABEL(pattern_label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(pattern_label), TRUE);
    gtk_widget_add_css_class(pattern_label, "graptos-ref-snippet");

    GtkWidget *hint = gtk_label_new("Type sample text below. Matches are highlighted.");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);
    gtk_widget_add_css_class(hint, "graptos-ref-kind");

    GtkWidget *view = gtk_text_view_new();
    gtk_widget_add_css_class(view, "graptos-regex-tester-input");
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(view), 6);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(view), 6);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(view), 6);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(view), 6);
    gtk_widget_set_size_request(view, 420, 90);
    gtk_widget_set_sensitive(view, TRUE);

    GtkWidget *status = gtk_label_new("0 matches");
    gtk_label_set_xalign(GTK_LABEL(status), 0.0f);
    gtk_widget_add_css_class(status, "graptos-ref-kind");

    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    g_object_set_data_full(G_OBJECT(buffer),
                           "graptos-regex-pattern",
                           g_strdup(pattern ? pattern : ""),
                           g_free);
    g_object_set_data(G_OBJECT(buffer), "graptos-regex-status", status);
    g_signal_connect(buffer, "changed",
                     G_CALLBACK(regex_tester_text_changed), NULL);
    regex_tester_update(buffer);

    if (test_view_out) *test_view_out = view;

    gtk_box_append(GTK_BOX(box), pattern_label);
    gtk_box_append(GTK_BOX(box), hint);
    gtk_box_append(GTK_BOX(box), view);
    gtk_box_append(GTK_BOX(box), status);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    return row;
}


/**
 * @brief Show regex tester in hover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param pattern The pattern supplied by the caller.
 * @param where The where supplied by the caller.
 */
static void show_regex_tester_in_hover(EditorTab *tab,
                                       const char *pattern,
                                       GtkTextIter *where) {
    if (!tab || !tab->hover_popover || !tab->hover_list || !pattern || !where) {
        regex_tester_debug(tab, "show aborted: missing widget or pattern");
        return;
    }

    hover_clear_rows(tab);
    cancel_hover_hide(tab);
    tab->color_preview_valid = FALSE;
    tab->regex_tester_active = TRUE;
    tab->hover_popover_locked = TRUE;
    tab->hover_pointer_inside = TRUE;
    g_clear_pointer(&tab->hover_word, g_free);

    GtkWidget *header = gtk_list_box_row_new();
    gtk_widget_set_sensitive(header, FALSE);

    GtkWidget *heading = gtk_label_new("Regex tester");
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);
    gtk_widget_set_margin_start(heading, 10);
    gtk_widget_set_margin_end(heading, 10);
    gtk_widget_set_margin_top(heading, 8);
    gtk_widget_set_margin_bottom(heading, 4);
    gtk_widget_add_css_class(heading, "graptos-ref-heading");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(header), heading);
    gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list), header, -1);
    GtkWidget *test_view = NULL;
    gtk_list_box_insert(GTK_LIST_BOX(tab->hover_list),
                        regex_tester_row_new(pattern, &test_view),
                        -1);

    (void)where;
    if (!editor_tab_place_popover_at_cursor(tab, tab->hover_popover)) {
        regex_tester_debug(tab, "show aborted: could not place popover at cursor");
        return;
    }
    graptos_popover_show(tab->hover_popover);
    (void)editor_tab_place_popover_at_cursor(tab, tab->hover_popover);
    if (test_view) gtk_widget_grab_focus(test_view);
}


/**
 * @brief Maybe show regex tester.
 * @details The tester shares the hover popover with references, diagnostics,
 *          and color previews. We only take over that popover when the current
 *          selection still looks like a real regex at the time the timeout fires.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean maybe_show_regex_tester(EditorTab *tab) {
    if (!tab || tab->disposing || !tab->buffer || !tab->hover_popover || !tab->hover_list) {
        regex_tester_debug(tab, "skip: missing tab buffer or hover widgets");
        return FALSE;
    }

    GtkTextIter start;
    GtkTextIter end;
    if (!gtk_text_buffer_get_selection_bounds(tab->buffer, &start, &end)) {
        regex_tester_debug(tab, "skip: no active selection");
        if (tab->regex_tester_active) hide_hover_preview(tab);
        return FALSE;
    }

    char *selected = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
    if (!selected) {
        regex_tester_debug(tab, "skip: could not read selected text");
        if (tab->regex_tester_active) hide_hover_preview(tab);
        return FALSE;
    }

    g_strstrip(selected);
    if (!selected_text_is_regex(tab, selected)) {
        if (tab->regex_tester_active) hide_hover_preview(tab);
        g_free(selected);
        return FALSE;
    }

    show_regex_tester_in_hover(tab, selected, &end);
    g_free(selected);
    return TRUE;
}


/**
 * @brief Regex tester timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean regex_tester_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab || tab->disposing) return G_SOURCE_REMOVE;
    tab->regex_tester_timeout = 0u;
    regex_tester_debug(tab, "timeout fired");

    if (!maybe_show_regex_tester(tab)) {
        regex_tester_debug(tab, "not showing; falling back to selection helpers");
        maybe_show_color_preview(tab);
        editor_tab_schedule_selection_matches(tab);
    }

    return G_SOURCE_REMOVE;
}


/**
 * @brief Editor tab schedule regex tester.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_regex_tester(EditorTab *tab) {
    if (!tab || tab->disposing || !tab->buffer) return;
    if (tab->win && !tab->win->regex_tester_enabled) {
        graptos_source_cancel(&tab->regex_tester_timeout);
        if (tab->regex_tester_active) hide_hover_preview(tab);
        return;
    }
    if (!gtk_text_buffer_get_has_selection(tab->buffer)) {
        regex_tester_debug(tab, "schedule skipped: no selection");
        graptos_source_cancel(&tab->regex_tester_timeout);
        if (tab->regex_tester_active) hide_hover_preview(tab);
        return;
    }
    regex_tester_debug(tab, "schedule requested");
    graptos_source_cancel(&tab->regex_tester_timeout);

    tab->regex_tester_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                   140u,
                                                   regex_tester_timeout_cb,
                                                   tab,
                                                   NULL);
}


/**
 * @brief Maybe show color preview.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
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
                g_free(tab->hover_word);
                tab->hover_word = g_strdup(selected);

                graptos_source_cancel(&tab->hover_timeout);
                tab->hover_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                        GRAPTOS_HOVER_DELAY_MS,
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
