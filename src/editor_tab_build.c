#include "editor_tab_private.h"

static void tab_init_state(EditorTab *tab, EditorWindow *win) {
    tab->win = win;
    tab->backup_enabled = win ? win->backup_enabled : TRUE;
    tab->wrap_enabled = FALSE;
    tab->tab_width = win && win->tab_width ? win->tab_width : 4u;
    tab->insert_spaces = win ? win->insert_spaces : TRUE;
    tab->autocomplete_enabled = win ? win->autocomplete_enabled : TRUE;
    tab->undo_stack = g_ptr_array_new_with_free_func(g_free);
    tab->redo_stack = g_ptr_array_new_with_free_func(g_free);
}

static void tab_create_text_area(EditorTab *tab, EditorWindow *win) {
    tab->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(tab->box, "cleaf-tab-page");

    tab->editor_area = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(tab->editor_area, "cleaf-editor-area");
    tab->gutter = gtk_drawing_area_new();
    gtk_widget_add_css_class(tab->gutter, "cleaf-gutter");
    tab->popover_parent = gtk_overlay_new();
    gtk_widget_add_css_class(tab->popover_parent, "cleaf-editor-overlay");

    tab->scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(tab->scrolled, "cleaf-editor-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab->scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    /*
     * GtkSourceView owns the live syntax engine.  Cleaf YAML rules now feed
     * style-scheme overrides only; they must not run regex tagging from the
     * buffer-changed path.
     */
    tab->source_buffer = gtk_source_buffer_new(NULL);
    gtk_source_buffer_set_highlight_matching_brackets(tab->source_buffer, TRUE);
    tab->buffer = GTK_TEXT_BUFFER(tab->source_buffer);
    tab->text_view = gtk_source_view_new_with_buffer(tab->source_buffer);
    gtk_widget_add_css_class(tab->text_view, "cleaf-editor");
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tab->text_view),
                                win ? !win->use_system_interface_font : TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tab->text_view), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tab->text_view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tab->text_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tab->text_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tab->text_view), 8);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(tab->text_view), TRUE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(tab->text_view), TRUE);
    gtk_source_view_set_auto_indent(GTK_SOURCE_VIEW(tab->text_view), FALSE);
    gtk_source_view_set_indent_on_tab(GTK_SOURCE_VIEW(tab->text_view), FALSE);
    gtk_source_view_set_smart_backspace(GTK_SOURCE_VIEW(tab->text_view), TRUE);
}


static void tab_create_preview(EditorTab *tab, EditorWindow *win) {
    tab->preview_buffer = gtk_text_buffer_new(NULL);
    tab->preview_view = gtk_text_view_new_with_buffer(tab->preview_buffer);
    gtk_widget_add_css_class(tab->preview_view, "cleaf-preview");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tab->preview_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tab->preview_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tab->preview_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tab->preview_view), 14);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tab->preview_view), 14);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tab->preview_view), 14);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tab->preview_view), 14);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tab->preview_view), FALSE);

    tab->preview_scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(tab->preview_scrolled, "cleaf-preview-scroll");
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab->preview_scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(tab->preview_scrolled, 360, 1);
    gtk_widget_set_vexpand(tab->preview_scrolled, TRUE);
    gtk_widget_set_hexpand(tab->preview_scrolled, FALSE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab->preview_scrolled),
                                  tab->preview_view);
    if (win && !win->preview_enabled) {
        gtk_widget_set_visible(tab->preview_scrolled, FALSE);
    }
}

/*
 * The minimap shares the main GtkSourceBuffer instead of mirroring text into a
 * second buffer.  This keeps scrolling previews accurate without duplicating
 * syntax highlighting or copying large file contents on every edit.
 */
static void tab_create_minimap(EditorTab *tab, EditorWindow *win) {
    tab->minimap_buffer = NULL;
    tab->minimap_view = gtk_source_view_new_with_buffer(tab->source_buffer);
    gtk_widget_add_css_class(tab->minimap_view, "cleaf-minimap");
    gtk_widget_set_size_request(tab->minimap_view, 150, -1);
    gtk_widget_set_vexpand(tab->minimap_view, TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tab->minimap_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tab->minimap_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tab->minimap_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tab->minimap_view), GTK_WRAP_NONE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tab->minimap_view), 3);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tab->minimap_view), 3);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(tab->minimap_view), FALSE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(tab->minimap_view), FALSE);
    gtk_source_view_set_show_line_marks(GTK_SOURCE_VIEW(tab->minimap_view), FALSE);

    tab->minimap_scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(tab->minimap_scrolled, "cleaf-minimap-scroll");
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

static void tab_create_completion_popover(EditorTab *tab) {
    tab->completion_popover = gtk_popover_new();
    cleaf_popover_attach(tab->completion_popover, tab->popover_parent);
    gtk_popover_set_position(GTK_POPOVER(tab->completion_popover), GTK_POS_BOTTOM);
    gtk_popover_set_autohide(GTK_POPOVER(tab->completion_popover), FALSE);
    gtk_widget_set_can_focus(tab->completion_popover, FALSE);
    gtk_widget_add_css_class(tab->completion_popover,
                             "cleaf-completion-popover");

    tab->completion_list = gtk_list_box_new();
    gtk_widget_set_can_focus(tab->completion_list, FALSE);
    gtk_widget_add_css_class(tab->completion_list, "cleaf-completion-list");
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(tab->completion_list),
                                    GTK_SELECTION_SINGLE);
    gtk_widget_set_size_request(tab->completion_list, 440, -1);

    tab->completion_scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(tab->completion_scrolled, "cleaf-completion-scroll");
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
    cleaf_popover_hide(tab->completion_popover);
}

static void tab_create_hover_popover(EditorTab *tab) {
    tab->hover_popover = gtk_popover_new();
    cleaf_popover_attach(tab->hover_popover, tab->popover_parent);
    gtk_popover_set_position(GTK_POPOVER(tab->hover_popover), GTK_POS_BOTTOM);
    gtk_popover_set_autohide(GTK_POPOVER(tab->hover_popover), FALSE);
    gtk_widget_set_can_focus(tab->hover_popover, TRUE);
    gtk_widget_add_css_class(tab->hover_popover, "cleaf-hover-popover");

    tab->hover_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(tab->hover_list),
                                    GTK_SELECTION_NONE);
    gtk_widget_add_css_class(tab->hover_list, "cleaf-ref-list");

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
    cleaf_popover_hide(tab->hover_popover);
}

static void tab_pack_widgets(EditorTab *tab) {
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(tab->scrolled),
                                  tab->text_view);
    gtk_overlay_set_child(GTK_OVERLAY(tab->popover_parent), tab->scrolled);

    gtk_widget_set_visible(tab->gutter, FALSE);
    gtk_box_append(GTK_BOX(tab->editor_area), tab->gutter);
    gtk_widget_set_hexpand(tab->popover_parent, TRUE);
    gtk_widget_set_vexpand(tab->popover_parent, TRUE);
    gtk_box_append(GTK_BOX(tab->editor_area), tab->popover_parent);
    gtk_box_append(GTK_BOX(tab->editor_area), tab->minimap_scrolled);
    gtk_box_append(GTK_BOX(tab->editor_area), tab->preview_scrolled);
    gtk_widget_set_vexpand(tab->editor_area, TRUE);
    gtk_box_append(GTK_BOX(tab->box), tab->editor_area);
}

static void tab_create_tab_label(EditorTab *tab) {
    tab->tab_widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_size_request(tab->tab_widget, 150, -1);
    gtk_widget_add_css_class(tab->tab_widget, "cleaf-tab");
    tab->tab_title = gtk_label_new("Untitled");
    gtk_widget_set_margin_start(tab->tab_title, 8);
    gtk_widget_set_margin_end(tab->tab_title, 6);
    gtk_label_set_xalign(GTK_LABEL(tab->tab_title), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(tab->tab_title), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(tab->tab_title), 32);
    gtk_widget_set_size_request(tab->tab_title, 96, -1);

    GtkWidget *close_btn = gtk_button_new_with_label("×");
    gtk_widget_add_css_class(close_btn, "cleaf-tab-close");
    gtk_widget_set_tooltip_text(close_btn, "Close tab");
    gtk_box_append(GTK_BOX(tab->tab_widget), tab->tab_title);
    gtk_box_append(GTK_BOX(tab->tab_widget), close_btn);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_button_clicked), tab);
}

/*
 * Controllers are owned by the widgets after gtk_widget_add_controller().  The
 * EditorTab user_data remains valid until tab destruction, where pending
 * timeouts/popovers are cancelled before the widgets are released.
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
    g_signal_connect(keys, "key-pressed",
                     G_CALLBACK(on_text_view_key_pressed), tab);
    g_signal_connect(keys, "key-released",
                     G_CALLBACK(on_text_view_key_released), tab);
    gtk_widget_add_controller(tab->text_view, keys);
}

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
