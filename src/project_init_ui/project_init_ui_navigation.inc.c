/**
 * @brief Choose the destination parent directory.
 * @details The final project directory is still derived from the project name;
 *          the chooser only selects the parent directory. The file chooser is
 *          parented to the init dialog, not the main window, because the init
 *          dialog is already modal and would otherwise sit above its own
 *          chooser on some compositors.
 * @param widget The button that triggered selection.
 * @param user_data The ProjectInitDialog.
 */
static void project_init_choose_destination(GtkWidget *widget, gpointer user_data) {
    ProjectInitDialog *dialog = user_data;
    project_init_debug("browse clicked widget=%p dialog=%p window=%p",
                       widget, dialog, dialog ? dialog->window : NULL);
    if (!project_init_dialog_accepts_callbacks(dialog) ||
        !GTK_IS_WINDOW(dialog->window)) {
        return;
    }
    g_autofree char *folder =
        graptos_select_folder_dialog(GTK_WINDOW(dialog->window),
                                     "Choose Destination Parent");
    project_init_debug("browse returned folder=%s dialog=%p",
                       folder ? folder : "(cancelled)", dialog);
    if (!folder) return;
    gtk_editable_set_text(GTK_EDITABLE(dialog->destination_entry), folder);
    project_init_rebuild_plan(dialog);
}

/**
 * @brief Move to the next wizard page.
 * @details Navigation is intentionally light because validation remains live
 *          across all pages and Create is gated by the latest plan.
 * @param widget The button that emitted the signal.
 * @param user_data The ProjectInitDialog.
 */
static void project_init_next(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ProjectInitDialog *dialog = user_data;
    project_init_debug("next clicked dialog=%p page=%u", dialog, dialog ? dialog->page : 0u);
    if (!project_init_dialog_accepts_callbacks(dialog)) return;
    project_init_set_page(dialog, dialog->page + 1u);
}

/**
 * @brief Move to the previous wizard page.
 * @details The dialog keeps entered values while moving between pages so the
 *          preview does not need to be rebuilt from scratch by page state.
 * @param widget The button that emitted the signal.
 * @param user_data The ProjectInitDialog.
 */
static void project_init_back(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ProjectInitDialog *dialog = user_data;
    project_init_debug("back clicked dialog=%p page=%u", dialog, dialog ? dialog->page : 0u);
    if (!project_init_dialog_accepts_callbacks(dialog)) return;
    if (dialog->page > 0u) project_init_set_page(dialog, dialog->page - 1u);
}

/**
 * @brief Close the project-init dialog.
 * @details Closing is ignored while apply is running because the async callback
 *          needs the dialog widgets to show success or failure.
 * @param widget The button that emitted the signal.
 * @param user_data The ProjectInitDialog.
 */
static void project_init_cancel(GtkWidget *widget, gpointer user_data) {
    ProjectInitDialog *dialog = user_data;
    project_init_debug("cancel clicked widget=%p dialog=%p applying=%d window=%p",
                       widget, dialog, dialog ? dialog->applying : FALSE,
                       dialog ? dialog->window : NULL);
    if (dialog && dialog->applying) return;
    if (dialog && dialog->window) {
        dialog->closing = TRUE;
        project_init_disconnect_dialog_signals(dialog);
        graptos_widget_destroy(dialog->window);
    }
}

/**
 * @brief Handle window close requests during project creation.
 * @details The async worker returns to widgets owned by the dialog. Blocking
 *          close while applying keeps that state valid until the result is
 *          presented or the dialog closes itself after success.
 * @param window The dialog window.
 * @param user_data The ProjectInitDialog.
 * @return TRUE to stop closing while an apply is active.
 */
static gboolean project_init_close_request(GtkWindow *window, gpointer user_data) {
    ProjectInitDialog *dialog = user_data;
    project_init_debug("close-request window=%p dialog=%p applying=%d",
                       window, dialog, dialog ? dialog->applying : FALSE);
    if (!dialog) return FALSE;
    if (dialog->applying) return TRUE;
    dialog->closing = TRUE;
    project_init_disconnect_dialog_signals(dialog);
    return FALSE;
}
