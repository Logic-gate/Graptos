/**
 * @file src/editor/editor_tab_context_menu.inc.c
 * @brief Context-menu helpers for editor tabs.
 * @details The context menu is small, but stale popovers are easy to leave behind. We keep
 *          the one-shot menu cleanup here so right-click UI does not leak old widgets after
 *          focus or selection changes.
 */

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
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param popover The popover supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
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
                                   "graptos-context-popover");

        if (stored == GTK_WIDGET(popover)) {
            g_object_set_data(G_OBJECT(parent),
                              "graptos-context-popover",
                              NULL);
        }
    }

    /*
     * Context popovers are one-shot widgets and should not remain allocated
     * after GTK has closed them.
     */
    graptos_widget_destroy(GTK_WIDGET(popover));
}


/**
 * context_button:
 * @tab: the editor tab passed to the button callback
 * @label: the text displayed by the button
 * @callback: the callback invoked when the button is activated
 *
 * Creates a context-menu button using Graptoς's shared flat-button styling.
 *
 * Returns: (transfer full): a newly created context-menu button
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param label The label supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *
context_button(EditorTab *tab,
               const char *label,
               GCallback callback)
{
    /*
     * Use the shared constructor so context-menu buttons remain visually and
     * behaviorally consistent with the rest of Graptoς.
     */
    return graptos_flat_button_new(label, NULL, callback, tab);
}

/**
 * @brief Return whether one widget is under another widget.
 * @details Right-click capture is installed on the editor overlay so Graptoς
 *          sees the event before GtkSourceView. This helper keeps the handler
 *          limited to clicks that actually landed inside the text view.
 * @param widget Widget picked at the click location.
 * @param ancestor Expected ancestor widget.
 * @return TRUE when @widget is @ancestor or one of its descendants.
 */
static gboolean context_widget_is_descendant(GtkWidget *widget,
                                             GtkWidget *ancestor) {
    while (widget) {
        if (widget == ancestor) return TRUE;
        widget = gtk_widget_get_parent(widget);
    }
    return FALSE;
}


/**
 * @brief Show the editor context menu.
 * @details The caller already decided this is Graptoς's secondary-click menu.
 *          Keeping menu construction separate from the event controller lets us
 *          stop GtkTextView's built-in context menu before opening ours.
 * @param tab Editor tab receiving the menu.
 * @param widget Text view owning the menu.
 * @param x Horizontal click position.
 * @param y Vertical click position.
 */
static void editor_tab_show_context_menu_at(EditorTab *tab,
                                            GtkWidget *widget,
                                            double x,
                                            double y) {
    GtkWidget *old_popover;
    GtkWidget *popover;
    GtkWidget *box;
    GdkRectangle rect;
    guint clicked_line = 0u;

    if (!tab || !widget)
        return;

    /*
     * Only one context popover should exist per text view. Destroy the previous
     * one before opening a new menu at the current pointer position.
     */
    old_popover = g_object_get_data(G_OBJECT(widget),
                                    "graptos-context-popover");

    if (old_popover && GTK_IS_POPOVER(old_popover)) {
        graptos_popover_hide(old_popover);
        graptos_widget_destroy(old_popover);
    }

    popover = gtk_popover_new();

    /*
     * Attach the popover to the text view so GTK can position it relative to
     * the editor instead of treating it as a detached window.
     */
    graptos_popover_attach(popover, widget);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "graptos-context-popover");

    /*
     * Store the current popover on the text view so a later right-click can
     * replace it without leaving multiple menus attached.
     */
    g_object_set_data(G_OBJECT(widget),
                      "graptos-context-popover",
                      popover);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    graptos_set_all_margins(box, 6);

    if (GTK_IS_TEXT_VIEW(widget)) {
        int buffer_x = 0;
        int buffer_y = 0;
        GtkTextIter iter;
        gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
                                              GTK_TEXT_WINDOW_WIDGET,
                                              (int)x,
                                              (int)y,
                                              &buffer_x,
                                              &buffer_y);
        gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget),
                                           &iter,
                                           buffer_x,
                                           buffer_y);
        clicked_line = (guint)gtk_text_iter_get_line(&iter) + 1u;
    }

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
    gtk_box_append(GTK_BOX(box),
                   context_button(tab,
                                  "Add Note",
                                  G_CALLBACK(menu_add_note)));
    if (tab->win && tab->win->plugins) {
        graptos_plugin_append_editor_context_items(tab->win->plugins,
                                                   tab,
                                                   box,
                                                   popover,
                                                   clicked_line);
    }

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

    graptos_popover_show(popover);
}

/**
 * @brief Handle a captured text-view right click.
 * @details GtkSourceView owns a default context menu. Graptoς claims the click
 *          sequence at capture phase before opening its own menu, which keeps
 *          the built-in menu from racing the editor context menu.
 * @param gesture Secondary-button gesture installed on the text view.
 * @param n_press Number of presses reported by GTK.
 * @param x Horizontal click position in text-view coordinates.
 * @param y Vertical click position in text-view coordinates.
 * @param user_data EditorTab owning the text view.
 */
void on_text_view_right_click(GtkGestureClick *gesture,
                              int n_press,
                              double x,
                              double y,
                              gpointer user_data) {
    (void)n_press;
    EditorTab *tab = user_data;
    if (!gesture) return;
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    if (!widget || !tab || !tab->text_view) return;
    GtkWidget *picked = gtk_widget_pick(widget, x, y, GTK_PICK_DEFAULT);
    if (!context_widget_is_descendant(picked, tab->text_view)) return;

    double text_x = x;
    double text_y = y;
    if (widget != tab->text_view) {
        graphene_point_t from = GRAPHENE_POINT_INIT((float)x, (float)y);
        graphene_point_t to = GRAPHENE_POINT_INIT(0.0f, 0.0f);
        if (gtk_widget_compute_point(widget, tab->text_view, &from, &to)) {
            text_x = to.x;
            text_y = to.y;
        }
    }
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    editor_tab_show_context_menu_at(tab, tab->text_view, text_x, text_y);
}
