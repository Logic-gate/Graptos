/**
 * @brief Return whether dialog callbacks should run.
 * @details GtkDropDown can emit selection notifications while models are being
 *          swapped, while pages are still being built, or while GTK tears down
 *          internal list views. We only let callbacks mutate the wizard when
 *          the dialog is fully alive and not inside a controlled rebuild.
 * @param dialog The project-init dialog.
 * @return TRUE when normal callback work is allowed.
 */
static gboolean project_init_dialog_accepts_callbacks(ProjectInitDialog *dialog) {
    return dialog &&
           !dialog->building &&
           !dialog->rebuilding &&
           !dialog->closing;
}

/**
 * @brief Disconnect a widget signal if the saved handler is still present.
 * @details Dialog teardown should not leave GtkDropDown notifications pointing
 *          at a ProjectInitDialog that is about to be freed.
 * @param widget The widget that owns the handler.
 * @param handler_id Saved signal handler id.
 */
static void project_init_disconnect_widget_signal(GtkWidget *widget,
                                                  gulong *handler_id) {
    if (!handler_id || *handler_id == 0u) return;
    if (G_IS_OBJECT(widget)) {
        g_signal_handler_disconnect(widget, *handler_id);
    }
    *handler_id = 0u;
}

/**
 * @brief Disconnect dynamic option controls from this dialog.
 * @details Option rows are destroyed whenever the selected template changes.
 *          Disconnecting by dialog data first prevents late switch/dropdown
 *          notifications from rebuilding the plan through stale controls.
 * @param dialog The project-init dialog.
 */
static void project_init_disconnect_option_controls(ProjectInitDialog *dialog) {
    if (!dialog || !dialog->controls) return;

    GHashTableIter iter;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iter, dialog->controls);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        (void)key;
        if (G_IS_OBJECT(value)) {
            g_signal_handlers_disconnect_by_data(value, dialog);
        }
    }
}

/**
 * @brief Disconnect all dialog-owned signals.
 * @details The init wizard uses the dialog state as callback data everywhere.
 *          A single teardown point makes close, cancel, and finalization safe
 *          even when GTK emits selection changes while destroying child widgets.
 * @param dialog The project-init dialog.
 */
static void project_init_disconnect_dialog_signals(ProjectInitDialog *dialog) {
    if (!dialog) return;
    project_init_disconnect_widget_signal(dialog->language_drop,
                                          &dialog->language_changed_id);
    project_init_disconnect_widget_signal(dialog->template_drop,
                                          &dialog->template_changed_id);
    project_init_disconnect_widget_signal(dialog->window,
                                          &dialog->close_request_id);
    project_init_disconnect_option_controls(dialog);
}

/**
 * @brief Assign a string model to a drop-down and release the local reference.
 * @details Init-project controls are rebuilt often. Using set_model() gives the
 *          drop-down its own reference before the builder releases the temporary
 *          reference, matching the ownership pattern used by the rest of
 *          Graptos. This avoids leaving GtkListView children with a freed model
 *          during dialog teardown.
 * @param drop_down The GtkDropDown receiving the model.
 * @param model The newly created model owned by the caller.
 */
static void project_init_drop_down_take_model(GtkWidget *drop_down,
                                              GtkStringList *model) {
    if (!GTK_IS_DROP_DOWN(drop_down) || !model) {
        project_init_debug("drop-down model rejected drop_down=%p is_dropdown=%d model=%p",
                           drop_down, GTK_IS_DROP_DOWN(drop_down), model);
        return;
    }
    guint count = g_list_model_get_n_items(G_LIST_MODEL(model));
    project_init_debug("drop-down set_model drop_down=%p model=%p items=%u",
                       drop_down, model, count);
    gtk_drop_down_set_model(GTK_DROP_DOWN(drop_down), G_LIST_MODEL(model));
    g_object_unref(model);
    project_init_debug("drop-down set_model complete drop_down=%p", drop_down);
}

/**
 * @brief Free project-init dialog state.
 * @details The window owns widgets, while this state owns discovered templates,
 *          generated models, controls metadata, and the latest plan.
 * @param data The ProjectInitDialog to free.
 */
static void project_init_dialog_free(gpointer data) {
    ProjectInitDialog *dialog = data;
    if (!dialog) return;
    dialog->closing = TRUE;
    project_init_debug("free dialog=%p window=%p templates=%u filtered=%u controls=%u plan=%p applying=%d",
                       dialog,
                       dialog->window,
                       dialog->templates ? dialog->templates->len : 0u,
                       dialog->filtered_templates ? dialog->filtered_templates->len : 0u,
                       dialog->controls ? g_hash_table_size(dialog->controls) : 0u,
                       dialog->plan,
                       dialog->applying);
    project_init_disconnect_dialog_signals(dialog);
    if (dialog->templates) g_ptr_array_free(dialog->templates, TRUE);
    if (dialog->filtered_templates) g_ptr_array_free(dialog->filtered_templates, TRUE);
    if (dialog->controls) g_hash_table_unref(dialog->controls);
    graptos_project_plan_free(dialog->plan);
    project_init_debug("free complete dialog=%p", dialog);
    g_free(dialog);
}
