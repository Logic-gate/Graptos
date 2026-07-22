/**
 * @file src/editor_tab_build.c
 * @brief Graptoς editor tab build module.
 * @details Editor tabs hold the real editing surface. We split the implementation by
 *          lifecycle, input, rendering, preview, and transient UI because each part has
 *          different timing and cleanup pressure.
 */

#include "editor_tab_private.h"

/**
 * @brief Tab init state.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param win The win supplied by the caller.
 */
static void tab_init_state(EditorTab *tab, EditorWindow *win) {
    tab->win = win;
    tab->backup_enabled = win ? win->backup_enabled : TRUE;
    tab->wrap_enabled = FALSE;
    tab->tab_width = win && win->tab_width ? win->tab_width : 4u;
    tab->insert_spaces = win ? win->insert_spaces : TRUE;
    tab->autocomplete_enabled = win ? win->autocomplete_enabled : TRUE;
    tab->undo_stack = g_ptr_array_new_with_free_func(g_free);
    tab->redo_stack = g_ptr_array_new_with_free_func(g_free);
    tab->diagnostics = g_ptr_array_new_with_free_func(editor_diagnostic_free);
}

/**
 * @brief Tab create text area.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param win The win supplied by the caller.
 */
static void tab_create_text_area(EditorTab *tab, EditorWindow *win) {
    tab->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(tab->box, "graptos-tab-page");

    tab->editor_area = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(tab->editor_area, "graptos-editor-area");
    tab->editor_content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(tab->editor_content, "graptos-editor-content");
    tab->preview_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(tab->preview_paned, "graptos-preview-paned");
    tab->gutter = gtk_drawing_area_new();
    gtk_widget_add_css_class(tab->gutter, "graptos-gutter");
    tab->popover_parent = gtk_overlay_new();
    gtk_widget_add_css_class(tab->popover_parent, "graptos-editor-overlay");

    tab->scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(tab->scrolled, "graptos-editor-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab->scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    /*
     * GtkSourceView owns the live syntax engine.  Graptoς YAML rules now feed
     * style-scheme overrides only; they must not run regex tagging from the
     * buffer-changed path.
     */
    tab->source_buffer = gtk_source_buffer_new(NULL);
    gtk_source_buffer_set_highlight_matching_brackets(tab->source_buffer, TRUE);
    tab->buffer = GTK_TEXT_BUFFER(tab->source_buffer);
    tab->text_view = gtk_source_view_new_with_buffer(tab->source_buffer);
    gtk_widget_add_css_class(tab->text_view, "graptos-editor");
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tab->text_view),
                                win ? !win->use_system_interface_font : TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tab->text_view), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tab->text_view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tab->text_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tab->text_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tab->text_view), 8);
    gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(tab->text_view), 3);
    gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(tab->text_view), 2);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(tab->text_view), TRUE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(tab->text_view), TRUE);
    gtk_source_view_set_auto_indent(GTK_SOURCE_VIEW(tab->text_view), FALSE);
    gtk_source_view_set_indent_on_tab(GTK_SOURCE_VIEW(tab->text_view), FALSE);
    gtk_source_view_set_smart_backspace(GTK_SOURCE_VIEW(tab->text_view), TRUE);
}

/**
 * @brief Toggle the preview between attached and detached placement.
 * @details The preview is still owned by the tab either way. The button only
 *          changes the widget parent so live updates and scroll restoration use
 *          the same buffer and do not need a second renderer path.
 * @param button The button that emitted the signal.
 * @param user_data The editor tab that owns the preview widgets.
 */
static void tab_preview_detach_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    EditorTab *tab = user_data;
    if (!tab) return;
    graptos_source_cancel(&tab->preview_reparent_idle);
    tab->preview_reparent_idle =
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                        preview_reparent_idle_cb,
                        tab,
                        NULL);
}

/**
 * @brief Tab create preview.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param win The win supplied by the caller.
 */
static void tab_create_preview(EditorTab *tab, EditorWindow *win) {
    tab->preview_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(tab->preview_box, "graptos-preview-box");
    gtk_widget_set_size_request(tab->preview_box, 360, 1);
    gtk_widget_set_hexpand(tab->preview_box, FALSE);
    gtk_widget_set_vexpand(tab->preview_box, TRUE);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(header, "graptos-preview-header");
    gtk_widget_set_halign(header, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(header, TRUE);
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    tab->preview_detach_button = gtk_button_new_from_icon_name("window-new-symbolic");
    gtk_widget_add_css_class(tab->preview_detach_button, "graptos-flat-button");
    gtk_widget_add_css_class(tab->preview_detach_button, "graptos-preview-detach-button");
    gtk_widget_set_tooltip_text(tab->preview_detach_button,
                                "Detach preview");
    g_signal_connect(tab->preview_detach_button, "clicked",
                     G_CALLBACK(tab_preview_detach_clicked), tab);
    gtk_box_append(GTK_BOX(header), spacer);
    gtk_box_append(GTK_BOX(header), tab->preview_detach_button);
    gtk_box_append(GTK_BOX(tab->preview_box), header);

    tab->preview_buffer = gtk_text_buffer_new(NULL);
    tab->preview_view = gtk_text_view_new_with_buffer(tab->preview_buffer);
    gtk_widget_add_css_class(tab->preview_view, "graptos-preview");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tab->preview_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tab->preview_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tab->preview_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tab->preview_view), 14);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tab->preview_view), 14);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tab->preview_view), 14);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tab->preview_view), 14);
    gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(tab->preview_view), 3);
    gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(tab->preview_view), 2);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tab->preview_view), FALSE);

    tab->preview_scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(tab->preview_scrolled, "graptos-preview-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab->preview_scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(tab->preview_scrolled, 360, 1);
    gtk_widget_set_vexpand(tab->preview_scrolled, TRUE);
    gtk_widget_set_hexpand(tab->preview_scrolled, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab->preview_scrolled),
                                  tab->preview_view);
    gtk_box_append(GTK_BOX(tab->preview_box), tab->preview_scrolled);
    if (win && !win->preview_enabled) gtk_widget_set_visible(tab->preview_box, FALSE);
}

/**
 * @brief Create the read-only minimap view for a tab.
 * @details The minimap shares the main GtkSourceBuffer instead of mirroring
 *          text into a second buffer. This keeps scrolling previews accurate
 *          without duplicating syntax highlighting or copying large file
 *          contents on every edit.
 * @param tab The editor tab that will own the minimap widgets.
 * @param win The window whose preview settings control initial visibility.
 *
 * The minimap shares the main GtkSourceBuffer instead of mirroring text into a
 * second buffer.  This keeps scrolling previews accurate without duplicating
 * syntax highlighting or copying large file contents on every edit.
 */
static void tab_create_minimap(EditorTab *tab, EditorWindow *win) {
    tab->minimap_buffer = NULL;
    tab->minimap_view = gtk_source_view_new_with_buffer(tab->source_buffer);
    gtk_widget_add_css_class(tab->minimap_view, "graptos-minimap");
    gtk_widget_set_size_request(tab->minimap_view, 150, -1);
    gtk_widget_set_vexpand(tab->minimap_view, TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tab->minimap_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tab->minimap_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tab->minimap_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tab->minimap_view), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tab->minimap_view), 3);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tab->minimap_view), 3);
    gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(tab->minimap_view), 0);
    gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(tab->minimap_view), 0);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(tab->minimap_view), FALSE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(tab->minimap_view), FALSE);
    gtk_source_view_set_show_line_marks(GTK_SOURCE_VIEW(tab->minimap_view), FALSE);

    tab->minimap_scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(tab->minimap_scrolled, "graptos-minimap-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab->minimap_scrolled),
                                   GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);

    gtk_widget_set_size_request(tab->minimap_scrolled, 150, 1);
    gtk_widget_set_vexpand(tab->minimap_scrolled, TRUE);
    gtk_widget_set_hexpand(tab->minimap_scrolled, FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab->minimap_scrolled),
                                  tab->minimap_view);
    if (win && !win->minimap_enabled) gtk_widget_set_visible(tab->minimap_scrolled, FALSE);
}

/**
 * @brief Tab create completion popover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void tab_create_completion_popover(EditorTab *tab) {
    tab->completion_popover = gtk_popover_new();
    graptos_popover_attach(tab->completion_popover, tab->popover_parent);
    gtk_popover_set_position(GTK_POPOVER(tab->completion_popover), GTK_POS_BOTTOM);
    gtk_popover_set_has_arrow(GTK_POPOVER(tab->completion_popover), FALSE);
    gtk_popover_set_autohide(GTK_POPOVER(tab->completion_popover), FALSE);
    gtk_widget_set_can_focus(tab->completion_popover, FALSE);
    gtk_widget_add_css_class(tab->completion_popover,
                             "graptos-completion-popover");

    tab->completion_list = gtk_list_box_new();
    gtk_widget_set_can_focus(tab->completion_list, FALSE);
    gtk_widget_add_css_class(tab->completion_list, "graptos-completion-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(tab->completion_list),
                                    GTK_SELECTION_SINGLE);
    gtk_widget_set_size_request(tab->completion_list, 440, -1);

    tab->completion_scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(tab->completion_scrolled, "graptos-completion-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab->completion_scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_widget_set_size_request(tab->completion_scrolled, 460, 32);
    gtk_widget_set_hexpand(tab->completion_scrolled, FALSE);
    gtk_widget_set_vexpand(tab->completion_scrolled, FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab->completion_scrolled),
                                  tab->completion_list);
    gtk_popover_set_child(GTK_POPOVER(tab->completion_popover),
                          tab->completion_scrolled);
    g_signal_connect(tab->completion_list, "row-activated",
                     G_CALLBACK(completion_row_activated), tab);
    graptos_popover_hide(tab->completion_popover);
}

/**
 * @brief Tab create hover popover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void tab_create_hover_popover(EditorTab *tab) {
    tab->hover_popover = gtk_popover_new();
    graptos_popover_attach(tab->hover_popover, tab->popover_parent);
    gtk_popover_set_position(GTK_POPOVER(tab->hover_popover), GTK_POS_BOTTOM);
    gtk_popover_set_has_arrow(GTK_POPOVER(tab->hover_popover), FALSE);
    gtk_popover_set_autohide(GTK_POPOVER(tab->hover_popover), FALSE);
    gtk_widget_set_can_focus(tab->hover_popover, TRUE);
    gtk_widget_add_css_class(tab->hover_popover, "graptos-hover-popover");

    tab->hover_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(tab->hover_list),
                                    GTK_SELECTION_NONE);
    gtk_widget_add_css_class(tab->hover_list, "graptos-ref-list");

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    
    gtk_widget_set_size_request(scrolled, 460, 300);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  tab->hover_list);
    gtk_popover_set_child(GTK_POPOVER(tab->hover_popover), scrolled);
    g_signal_connect(tab->hover_list, "row-activated",
                     G_CALLBACK(hover_row_activated), tab);
    /*
     * The reference popover must stay open while the pointer moves from the
     * editor into the popover.  Track enter/leave on each child that can receive
     * pointer events so the hide timeout does not fire during that transition.
     */
    GtkEventController *popover_motion = gtk_event_controller_motion_new();
    g_signal_connect(popover_motion, "enter",
                     G_CALLBACK(on_hover_popover_enter), tab);
    g_signal_connect(popover_motion, "leave",
                     G_CALLBACK(on_hover_popover_leave), tab);
    gtk_widget_add_controller(tab->hover_popover, popover_motion);

    GtkEventController *scrolled_motion = gtk_event_controller_motion_new();
    g_signal_connect(scrolled_motion, "enter",
                     G_CALLBACK(on_hover_popover_enter), tab);
    g_signal_connect(scrolled_motion, "leave",
                     G_CALLBACK(on_hover_popover_leave), tab);
    gtk_widget_add_controller(scrolled, scrolled_motion);

    GtkEventController *list_motion = gtk_event_controller_motion_new();
    g_signal_connect(list_motion, "enter",
                     G_CALLBACK(on_hover_popover_enter), tab);
    g_signal_connect(list_motion, "leave",
                     G_CALLBACK(on_hover_popover_leave), tab);
    gtk_widget_add_controller(tab->hover_list, list_motion);
    graptos_popover_hide(tab->hover_popover);
}

/**
 * @brief Tab pack widgets.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void tab_pack_widgets(EditorTab *tab) {
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab->scrolled),
                                  tab->text_view);
    gtk_overlay_set_child(GTK_OVERLAY(tab->popover_parent), tab->scrolled);

    gtk_widget_set_visible(tab->gutter, FALSE);
    gtk_box_append(GTK_BOX(tab->editor_area), tab->gutter);
    gtk_widget_set_hexpand(tab->popover_parent, TRUE);
    gtk_widget_set_vexpand(tab->popover_parent, TRUE);
    gtk_box_append(GTK_BOX(tab->editor_content), tab->popover_parent);
    gtk_box_append(GTK_BOX(tab->editor_content), tab->minimap_scrolled);
    gtk_widget_set_hexpand(tab->editor_content, TRUE);
    gtk_widget_set_vexpand(tab->editor_content, TRUE);
    gtk_paned_set_start_child(GTK_PANED(tab->preview_paned), tab->editor_content);
    gtk_paned_set_end_child(GTK_PANED(tab->preview_paned), tab->preview_box);
    gtk_paned_set_resize_start_child(GTK_PANED(tab->preview_paned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(tab->preview_paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(tab->preview_paned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(tab->preview_paned), FALSE);
    gtk_widget_set_hexpand(tab->preview_paned, TRUE);
    gtk_widget_set_vexpand(tab->preview_paned, TRUE);
    gtk_box_append(GTK_BOX(tab->editor_area), tab->preview_paned);
    gtk_widget_set_vexpand(tab->editor_area, TRUE);
    gtk_box_append(GTK_BOX(tab->box), tab->editor_area);
}

/**
 * @brief Tab symbolic icon.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param primary The primary supplied by the caller.
 * @param fallbacks The fallbacks supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *tab_symbolic_icon(const char *primary, const char **fallbacks) {
    GtkWidget *icon = gtk_picture_new();
    gtk_widget_set_size_request(icon, 12, 12);
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
    gtk_picture_set_can_shrink(GTK_PICTURE(icon), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(icon), GTK_CONTENT_FIT_CONTAIN);

    GdkDisplay *display = gdk_display_get_default();
    GtkIconTheme *theme = display ? gtk_icon_theme_get_for_display(display) : NULL;
    GtkIconPaintable *paintable = theme
        ? gtk_icon_theme_lookup_icon(theme,
                                     primary ? primary : "",
                                     fallbacks,
                                     11,
                                     1,
                                     GTK_TEXT_DIR_NONE,
                                     GTK_ICON_LOOKUP_PRELOAD)
        : NULL;
    if (paintable) {
        gtk_picture_set_paintable(GTK_PICTURE(icon), GDK_PAINTABLE(paintable));
        g_object_unref(paintable);
    }

    return icon;
}


/**
 * @brief Handle a primary click on an editor tab label.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param gesture The gesture supplied by the caller.
 * @param n_press The n press supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void on_tab_label_pressed(GtkGestureClick *gesture,
                                 int n_press,
                                 double x,
                                 double y,
                                 gpointer user_data) {
    (void)n_press;
    (void)x;
    (void)y;
    EditorTab *tab = user_data;
    if (!tab || !tab->win) return;
    GdkModifierType state =
        gtk_event_controller_get_current_event_state(GTK_EVENT_CONTROLLER(gesture));
    app_window_handle_tab_label_click(tab->win, tab, state);
}

/**
 * @brief Tab create tab label.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void tab_create_tab_label(EditorTab *tab) {
    tab->tab_widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_size_request(tab->tab_widget, 150, -1);
    gtk_widget_add_css_class(tab->tab_widget, "graptos-tab");
    const char *lock_fallbacks[] = {
        "changes-prevent-symbolic",
        "system-lock-screen-symbolic",
        "channel-secure-symbolic",
        "security-high-symbolic",
        NULL
    };
    tab->tab_lock_icon = tab_symbolic_icon("object-locked-symbolic", lock_fallbacks);
    gtk_widget_set_margin_end(tab->tab_lock_icon, 1);
    gtk_widget_set_tooltip_text(tab->tab_lock_icon, "Locked");
    gtk_widget_set_visible(tab->tab_lock_icon, FALSE);
    tab->tab_title = gtk_label_new("Untitled");
    gtk_widget_set_margin_start(tab->tab_title, 8);
    gtk_widget_set_margin_end(tab->tab_title, 6);
    gtk_label_set_xalign(GTK_LABEL(tab->tab_title), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(tab->tab_title), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(tab->tab_title), 32);
    gtk_widget_set_size_request(tab->tab_title, 96, -1);

    GtkWidget *close_btn = graptos_flat_button_new("×",
                                                   "Close tab",
                                                   G_CALLBACK(on_close_button_clicked),
                                                   tab);
    gtk_widget_add_css_class(close_btn, "graptos-tab-close");
    gtk_box_append(GTK_BOX(tab->tab_widget), tab->tab_lock_icon);
    gtk_box_append(GTK_BOX(tab->tab_widget), tab->tab_title);
    gtk_box_append(GTK_BOX(tab->tab_widget), close_btn);
    GtkGesture *tab_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(tab_click),
                                  GDK_BUTTON_PRIMARY);
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(tab_click),
                                               GTK_PHASE_CAPTURE);
    g_signal_connect(tab_click, "pressed",
                     G_CALLBACK(on_tab_label_pressed), tab);
    gtk_widget_add_controller(tab->tab_widget,
                              GTK_EVENT_CONTROLLER(tab_click));
}

/**
 * @brief Connect editor buffer and widget signals.
 * @details Controllers are owned by the widgets after gtk_widget_add_controller().
 *          The EditorTab user_data remains valid until tab destruction, where
 *          pending timeouts and popovers are cancelled before the widgets are
 *          released.
 * @param tab The editor tab whose widgets should receive signal handlers.
 */
static void tab_connect_signals(EditorTab *tab) {
    g_signal_connect(tab->buffer, "changed", G_CALLBACK(on_buffer_changed), tab);
    g_signal_connect(tab->buffer, "mark-set", G_CALLBACK(on_mark_set), tab);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(tab->gutter),
                                   on_gutter_draw, tab, NULL);

    GtkAdjustment *vadj =
        gtk_scrolled_window_get_vadjustment(
            GTK_SCROLLED_WINDOW(tab->scrolled));
    if (vadj) {
        g_signal_connect(vadj, "value-changed",
                         G_CALLBACK(on_vadjustment_value_changed), tab);
    }

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "motion", G_CALLBACK(on_text_view_motion), tab);
    g_signal_connect(motion, "leave", G_CALLBACK(on_text_view_leave), tab);
    gtk_widget_add_controller(tab->text_view, motion);

    GtkGesture *right_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click),
                                  GDK_BUTTON_SECONDARY);
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(right_click), GTK_PHASE_CAPTURE);
    g_signal_connect(right_click, "pressed",
                     G_CALLBACK(on_text_view_right_click), tab);
    gtk_widget_add_controller(tab->text_view, GTK_EVENT_CONTROLLER(right_click));

    GtkGesture *minimap_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(minimap_click),
                                  GDK_BUTTON_PRIMARY);
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(minimap_click), GTK_PHASE_CAPTURE);
    g_signal_connect(minimap_click, "pressed", G_CALLBACK(on_minimap_click),
                     tab);
    gtk_widget_add_controller(tab->minimap_view,
                              GTK_EVENT_CONTROLLER(minimap_click));

    GtkEventController *keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed",
                     G_CALLBACK(on_text_view_key_pressed), tab);
    g_signal_connect(keys, "key-released",
                     G_CALLBACK(on_text_view_key_released), tab);
    gtk_widget_add_controller(tab->text_view, keys);
}

/**
 * @brief Editor tab new.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
EditorTab *editor_tab_new(EditorWindow *win) {
    EditorTab *tab = g_new0(EditorTab, 1);
    if (!tab) return NULL;

    tab_init_state(tab, win);
    tab_create_text_area(tab, win);
    tab_create_minimap(tab, win);
    tab_create_preview(tab, win);
    tab_create_completion_popover(tab);
    tab_create_hover_popover(tab);
    tab_pack_widgets(tab);
    tab_create_tab_label(tab);
    tab_connect_signals(tab);

    editor_tab_set_tab_policy(tab, tab->tab_width, tab->insert_spaces);
    editor_tab_update_highlight_engine(tab);
    reset_undo_state(tab);
    update_gutter_width(tab);
    update_minimap_text(tab);
    editor_tab_update_preview(tab);
    editor_tab_update_title(tab);
    return tab;
}
