/**
 * context_popover_closed:
 * @popover: the context-menu popover that was closed
 * @user_data: the text view that owns the active context popover
 *
 * Clears the text view's stored popover pointer when it still refers to
 * @popover, then destroys the one-shot popover.
 *
 * The identity check avoids clearing a newer popover that may have replaced
 * the closed one before this callback runs.
 */
static void
context_popover_closed(GtkPopover *popover,
                       gpointer user_data)
{
    GtkWidget *parent = user_data;

    /*
     * The text view stores the active context popover. Clear only if this is
     * still the same popover, because a new one may have been opened already.
     */
    if (parent) {
        GtkWidget *stored;

        stored = g_object_get_data(G_OBJECT(parent),
                                   "cleaf-context-popover");

        if (stored == GTK_WIDGET(popover)) {
            g_object_set_data(G_OBJECT(parent),
                              "cleaf-context-popover",
                              NULL);
        }
    }

    /*
     * Context popovers are one-shot widgets and should not remain allocated
     * after GTK has closed them.
     */
    cleaf_widget_destroy(GTK_WIDGET(popover));
}


/**
 * context_button:
 * @tab: the editor tab passed to the button callback
 * @label: the text displayed by the button
 * @callback: the callback invoked when the button is activated
 *
 * Creates a context-menu button using Cleaf's shared flat-button styling.
 *
 * Returns: (transfer full): a newly created context-menu button
 */
static GtkWidget *
context_button(EditorTab *tab,
               const char *label,
               GCallback callback)
{
    /*
     * Use the shared constructor so context-menu buttons remain visually and
     * behaviorally consistent with the rest of Cleaf.
     */
    return cleaf_flat_button_new(label, NULL, callback, tab);
}


/**
 * on_text_view_right_click:
 * @gesture: the click gesture that received the right-button event
 * @n_press: the number of presses in the current click sequence
 * @x: the horizontal click position relative to the text view
 * @y: the vertical click position relative to the text view
 * @user_data: the #EditorTab associated with the text view
 *
 * Opens the editor context menu at the pointer position.
 *
 * Any previously stored context popover is destroyed before the new one is
 * attached. This keeps one active context menu per text view and prevents
 * stale popovers from retaining references to editor state.
 */
void
on_text_view_right_click(GtkGestureClick *gesture,
                         int n_press,
                         double x,
                         double y,
                         gpointer user_data)
{
    EditorTab *tab = user_data;
    GtkWidget *widget;
    GtkWidget *old_popover;
    GtkWidget *popover;
    GtkWidget *box;
    GdkRectangle rect;

    (void)n_press;

    widget = gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture));

    if (!tab || !widget)
        return;

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
    old_popover = g_object_get_data(G_OBJECT(widget),
                                    "cleaf-context-popover");

    if (old_popover && GTK_IS_POPOVER(old_popover)) {
        cleaf_popover_hide(old_popover);
        cleaf_widget_destroy(old_popover);
    }

    popover = gtk_popover_new();

    /*
     * Attach the popover to the text view so GTK can position it relative to
     * the editor instead of treating it as a detached window.
     */
    cleaf_popover_attach(popover, widget);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "cleaf-context-popover");

    /*
     * Store the current popover on the text view so a later right-click can
     * replace it without leaving multiple menus attached.
     */
    g_object_set_data(G_OBJECT(widget),
                      "cleaf-context-popover",
                      popover);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    cleaf_set_all_margins(box, 6);

    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Undo",
                                  G_CALLBACK(menu_undo)));
    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Redo",
                                  G_CALLBACK(menu_redo)));

    gtk_box_append(
        GTK_BOX(box),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Cut",
                                  G_CALLBACK(menu_cut)));
    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Copy",
                                  G_CALLBACK(menu_copy)));
    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Paste",
                                  G_CALLBACK(menu_paste)));
    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Select All",
                                  G_CALLBACK(menu_select_all)));

    gtk_box_append(
        GTK_BOX(box),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Cut Line",
                                  G_CALLBACK(menu_cut_line)));
    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Paste Cut Line",
                                  G_CALLBACK(menu_paste_line)));
    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Toggle Comment",
                                  G_CALLBACK(menu_comment)));
    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Auto Complete",
                                  G_CALLBACK(menu_complete)));

    /*
     * Use the click position as a tiny target rectangle so the popover opens
     * where the user requested the context menu.
     */
    rect = (GdkRectangle) {
        .x = (int)x,
        .y = (int)y,
        .width = 1,
        .height = 1
    };

    gtk_popover_set_child(GTK_POPOVER(popover), box);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);

    /*
     * The closed callback clears the stored pointer before destroying the
     * one-shot menu.
     */
    g_signal_connect(popover,
                     "closed",
                     G_CALLBACK(context_popover_closed),
                     widget);

    cleaf_popover_show(popover);
}