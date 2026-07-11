#include "ui.h"

/*
 * Cleaf keeps a few custom dialogs synchronous because the surrounding save and
 * close flows must return a concrete response before continuing.  The nested
 * loop is local to the dialog and is stopped by either an explicit response or
 * the window close signal.
 */
typedef struct {
    GMainLoop *loop;
    int response;
} ModalRunState;

typedef struct {
    GMainLoop *loop;
    char *path;
} FileDialogState;

static gboolean modal_close_cb(GtkWindow *window, gpointer user_data) {
    (void)window;
    ModalRunState *state = user_data;
    if (!state) return TRUE;
    state->response = GTK_RESPONSE_CANCEL;
    if (state->loop) g_main_loop_quit(state->loop);
    return TRUE;
}

void cleaf_set_all_margins(GtkWidget *widget, int margin) {
    if (!widget) return;
    gtk_widget_set_margin_start(widget, margin);
    gtk_widget_set_margin_end(widget, margin);
    gtk_widget_set_margin_top(widget, margin);
    gtk_widget_set_margin_bottom(widget, margin);
}

int cleaf_modal_window_run(GtkWindow *window, int default_response) {
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
    g_object_set_data(G_OBJECT(window), "cleaf-modal-state", &state);
    gulong close_handler = g_signal_connect(window, "close-request",
                                            G_CALLBACK(modal_close_cb),
                                            &state);
    gtk_window_set_modal(window, TRUE);
    gtk_window_present(window);
    g_main_loop_run(state.loop);
    g_signal_handler_disconnect(window, close_handler);
    g_object_set_data(G_OBJECT(window), "cleaf-modal-state", NULL);
    g_main_loop_unref(state.loop);
    g_object_unref(window);
    return state.response;
}

void cleaf_modal_window_respond(GtkWidget *widget, gpointer user_data) {
    int response = GPOINTER_TO_INT(user_data);
    GtkRoot *root = widget ? gtk_widget_get_root(widget) : NULL;
    if (!GTK_IS_WINDOW(root)) return;

    ModalRunState *state = g_object_get_data(G_OBJECT(root),
                                             "cleaf-modal-state");
    if (state) {
        state->response = response;
        if (state->loop) g_main_loop_quit(state->loop);
    }
}

/*
 * GtkFileDialog is asynchronous in GTK4.  Cleaf wraps it in the same modal
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

    GError *error = NULL;
    GFile *file = finish_func(dialog, result, &error);
    if (file) {
        state->path = g_file_get_path(file);
        g_object_unref(file);
    }
    g_clear_error(&error);
    if (state->loop) g_main_loop_quit(state->loop);
}

static void open_finish_cb(GObject *source, GAsyncResult *result,
                           gpointer user_data) {
    file_dialog_finish(GTK_FILE_DIALOG(source), result, user_data,
                       gtk_file_dialog_open_finish);
}

static void save_finish_cb(GObject *source, GAsyncResult *result,
                           gpointer user_data) {
    file_dialog_finish(GTK_FILE_DIALOG(source), result, user_data,
                       gtk_file_dialog_save_finish);
}

static void folder_finish_cb(GObject *source, GAsyncResult *result,
                             gpointer user_data) {
    file_dialog_finish(GTK_FILE_DIALOG(source), result, user_data,
                       gtk_file_dialog_select_folder_finish);
}

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

char *cleaf_open_file_dialog(GtkWindow *parent, const char *title) {
    return run_file_dialog(parent, title ? title : "Open File",
                           gtk_file_dialog_open, open_finish_cb);
}

char *cleaf_save_file_dialog(GtkWindow *parent, const char *title) {
    return run_file_dialog(parent, title ? title : "Save File",
                           gtk_file_dialog_save, save_finish_cb);
}

char *cleaf_select_folder_dialog(GtkWindow *parent, const char *title) {
    return run_file_dialog(parent, title ? title : "Open Folder",
                           gtk_file_dialog_select_folder, folder_finish_cb);
}

void cleaf_source_cancel(guint *source_id) {
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

/*
 * Popovers are not normal toplevels: destroying the row or overlay that owns
 * them while they still have a child can produce GTK finalization warnings.
 * Centralize the teardown order so callers do not have to know that detail.
 */
void cleaf_widget_destroy(GtkWidget *widget) {
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

void cleaf_box_append(GtkWidget *box, GtkWidget *child) {
    if (box && child) gtk_box_append(GTK_BOX(box), child);
}

void cleaf_box_prepend(GtkWidget *box, GtkWidget *child) {
    if (box && child) gtk_box_prepend(GTK_BOX(box), child);
}

/*
 * Several popovers are reused and moved between rebuilt rows/overlays.  GTK
 * requires a single widget parent, so detach first rather than relying on the
 * caller to remember the previous owner.
 */
void cleaf_popover_attach(GtkWidget *popover, GtkWidget *parent) {
    if (!popover || !parent) return;
    if (gtk_widget_get_parent(popover) == parent) return;
    if (gtk_widget_get_parent(popover)) gtk_widget_unparent(popover);
    gtk_widget_set_parent(popover, parent);
}

void cleaf_popover_show(GtkWidget *popover) {
    if (!popover || !GTK_IS_POPOVER(popover)) return;

    GtkWidget *parent = gtk_widget_get_parent(popover);
    if (!parent) return;

    /* Avoid "Trying to snapshot GtkGizmo without a current allocation" by
     * refusing to popup against widgets that are not yet realized/mapped or do
     * not have an allocation.  The next user event/timer can show it normally.
     */
    if (!gtk_widget_get_realized(parent) || !gtk_widget_get_mapped(parent)) return;
    if (gtk_widget_get_allocated_width(parent) <= 0 ||
        gtk_widget_get_allocated_height(parent) <= 0) return;

    gtk_popover_popup(GTK_POPOVER(popover));
}

void cleaf_popover_hide(GtkWidget *popover) {
    if (popover && GTK_IS_POPOVER(popover) &&
        gtk_widget_get_parent(popover) && gtk_widget_get_visible(popover)) {
        gtk_popover_popdown(GTK_POPOVER(popover));
    }
}

void cleaf_list_box_clear(GtkWidget *list_box) {
    if (!list_box || !GTK_IS_LIST_BOX(list_box)) return;

    GtkWidget *child = gtk_widget_get_first_child(list_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(list_box), child);
        child = next;
    }
}

GtkWidget *cleaf_button_new(const char *label, const char *tooltip,
                            GCallback callback, gpointer data) {
    GtkWidget *button = gtk_button_new_with_label(label ? label : "");
    if (tooltip) gtk_widget_set_tooltip_text(button, tooltip);
    if (callback) g_signal_connect(button, "clicked", callback, data);
    return button;
}

GtkWidget *cleaf_flat_button_new(const char *label, const char *tooltip,
                                 GCallback callback, gpointer data) {
    GtkWidget *button = cleaf_button_new(label, tooltip, callback, data);
    gtk_widget_add_css_class(button, "cleaf-flat");
    return button;
}

GtkWidget *cleaf_separator_new(void) {
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_start(sep, 4);
    gtk_widget_set_margin_end(sep, 4);
    return sep;
}
