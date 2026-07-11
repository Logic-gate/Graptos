#include "dialogs.h"
#include "ui.h"

static GtkWidget *dialog_window_new(GtkWindow *parent,
                                    const char *title,
                                    int width,
                                    int height) {
    GtkWidget *window = gtk_window_new();
    gtk_widget_add_css_class(window, "cleaf-window");
    gtk_window_set_title(GTK_WINDOW(window), title ? title : "Cleaf");
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    if (parent) gtk_window_set_transient_for(GTK_WINDOW(window), parent);
    return window;
}

static GtkWidget *dialog_content_new(GtkWidget *window) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(box, "cleaf-root");
    cleaf_set_all_margins(box, 14);
    gtk_window_set_child(GTK_WINDOW(window), box);
    return box;
}

static GtkWidget *dialog_label_new(const char *text, gboolean title) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    if (title) gtk_widget_add_css_class(label, "cleaf-menu-title");
    return label;
}

static GtkWidget *button_row_new(void) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(row, GTK_ALIGN_END);
    return row;
}

static void show_message(GtkWindow *parent,
                         const char *title,
                         const char *primary,
                         const char *detail) {
    GtkWidget *window = dialog_window_new(parent, title, 420, 160);
    GtkWidget *box = dialog_content_new(window);
    gtk_box_append(GTK_BOX(box), dialog_label_new(primary, TRUE));
    if (detail && detail[0] != '\0') {
        gtk_box_append(GTK_BOX(box), dialog_label_new(detail, FALSE));
    }

    GtkWidget *row = button_row_new();
    GtkWidget *ok = cleaf_flat_button_new("Close", NULL,
                                          G_CALLBACK(cleaf_modal_window_respond),
                                          GINT_TO_POINTER(GTK_RESPONSE_CLOSE));
    gtk_box_append(GTK_BOX(row), ok);
    gtk_box_append(GTK_BOX(box), row);
    (void)cleaf_modal_window_run(GTK_WINDOW(window), GTK_RESPONSE_CLOSE);
    cleaf_widget_destroy(window);
}

void dialog_error(GtkWindow *parent, const char *primary, const char *detail) {
    show_message(parent, "Error", primary ? primary : "Error", detail);
}

void dialog_info(GtkWindow *parent, const char *primary, const char *detail) {
    show_message(parent, "Information",
                 primary ? primary : "Information", detail);
}

char *dialog_prompt_text(GtkWindow *parent,
                         const char *title,
                         const char *label,
                         const char *initial) {
    GtkWidget *window = dialog_window_new(parent, title ? title : "Input",
                                          460, 150);
    GtkWidget *box = dialog_content_new(window);
    gtk_box_append(GTK_BOX(box), dialog_label_new(label ? label : "Value:",
                                                  FALSE));

    GtkWidget *entry = gtk_entry_new();
    if (initial) gtk_entry_set_text(GTK_ENTRY(entry), initial);
    gtk_box_append(GTK_BOX(box), entry);

    GtkWidget *row = button_row_new();
    GtkWidget *cancel = cleaf_flat_button_new("Cancel", NULL,
        G_CALLBACK(cleaf_modal_window_respond),
        GINT_TO_POINTER(GTK_RESPONSE_CANCEL));
    GtkWidget *ok = cleaf_flat_button_new("OK", NULL,
        G_CALLBACK(cleaf_modal_window_respond),
        GINT_TO_POINTER(GTK_RESPONSE_ACCEPT));
    gtk_box_append(GTK_BOX(row), cancel);
    gtk_box_append(GTK_BOX(row), ok);
    gtk_box_append(GTK_BOX(box), row);

    gtk_widget_grab_focus(entry);
    char *result = NULL;
    if (cleaf_modal_window_run(GTK_WINDOW(window), GTK_RESPONSE_CANCEL)
        == GTK_RESPONSE_ACCEPT) {
        result = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    }
    cleaf_widget_destroy(window);
    return result;
}

gboolean dialog_confirm_yes_no(GtkWindow *parent,
                               const char *title,
                               const char *message) {
    GtkWidget *window = dialog_window_new(parent, title ? title : "Confirm",
                                          440, 160);
    GtkWidget *box = dialog_content_new(window);
    gtk_box_append(GTK_BOX(box), dialog_label_new(title ? title : "Confirm",
                                                  TRUE));
    if (message) gtk_box_append(GTK_BOX(box), dialog_label_new(message, FALSE));

    GtkWidget *row = button_row_new();
    GtkWidget *no = cleaf_flat_button_new("No", NULL,
        G_CALLBACK(cleaf_modal_window_respond),
        GINT_TO_POINTER(GTK_RESPONSE_NO));
    GtkWidget *yes = cleaf_flat_button_new("Yes", NULL,
        G_CALLBACK(cleaf_modal_window_respond),
        GINT_TO_POINTER(GTK_RESPONSE_YES));
    gtk_box_append(GTK_BOX(row), no);
    gtk_box_append(GTK_BOX(row), yes);
    gtk_box_append(GTK_BOX(box), row);

    gboolean accepted = cleaf_modal_window_run(GTK_WINDOW(window),
                                               GTK_RESPONSE_NO)
        == GTK_RESPONSE_YES;
    cleaf_widget_destroy(window);
    return accepted;
}
