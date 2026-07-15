/**
 * @file src/ui.h
 * @brief Shared GTK utility helpers and compatibility wrappers.
 */

#ifndef CLEAF_UI_H
#define CLEAF_UI_H

#include <gtk/gtk.h>

#ifndef GTK_CHECK_VERSION
#error "GTK headers are required"
#endif

#if !GTK_CHECK_VERSION(4, 10, 0)
#error "Cleaf GTK4 build requires GTK 4.10 or newer"
#endif

/**
 * @brief Cleaf apply css.
 */
void cleaf_apply_css(void);
/**
 * @brief Cleaf apply editor css.
 */
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
/**
 * @brief Cleaf button new.
 */
GtkWidget *cleaf_button_new(const char *label, const char *tooltip,
                            GCallback callback, gpointer data);
/**
 * @brief Cleaf flat button new.
 */
GtkWidget *cleaf_flat_button_new(const char *label, const char *tooltip,
                                 GCallback callback, gpointer data);
/**
 * @brief Cleaf separator new.
 */
GtkWidget *cleaf_separator_new(void);
/**
 * @brief Cleaf widget destroy.
 */
void cleaf_widget_destroy(GtkWidget *widget);
/**
 * @brief Cleaf source cancel.
 */
void cleaf_source_cancel(guint *source_id);
/**
 * @brief Cleaf box append.
 */
void cleaf_box_append(GtkWidget *box, GtkWidget *child);
/**
 * @brief Cleaf box prepend.
 */
void cleaf_box_prepend(GtkWidget *box, GtkWidget *child);
/**
 * @brief Cleaf set all margins.
 */
void cleaf_set_all_margins(GtkWidget *widget, int margin);
/**
 * @brief Cleaf modal window run.
 */
int cleaf_modal_window_run(GtkWindow *window, int default_response);
/**
 * @brief Cleaf modal window respond.
 */
void cleaf_modal_window_respond(GtkWidget *widget, gpointer user_data);
/**
 * @brief Cleaf open file dialog.
 */
char *cleaf_open_file_dialog(GtkWindow *parent, const char *title);
/**
 * @brief Cleaf save file dialog.
 */
char *cleaf_save_file_dialog(GtkWindow *parent, const char *title);
/**
 * @brief Cleaf select folder dialog.
 */
char *cleaf_select_folder_dialog(GtkWindow *parent, const char *title);
/**
 * @brief Cleaf popover attach.
 */
void cleaf_popover_attach(GtkWidget *popover, GtkWidget *parent);
/**
 * @brief Cleaf popover show.
 */
void cleaf_popover_show(GtkWidget *popover);
/**
 * @brief Cleaf popover hide.
 */
void cleaf_popover_hide(GtkWidget *popover);
/**
 * @brief Cleaf list box clear.
 */
void cleaf_list_box_clear(GtkWidget *list_box);

/**
 * @brief Gtk entry get text macro.
 */
#define gtk_entry_get_text(entry) \
    gtk_editable_get_text(GTK_EDITABLE(entry))
/**
 * @brief Gtk entry set text macro.
 */
#define gtk_entry_set_text(entry, text) \
    gtk_editable_set_text(GTK_EDITABLE(entry), text)
/**
 * @brief Gtk widget set can focus macro.
 */
#define gtk_widget_set_can_focus(widget, can_focus) \
    gtk_widget_set_focusable(widget, can_focus)

#endif /* CLEAF_UI_H */
