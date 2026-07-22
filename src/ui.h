/**
 * @file src/ui.h
 * @brief Shared GTK utility helpers and compatibility wrappers.
 * @details Theme values should be traceable. We translate config into GTK CSS here so
 *          colors and fonts do not get scattered through feature code where they are hard
 *          to reason about later.
 */

#ifndef GRAPTOS_UI_H
#define GRAPTOS_UI_H

#include <gtk/gtk.h>

#ifndef GTK_CHECK_VERSION
#error "GTK headers are required"
#endif

#if !GTK_CHECK_VERSION(4, 10, 0)
#error "Graptoς GTK4 build requires GTK 4.10 or newer"
#endif

/**
 * @brief Graptoς apply css.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 */
void graptos_apply_css(void);
/**
 * @brief Graptoς apply editor css.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param editor_bg_color The editor bg color supplied by the caller.
 * @param editor_fg_color The editor fg color supplied by the caller.
 * @param editor_gutter_bg_color The editor gutter bg color supplied by the caller.
 * @param editor_gutter_fg_color The editor gutter fg color supplied by the caller.
 * @param editor_current_line_bg_color The editor current line bg color supplied by the caller.
 * @param editor_selection_bg_color The editor selection bg color supplied by the caller.
 * @param editor_selection_fg_color The editor selection fg color supplied by the caller.
 * @param editor_cursor_color The editor cursor color supplied by the caller.
 * @param sidebar_bg_color The sidebar bg color supplied by the caller.
 * @param tabbar_bg_color The tabbar bg color supplied by the caller.
 * @param tabbar_fg_color The tabbar fg color supplied by the caller.
 * @param tab_active_bg_color The tab active bg color supplied by the caller.
 * @param tab_active_fg_color The tab active fg color supplied by the caller.
 * @param topbar_bg_color The topbar bg color supplied by the caller.
 * @param topbar_fg_color The topbar fg color supplied by the caller.
 * @param bottombar_bg_color The bottombar bg color supplied by the caller.
 * @param bottombar_fg_color The bottombar fg color supplied by the caller.
 * @param status_error_color The status error color supplied by the caller.
 * @param button_bg_color The button bg color supplied by the caller.
 * @param button_fg_color The button fg color supplied by the caller.
 * @param button_hover_bg_color The button hover bg color supplied by the caller.
 * @param button_active_bg_color The button active bg color supplied by the caller.
 * @param input_bg_color The input bg color supplied by the caller.
 * @param input_fg_color The input fg color supplied by the caller.
 * @param input_border_color The input border color supplied by the caller.
 * @param project_tree_fg_color The project tree fg color supplied by the caller.
 * @param project_tree_selected_bg_color The project tree selected bg color supplied by the caller.
 * @param project_tree_selected_fg_color The project tree selected fg color supplied by the caller.
 * @param git_status_modified_color The git status modified color supplied by the caller.
 * @param git_status_added_color The git status added color supplied by the caller.
 * @param git_status_deleted_color The git status deleted color supplied by the caller.
 * @param git_status_renamed_color The git status renamed color supplied by the caller.
 * @param git_status_conflict_color The git status conflict color supplied by the caller.
 * @param git_status_untracked_color The git status untracked color supplied by the caller.
 * @param git_status_staged_color The git status staged color supplied by the caller.
 * @param scroll_preview_bg_color The scroll preview bg color supplied by the caller.
 * @param scroll_preview_fg_color The scroll preview fg color supplied by the caller.
 * @param popover_bg_color The popover bg color supplied by the caller.
 * @param popover_border_color The popover border color supplied by the caller.
 * @param tooltip_bg_color The tooltip background color supplied by the caller.
 * @param tooltip_fg_color The tooltip foreground color supplied by the caller.
 * @param tooltip_border_color The tooltip border color supplied by the caller.
 * @param ref_popover_bg_color The ref popover bg color supplied by the caller.
 * @param ref_popover_fg_color The ref popover fg color supplied by the caller.
 * @param ref_popover_heading_color The ref popover heading color supplied by the caller.
 * @param ref_popover_title_color The ref popover title color supplied by the caller.
 * @param ref_popover_kind_color The ref popover kind color supplied by the caller.
 * @param ref_popover_snippet_color The ref popover snippet color supplied by the caller.
 * @param ref_popover_hover_bg_color The ref popover hover bg color supplied by the caller.
 * @param ref_popover_hover_fg_color The ref popover hover fg color supplied by the caller.
 * @param completion_popover_bg_color The completion popover bg color supplied by the caller.
 * @param completion_popover_fg_color The completion popover fg color supplied by the caller.
 * @param completion_popover_detail_color The completion popover detail color supplied by the caller.
 * @param completion_selection_bg_color The completion selection bg color supplied by the caller.
 * @param completion_selection_fg_color The completion selection fg color supplied by the caller.
 * @param dialog_bg_color The dialog bg color supplied by the caller.
 * @param dialog_fg_color The dialog fg color supplied by the caller.
 * @param dialog_border_color The dialog border color supplied by the caller.
 * @param dialog_title_color The dialog title color supplied by the caller.
 * @param dialog_body_color The dialog body color supplied by the caller.
 * @param dialog_muted_color The dialog muted color supplied by the caller.
 * @param dialog_output_color The dialog output color supplied by the caller.
 * @param git_output_bg_color The git output background color supplied by the caller.
 * @param dialog_action_color The dialog action color supplied by the caller.
 * @param dialog_destructive_action_color The dialog destructive action color supplied by the caller.
 * @param dialog_input_fg_color The dialog input foreground color supplied by the caller.
 * @param dialog_input_bg_color The dialog input background color supplied by the caller.
 * @param search_match_bg_color The search match bg color supplied by the caller.
 * @param search_match_fg_color The search match fg color supplied by the caller.
 * @param diagnostic_warning_bg_color The diagnostic warning bg color supplied by the caller.
 * @param diagnostic_warning_fg_color The diagnostic warning fg color supplied by the caller.
 * @param codex_preview_bg_color The codex preview bg color supplied by the caller.
 * @param codex_preview_fg_color The codex preview fg color supplied by the caller.
 * @param codex_prompt_bg_color The codex prompt bg color supplied by the caller.
 * @param ui_font The ui font supplied by the caller.
 * @param editor_font The editor font supplied by the caller.
 * @param preview_font The preview font supplied by the caller.
 * @param terminal_font The terminal font supplied by the caller.
 * @param code_font The code font supplied by the caller.
 * @param use_system_interface_font The use system interface font supplied by the caller.
 */
void graptos_apply_editor_css(const char *editor_bg_color,
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
                            const char *status_error_color,
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
                            const char *git_status_modified_color,
                            const char *git_status_added_color,
                            const char *git_status_deleted_color,
                            const char *git_status_renamed_color,
                            const char *git_status_conflict_color,
                            const char *git_status_untracked_color,
                            const char *git_status_staged_color,
                            const char *scroll_preview_bg_color,
                            const char *scroll_preview_fg_color,
                            const char *popover_bg_color,
                            const char *popover_border_color,
                            const char *tooltip_bg_color,
                            const char *tooltip_fg_color,
                            const char *tooltip_border_color,
                            const char *ref_popover_bg_color,
                            const char *ref_popover_fg_color,
                            const char *ref_popover_heading_color,
                            const char *ref_popover_title_color,
                            const char *ref_popover_kind_color,
                            const char *ref_popover_snippet_color,
                            const char *ref_popover_hover_bg_color,
                            const char *ref_popover_hover_fg_color,
                            const char *completion_popover_bg_color,
                            const char *completion_popover_fg_color,
                            const char *completion_popover_detail_color,
                            const char *completion_selection_bg_color,
                            const char *completion_selection_fg_color,
                            const char *dialog_bg_color,
                            const char *dialog_fg_color,
                            const char *dialog_border_color,
                            const char *dialog_title_color,
                            const char *dialog_body_color,
                            const char *dialog_muted_color,
                            const char *dialog_output_color,
                            const char *git_output_bg_color,
                            const char *dialog_action_color,
                            const char *dialog_destructive_action_color,
                            const char *dialog_input_fg_color,
                            const char *dialog_input_bg_color,
                            const char *search_match_bg_color,
                            const char *search_match_fg_color,
                            const char *diagnostic_warning_bg_color,
                            const char *diagnostic_warning_fg_color,
                            const char *codex_preview_bg_color,
                            const char *codex_preview_fg_color,
                            const char *codex_prompt_bg_color,
                            const char *ui_font,
                            const char *editor_font,
                            const char *preview_font,
                            const char *terminal_font,
                            const char *code_font,
                            gboolean use_system_interface_font);
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
                            GCallback callback, gpointer data);
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
                                 GCallback callback, gpointer data);
/**
 * @brief Graptoς separator new.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *graptos_separator_new(void);
/**
 * @brief Graptoς widget destroy.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param widget The widget that emitted the callback or receives the update.
 */
void graptos_widget_destroy(GtkWidget *widget);
/**
 * @brief Graptoς source cancel.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param source_id The source id supplied by the caller.
 */
void graptos_source_cancel(guint *source_id);
/**
 * @brief Graptoς box append.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param box The box supplied by the caller.
 * @param child The child supplied by the caller.
 */
void graptos_box_append(GtkWidget *box, GtkWidget *child);
/**
 * @brief Graptoς box prepend.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param box The box supplied by the caller.
 * @param child The child supplied by the caller.
 */
void graptos_box_prepend(GtkWidget *box, GtkWidget *child);
/**
 * @brief Graptoς set all margins.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param widget The widget that emitted the callback or receives the update.
 * @param margin The margin supplied by the caller.
 */
void graptos_set_all_margins(GtkWidget *widget, int margin);
/**
 * @brief Graptoς modal window run.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param window The window supplied by the caller.
 * @param default_response The default response supplied by the caller.
 * @return The computed value requested by the caller.
 */
int graptos_modal_window_run(GtkWindow *window, int default_response);
/**
 * @brief Graptoς modal window respond.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void graptos_modal_window_respond(GtkWidget *widget, gpointer user_data);
/**
 * @brief Graptoς open file dialog.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_open_file_dialog(GtkWindow *parent, const char *title);
/**
 * @brief Graptoς save file dialog.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_save_file_dialog(GtkWindow *parent, const char *title);
/**
 * @brief Graptoς select folder dialog.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_select_folder_dialog(GtkWindow *parent, const char *title);
/**
 * @brief Graptoς popover attach.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param popover The popover supplied by the caller.
 * @param parent The parent supplied by the caller.
 */
void graptos_popover_attach(GtkWidget *popover, GtkWidget *parent);
/**
 * @brief Graptoς popover show.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param popover The popover supplied by the caller.
 */
void graptos_popover_show(GtkWidget *popover);
/**
 * @brief Graptoς popover hide.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param popover The popover supplied by the caller.
 */
void graptos_popover_hide(GtkWidget *popover);
/**
 * @brief Graptoς list box clear.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param list_box The list box supplied by the caller.
 */
void graptos_list_box_clear(GtkWidget *list_box);

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

#endif /* GRAPTOS_UI_H */
