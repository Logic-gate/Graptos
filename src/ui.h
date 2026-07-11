#ifndef CLEAF_UI_H
#define CLEAF_UI_H

#include <gtk/gtk.h>

#ifndef GTK_CHECK_VERSION
#error "GTK headers are required"
#endif

#if !GTK_CHECK_VERSION(4, 10, 0)
#error "Cleaf GTK4 build requires GTK 4.10 or newer"
#endif

void cleaf_apply_css(void);
void cleaf_apply_editor_css(const char *editor_bg_color,
                            const char *editor_fg_color,
                            const char *editor_gutter_bg_color,
                            const char *editor_gutter_fg_color,
                            const char *editor_current_line_bg_color,
                            const char *editor_selection_bg_color,
                            const char *editor_selection_fg_color,
                            const char *editor_cursor_color,
                            const char *sidebar_bg_color,
                            const char *tabbar_bg_color,
                            const char *tabbar_fg_color,
                            const char *tab_active_bg_color,
                            const char *tab_active_fg_color,
                            const char *topbar_bg_color,
                            const char *topbar_fg_color,
                            const char *bottombar_bg_color,
                            const char *bottombar_fg_color,
                            const char *button_bg_color,
                            const char *button_fg_color,
                            const char *button_hover_bg_color,
                            const char *button_active_bg_color,
                            const char *input_bg_color,
                            const char *input_fg_color,
                            const char *input_border_color,
                            const char *project_tree_fg_color,
                            const char *project_tree_selected_bg_color,
                            const char *project_tree_selected_fg_color,
                            const char *scroll_preview_bg_color,
                            const char *scroll_preview_fg_color,
                            const char *popover_bg_color,
                            const char *ref_popover_bg_color,
                            const char *ref_popover_fg_color,
                            const char *ref_popover_heading_color,
                            const char *ref_popover_title_color,
                            const char *ref_popover_kind_color,
                            const char *ref_popover_snippet_color,
                            const char *completion_popover_bg_color,
                            const char *completion_popover_fg_color,
                            const char *completion_popover_detail_color,
                            const char *completion_selection_bg_color,
                            const char *completion_selection_fg_color,
                            const char *dialog_bg_color,
                            const char *dialog_fg_color,
                            const char *dialog_border_color,
                            const char *dialog_title_color,
                            const char *search_match_bg_color,
                            const char *search_match_fg_color,
                            const char *diagnostic_warning_bg_color,
                            const char *diagnostic_warning_fg_color,
                            const char *codex_preview_bg_color,
                            const char *codex_preview_fg_color,
                            const char *codex_prompt_bg_color,
                            gboolean use_system_interface_font);
GtkWidget *cleaf_button_new(const char *label, const char *tooltip,
                            GCallback callback, gpointer data);
GtkWidget *cleaf_flat_button_new(const char *label, const char *tooltip,
                                 GCallback callback, gpointer data);
GtkWidget *cleaf_separator_new(void);
void cleaf_widget_destroy(GtkWidget *widget);
void cleaf_source_cancel(guint *source_id);
void cleaf_box_append(GtkWidget *box, GtkWidget *child);
void cleaf_box_prepend(GtkWidget *box, GtkWidget *child);
void cleaf_set_all_margins(GtkWidget *widget, int margin);
int cleaf_modal_window_run(GtkWindow *window, int default_response);
void cleaf_modal_window_respond(GtkWidget *widget, gpointer user_data);
char *cleaf_open_file_dialog(GtkWindow *parent, const char *title);
char *cleaf_save_file_dialog(GtkWindow *parent, const char *title);
char *cleaf_select_folder_dialog(GtkWindow *parent, const char *title);
void cleaf_popover_attach(GtkWidget *popover, GtkWidget *parent);
void cleaf_popover_show(GtkWidget *popover);
void cleaf_popover_hide(GtkWidget *popover);
void cleaf_list_box_clear(GtkWidget *list_box);

#define gtk_entry_get_text(entry) \
    gtk_editable_get_text(GTK_EDITABLE(entry))
#define gtk_entry_set_text(entry, text) \
    gtk_editable_set_text(GTK_EDITABLE(entry), text)
#define gtk_widget_set_can_focus(widget, can_focus) \
    gtk_widget_set_focusable(widget, can_focus)

#endif /* CLEAF_UI_H */
