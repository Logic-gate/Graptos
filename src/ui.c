/**
 * @file src/ui.c
 * @brief Shared GTK utility helpers and compatibility wrappers.
 * @details Theme values should be traceable. We translate config into GTK CSS here so
 *          colors and fonts do not get scattered through feature code where they are hard
 *          to reason about later.
 */

#include "ui.h"

/*
 * Graptoς keeps a few custom dialogs synchronous because the surrounding save and
 * close flows must return a concrete response before continuing.  The nested
 * loop is local to the dialog and is stopped by either an explicit response or
 * the window close signal.
 */
typedef struct {
    GMainLoop *loop; /**< Loop. */
    int response; /**< Response. */
} ModalRunState;

/**
 * @brief Ui type definition.
 */
typedef struct {
    GMainLoop *loop; /**< Loop. */
    char *path; /**< Path. */
} FileDialogState;

/**
 * @brief Modal close cb.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param window The window supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean modal_close_cb(GtkWindow *window, gpointer user_data) {
    (void)window;
    ModalRunState *state = user_data;
    if (!state) return TRUE;
    state->response = GTK_RESPONSE_CANCEL;
    if (state->loop) g_main_loop_quit(state->loop);
    return TRUE;
}

/**
 * @brief Graptoς set all margins.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param widget The widget that emitted the callback or receives the update.
 * @param margin The margin supplied by the caller.
 */
void graptos_set_all_margins(GtkWidget *widget, int margin) {
    if (!widget) return;
    gtk_widget_set_margin_start(widget, margin);
    gtk_widget_set_margin_end(widget, margin);
    gtk_widget_set_margin_top(widget, margin);
    gtk_widget_set_margin_bottom(widget, margin);
}

/**
 * @brief Graptoς modal window run.
 * @details GTK4 prefers async dialogs, but close/save/error flows are easier to
 *          reason about when they return one concrete response. The nested loop
 *          is deliberately small and owned by this helper, so callers get the
 *          old synchronous shape without spreading nested-loop code around.
 * @param window The window supplied by the caller.
 * @param default_response The default response supplied by the caller.
 * @return The computed value requested by the caller.
 */
int graptos_modal_window_run(GtkWindow *window, int default_response) {
    if (!window) return GTK_RESPONSE_NONE;

    ModalRunState state;
    state.loop = g_main_loop_new(NULL, FALSE);
    state.response = default_response;

    /*
     * The caller still owns the dialog widget.  Take a temporary reference so
     * the window cannot disappear while the nested loop is active, then release
     * it before returning the response.
     */
    g_object_ref(window);
    g_object_set_data(G_OBJECT(window), "graptos-modal-state", &state);
    gulong close_handler = g_signal_connect(window, "close-request",
                                            G_CALLBACK(modal_close_cb),
                                            &state);
    gtk_window_set_modal(window, TRUE);
    gtk_window_present(window);
    g_main_loop_run(state.loop);
    g_signal_handler_disconnect(window, close_handler);
    g_object_set_data(G_OBJECT(window), "graptos-modal-state", NULL);
    g_main_loop_unref(state.loop);
    g_object_unref(window);
    return state.response;
}

/**
 * @brief Graptoς modal window respond.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void graptos_modal_window_respond(GtkWidget *widget, gpointer user_data) {
    int response = GPOINTER_TO_INT(user_data);
    GtkRoot *root = widget ? gtk_widget_get_root(widget) : NULL;
    if (!GTK_IS_WINDOW(root)) return;

/**
 * @brief File dialog finish.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param dialog The dialog supplied by the caller.
 * @param result The result supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 * @param finish_func The finish func supplied by the caller.
 */
    ModalRunState *state = g_object_get_data(G_OBJECT(root),
                                             "graptos-modal-state");
    if (state) {
        state->response = response;
        if (state->loop) g_main_loop_quit(state->loop);
    }
}

/**
 * @brief Finish a synchronous-style file dialog wrapper.
 * @details GtkFileDialog is asynchronous in GTK4. Graptoς wraps it in the same
 *          modal style used by the rest of the UI so file/open/save actions can
 *          stay simple without leaking the GFile returned by the finish
 *          function.
 * @param dialog The file dialog that completed.
 * @param result The asynchronous result passed by GTK.
 * @param user_data The FileDialogState used to wake the nested modal loop.
 * @param finish_func The GtkFileDialog finish function matching the operation.
 *
 * GtkFileDialog is asynchronous in GTK4.  Graptoς wraps it in the same modal
 * style used by the rest of the UI so file/open/save actions can stay simple
 * without leaking the GFile returned by the finish function.
 */
static void file_dialog_finish(GtkFileDialog *dialog,
                               GAsyncResult *result,
                               gpointer user_data,
                               GFile *(*finish_func)(GtkFileDialog *,
                                                     GAsyncResult *,
                                                     GError **)) {
    FileDialogState *state = user_data;
    if (!state) return;

    g_autoptr(GError) error = NULL;
    g_autoptr(GFile) file = finish_func(dialog, result, &error);
    if (file) {
        state->path = g_file_get_path(file);
    }
    if (state->loop) g_main_loop_quit(state->loop);
}

/**
 * @brief Open finish cb.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param source The source supplied by the caller.
 * @param result The result supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void open_finish_cb(GObject *source, GAsyncResult *result,
                           gpointer user_data) {
    file_dialog_finish(GTK_FILE_DIALOG(source), result, user_data,
                       gtk_file_dialog_open_finish);
}

/**
 * @brief Save finish cb.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param source The source supplied by the caller.
 * @param result The result supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void save_finish_cb(GObject *source, GAsyncResult *result,
                           gpointer user_data) {
    file_dialog_finish(GTK_FILE_DIALOG(source), result, user_data,
                       gtk_file_dialog_save_finish);
}

/**
 * @brief Folder finish cb.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param source The source supplied by the caller.
 * @param result The result supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void folder_finish_cb(GObject *source, GAsyncResult *result,
                             gpointer user_data) {
    file_dialog_finish(GTK_FILE_DIALOG(source), result, user_data,
                       gtk_file_dialog_select_folder_finish);
}

/**
 * @brief Run file dialog.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @param start_func The start func supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *run_file_dialog(GtkWindow *parent,
                             const char *title,
                             void (*start_func)(GtkFileDialog *,
                                                GtkWindow *,
                                                GCancellable *,
                                                GAsyncReadyCallback,
                                                gpointer),
                             GAsyncReadyCallback callback) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    if (!dialog) return NULL;
    if (title) gtk_file_dialog_set_title(dialog, title);

    FileDialogState state;
    state.loop = g_main_loop_new(NULL, FALSE);
    state.path = NULL;

    start_func(dialog, parent, NULL, callback, &state);
    g_main_loop_run(state.loop);
    g_main_loop_unref(state.loop);
    g_object_unref(dialog);
    return state.path;
}

/**
 * @brief Graptoς open file dialog.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_open_file_dialog(GtkWindow *parent, const char *title) {
    return run_file_dialog(parent, title ? title : "Open File",
                           gtk_file_dialog_open, open_finish_cb);
}

/**
 * @brief Graptoς save file dialog.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_save_file_dialog(GtkWindow *parent, const char *title) {
/**
 * @brief Widget destroy.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param widget The widget that emitted the callback or receives the update.
 */
    return run_file_dialog(parent, title ? title : "Save File",
                           gtk_file_dialog_save, save_finish_cb);
}

/**
 * @brief Graptoς select folder dialog.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_select_folder_dialog(GtkWindow *parent, const char *title) {
    return run_file_dialog(parent, title ? title : "Open Folder",
                           gtk_file_dialog_select_folder, folder_finish_cb);
}

/**
 * @brief Graptoς source cancel.
 * @details Stored source IDs can go stale after a timeout removes itself. We
 *          look up the source before destroying it because the alternative is a
 *          stream of GLib criticals that hide real warnings during debugging.
 * @param source_id The source id supplied by the caller.
 */
void graptos_source_cancel(guint *source_id) {
    if (!source_id || *source_id == 0u) return;

    /*
     * Do not cancel timeout IDs blindly here.  In GTK/GLib code it is
     * common for a timeout callback to run and remove itself before another
     * branch clears the stored integer ID.  Cancelling such
     * a stale ID produces noisy "Source ID was not found" critical messages.
     * Looking up the source first avoids that diagnostic and still destroys
     * the source when it is actually pending.
     */
    GSource *source = g_main_context_find_source_by_id(NULL, *source_id);
    if (source) g_source_destroy(source);
    *source_id = 0u;
}

/**
 * @brief Destroy a widget through the Graptoς-safe path.
 * @details Popovers are not normal toplevels: destroying the row or overlay
 *          that owns them while they still have a child can produce GTK
 *          finalization warnings. Centralizing the teardown order keeps callers
 *          from having to remember that detail.
 * @param widget The widget that should be torn down.
 */
void graptos_widget_destroy(GtkWidget *widget) {
    if (!widget) return;

    if (GTK_IS_WINDOW(widget)) {
        gtk_window_destroy(GTK_WINDOW(widget));
        return;
    }

    if (GTK_IS_POPOVER(widget)) {
        GtkWidget *parent = gtk_widget_get_parent(widget);
        if (parent && gtk_widget_get_visible(widget)) {
            gtk_popover_popdown(GTK_POPOVER(widget));
        }
        gtk_popover_set_child(GTK_POPOVER(widget), NULL);
        if (parent) gtk_widget_unparent(widget);
        return;
    }

    if (gtk_widget_get_parent(widget)) {
        gtk_widget_unparent(widget);
    }
}

/**
 * @brief Graptoς box append.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param box The box supplied by the caller.
 * @param child The child supplied by the caller.
 */
void graptos_box_append(GtkWidget *box, GtkWidget *child) {
    if (box && child) gtk_box_append(GTK_BOX(box), child);
}

/**
 * @brief Graptoς box prepend.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param box The box supplied by the caller.
 * @param child The child supplied by the caller.
 */
void graptos_box_prepend(GtkWidget *box, GtkWidget *child) {
    if (box && child) gtk_box_prepend(GTK_BOX(box), child);
}

/**
 * @brief Attach a popover to a widget after detaching any old parent.
 * @details Several popovers are reused and moved between rebuilt rows and
 *          overlays. GTK requires a single widget parent, so detaching first
 *          keeps callers from having to remember the previous owner.
 * @param popover The popover being reparented.
 * @param parent The widget that should own the popover.
 */
void graptos_popover_attach(GtkWidget *popover, GtkWidget *parent) {
    if (!popover || !parent) return;
    if (gtk_widget_get_parent(popover) == parent) return;
    if (gtk_widget_get_parent(popover)) gtk_widget_unparent(popover);
    gtk_widget_set_parent(popover, parent);
}

/**
 * @brief Graptoς popover show.
 * @details Popovers are sensitive to timing. Showing one before the parent has
 *          an allocation causes GTK snapshot warnings, so this helper refuses
 *          to show early and lets the next user event or timeout try again.
 * @param popover The popover supplied by the caller.
 */
void graptos_popover_show(GtkWidget *popover) {
    if (!popover || !GTK_IS_POPOVER(popover)) return;

    GtkWidget *parent = gtk_widget_get_parent(popover);
    if (!parent) return;

    /* Avoid "Trying to snapshot GtkGizmo without a current allocation" by
     * refusing to popup against widgets that are not yet realized/mapped or do
     * not have an allocation.  The next user event/timer can show it normally.
     */
    if (!gtk_widget_get_realized(parent) || !gtk_widget_get_mapped(parent)) return;
    if (gtk_widget_get_width(parent) <= 0 ||
        gtk_widget_get_height(parent) <= 0) return;

    gtk_popover_popup(GTK_POPOVER(popover));
}

/**
 * @brief Graptoς popover hide.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param popover The popover supplied by the caller.
 */
void graptos_popover_hide(GtkWidget *popover) {
    if (popover && GTK_IS_POPOVER(popover) &&
        gtk_widget_get_parent(popover) && gtk_widget_get_visible(popover)) {
        gtk_popover_popdown(GTK_POPOVER(popover));
    }
}

/**
 * @brief Graptoς list box clear.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param list_box The list box supplied by the caller.
 */
void graptos_list_box_clear(GtkWidget *list_box) {
    if (!list_box || !GTK_IS_LIST_BOX(list_box)) return;

    GtkWidget *child = gtk_widget_get_first_child(list_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(list_box), child);
        child = next;
    }
}

/**
 * @brief Graptoς button new.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param label The label supplied by the caller.
 * @param tooltip The tooltip supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param data The callback context passed by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *graptos_button_new(const char *label, const char *tooltip,
                            GCallback callback, gpointer data) {
    GtkWidget *button = gtk_button_new_with_label(label ? label : "");
    if (tooltip) gtk_widget_set_tooltip_text(button, tooltip);
    if (callback) g_signal_connect(button, "clicked", callback, data);
    return button;
}

/**
 * @brief Graptoς flat button new.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param label The label supplied by the caller.
 * @param tooltip The tooltip supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param data The callback context passed by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *graptos_flat_button_new(const char *label, const char *tooltip,
                                 GCallback callback, gpointer data) {
    GtkWidget *button = graptos_button_new(label, tooltip, callback, data);
    gtk_widget_add_css_class(button, "graptos-flat");
    return button;
}

/**
 * @brief Graptoς separator new.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *graptos_separator_new(void) {
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(sep, 4);
    gtk_widget_set_margin_end(sep, 4);
    return sep;
}
