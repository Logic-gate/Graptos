static void context_popover_closed(GtkPopover *popover, gpointer user_data) {
    GtkWidget *parent = user_data;

    /*
     * The text view stores the active context popover. Clear only if this is
     * still the same popover, because a new one may have been opened already.
     */
    if (parent) {
        GtkWidget *stored = g_object_get_data(G_OBJECT(parent),
                                             "cleaf-context-popover");
        if (stored == GTK_WIDGET(popover)) {
            g_object_set_data(G_OBJECT(parent), "cleaf-context-popover", NULL);
        }
    }

    // Context popovers are one-shot widgets and should be destroyed after closing.
    cleaf_widget_destroy(GTK_WIDGET(popover));
}


static GtkWidget *context_button(EditorTab *tab,
                                 const char *label,
                                 GCallback callback) {
    // Keep context-menu buttons visually consistent with the rest of Cleaf.
    return cleaf_flat_button_new(label, NULL, callback, tab);
}


void on_text_view_right_click(GtkGestureClick *gesture,
                              int n_press,
                              double x,
                              double y,
                              gpointer user_data) {
    (void)n_press;

    EditorTab *tab = user_data;
    GtkWidget *widget = gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture));

    if (!tab || !widget) return;

    /*
     * Claim the sequence so the right-click is handled by Cleaf's context menu
     * instead of falling through to other gesture handlers.
     */
    gtk_gesture_set_state(GTK_GESTURE(gesture),
                          GTK_EVENT_SEQUENCE_CLAIMED);

    /*
     * Only one context popover should exist per text view. Destroy the previous
     * one before opening a new menu at the current pointer position.
     */
    GtkWidget *old_popover = g_object_get_data(G_OBJECT(widget),
                                               "cleaf-context-popover");
    if (old_popover && GTK_IS_POPOVER(old_popover)) {
        cleaf_popover_hide(old_popover);
        cleaf_widget_destroy(old_popover);
    }

    GtkWidget *popover = gtk_popover_new();

    /*
     * Attach the popover to the text view so GTK can position it relative to the
     * editor instead of treating it as a detached window.
     */
    cleaf_popover_attach(popover, widget);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "cleaf-context-popover");

    // Store it on the widget so the next right-click can replace it cleanly.
    g_object_set_data(G_OBJECT(widget), "cleaf-context-popover", popover);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    cleaf_set_all_margins(box, 6);

    gtk_box_append(GTK_BOX(box), context_button(tab, "Undo",
                                                G_CALLBACK(menu_undo)));
    gtk_box_append(GTK_BOX(box), context_button(tab, "Redo",
                                                G_CALLBACK(menu_redo)));

    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    gtk_box_append(GTK_BOX(box), context_button(tab, "Cut",
                                                G_CALLBACK(menu_cut)));
    gtk_box_append(GTK_BOX(box), context_button(tab, "Copy",
                                                G_CALLBACK(menu_copy)));
    gtk_box_append(GTK_BOX(box), context_button(tab, "Paste",
                                                G_CALLBACK(menu_paste)));
    gtk_box_append(GTK_BOX(box), context_button(tab, "Select All",
                                                G_CALLBACK(menu_select_all)));

    gtk_box_append(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    gtk_box_append(GTK_BOX(box), context_button(tab, "Cut Line",
                                                G_CALLBACK(menu_cut_line)));
    gtk_box_append(GTK_BOX(box), context_button(tab, "Paste Cut Line",
                                                G_CALLBACK(menu_paste_line)));
    gtk_box_append(GTK_BOX(box), context_button(tab, "Toggle Comment",
                                                G_CALLBACK(menu_comment)));
    gtk_box_append(GTK_BOX(box), context_button(tab, "Auto Complete",
                                                G_CALLBACK(menu_complete)));

    /*
     * Use the click position as a tiny target rectangle. This makes the popover
     * appear where the user opened the menu.
     */
    GdkRectangle rect = { (int)x, (int)y, 1, 1 };

    gtk_popover_set_child(GTK_POPOVER(popover), box);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);

    /*
     * When GTK closes the popover, clear the stored pointer and destroy the
     * one-shot menu widget.
     */
    g_signal_connect(popover,
                     "closed",
                     G_CALLBACK(context_popover_closed),
                     widget);

    cleaf_popover_show(popover);
}