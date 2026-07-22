/**
 * @file src/app_private.h
 * @brief Internal application window helpers and action declarations.
 * @details The app window is the meeting point for most features. We keep that coordination
 *          here so editor tabs, project state, dialogs, terminals, Git, and AI do not all
 *          learn about each other directly.
 */

#ifndef GRAPTOS_APP_PRIVATE_H
#define GRAPTOS_APP_PRIVATE_H

#include "app.h"
#include "dialogs.h"
#include "ui.h"
#include "project.h"
#include "git.h"
#include "config.h"
#include "codex_client.h"
#include "codex_panel.h"
#include "codex_review.h"
#include "lsp_client.h"
#include "terminal_panel.h"

/**
 * @brief Update policy buttons.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void update_policy_buttons(EditorWindow *win);
/**
 * @brief Apply tab policy to all tabs.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void apply_tab_policy_to_all_tabs(EditorWindow *win);
/**
 * @brief App window apply css.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_apply_css(EditorWindow *win);
/**
 * @brief Apply preferences to all tabs.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void apply_preferences_to_all_tabs(EditorWindow *win);
/**
 * @brief Auto save timeout cb.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean auto_save_timeout_cb(gpointer user_data);
/**
 * @brief Restart auto save timer.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void restart_auto_save_timer(EditorWindow *win);
/**
 * @brief Register a tab in the window-owned tab registry.
 * @details The notebook only contains visible pages. Tile groups can fold tabs
 *          out of the notebook, so app-wide operations must use this registry
 *          when they mean every open tab.
 * @param win The window that owns the tab registry.
 * @param tab The tab to register.
 */
void app_window_register_tab(EditorWindow *win, EditorTab *tab);
/**
 * @brief Remove a tab from the window-owned tab registry.
 * @details This does not free the tab. Callers unregister only when the tab is
 *          really closing and no grouped owner should reference it anymore.
 * @param win The window that owns the tab registry.
 * @param tab The tab to unregister.
 */
void app_window_unregister_tab(EditorWindow *win, EditorTab *tab);
/**
 * @brief Return the number of registered tabs.
 * @details Hidden tile members are included because they are still open editor
 *          documents even when they are not visible notebook pages.
 * @param win The window whose registry is being inspected.
 * @return Number of registered tabs.
 */
guint app_window_tab_count(EditorWindow *win);
/**
 * @brief Return a registered tab by index.
 * @details This helper keeps registry iteration consistent across files that
 *          cannot see the registry implementation.
 * @param win The window whose registry is being inspected.
 * @param index Registry index.
 * @return The tab at index, or NULL.
 */
EditorTab *app_window_tab_at(EditorWindow *win, guint index);
/**
 * @brief Menu small label.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *menu_small_label(const char *text);
/**
 * @brief Build top bar.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *build_top_bar(EditorWindow *win);
/**
 * @brief Build search panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *build_search_panel(EditorWindow *win);
/**
 * @brief Build tool panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *build_tool_panel(EditorWindow *win);
/**
 * @brief Build bottom bar.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *build_bottom_bar(EditorWindow *win);
/**
 * @brief Action toggle codex panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_codex_panel(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle terminal panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_terminal_panel(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle project tree.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_project_tree(GtkWidget *widget, gpointer user_data);
/**
 * @brief Combo index for syntax.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param syntax The syntax definition used by the editor path.
 * @return The computed value requested by the caller.
 */
int combo_index_for_syntax(EditorWindow *win, SyntaxDef *syntax);
/**
 * @brief Populate syntax combo.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void populate_syntax_combo(EditorWindow *win, EditorTab *tab);
/**
 * @brief Set search panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param visible The visible supplied by the caller.
 * @param replace_mode The replace mode supplied by the caller.
 */
void set_search_panel(EditorWindow *win, gboolean visible, gboolean replace_mode);
/**
 * @brief Action new.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_new(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action open.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_open(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action open folder.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_open_folder(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action init project.
 * @details Project initialization is launched from the File menu but keeps its
 *          parser, planner, and apply engine outside the application action
 *          composition unit so it can be tested without a full window.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_init_project(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action open folder new instance.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_open_folder_new_instance(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action save.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_save(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action save as.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_save_as(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action show find.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_show_find(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action show replace.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_show_replace(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action hide search.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_hide_search(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action find next.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_find_next(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action find prev.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_find_prev(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action replace.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_replace(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action replace all.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_replace_all(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action comment.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_comment(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action reload syntax.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_reload_syntax(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action syntax diagnostics.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_syntax_diagnostics(GtkWidget *widget, gpointer user_data);
/**
 * @brief Set tab policy.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param width The width supplied by the caller.
 * @param insert_spaces The insert spaces supplied by the caller.
 */
void set_tab_policy(EditorWindow *win, guint width, gboolean insert_spaces);
/**
 * @brief Action tab spaces 2.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_tab_spaces_2(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action tab spaces 4.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_tab_spaces_4(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action tab spaces 8.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_tab_spaces_8(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action tab hard tabs.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_tab_hard_tabs(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle autocomplete.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_autocomplete(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle autosave.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_autosave(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle backup.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_backup(GtkWidget *widget, gpointer user_data);
/**
 * @brief Rgba to hex.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param rgba The rgba supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *rgba_to_hex(const GdkRGBA *rgba);
/**
 * @brief Choose color for slot.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param parent_widget The parent widget supplied by the caller.
 * @param title The title supplied by the caller.
 * @param slot The slot supplied by the caller.
 */
void choose_color_for_slot(EditorWindow *win, GtkWidget *parent_widget, const char *title, char **slot);
/**
 * @brief Action choose background.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_choose_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action choose sidebar background.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_choose_sidebar_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action choose tabbar background.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_choose_tabbar_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action choose scroll preview background.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_choose_scroll_preview_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action choose popover background.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_choose_popover_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action reset background.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_reset_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action reset all backgrounds.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_reset_all_backgrounds(GtkWidget *widget, gpointer user_data);
/**
 * @brief Pref section.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *pref_section(const char *text);
/**
 * @brief Pref tab label.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *pref_tab_label(const char *text);
/**
 * @brief Action about.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_about(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action shortcuts.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_shortcuts(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action show preferences.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_show_preferences(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action project search.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_project_search(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle minimap.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_minimap(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle preview.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_preview(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action render latex.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_render_latex(GtkWidget *widget, gpointer user_data);
/**
 * @brief On switch page.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param notebook The notebook supplied by the caller.
 * @param page The page supplied by the caller.
 * @param page_num The page num supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_switch_page(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data);
/**
 * @brief On syntax changed.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param drop_down The drop down supplied by the caller.
 * @param pspec The pspec supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_syntax_changed(GtkDropDown *drop_down,
                       GParamSpec *pspec,
                       gpointer user_data);
/**
 * @brief On window close request.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param window The window supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean on_window_close_request(GtkWindow *window, gpointer user_data);
/**
 * @brief Switch page delta.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param delta The delta supplied by the caller.
 */
void switch_page_delta(EditorWindow *win, int delta);
/**
 * @brief On window key pressed.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param controller The controller supplied by the caller.
 * @param keyval The keyval supplied by the caller.
 * @param keycode The keycode supplied by the caller.
 * @param state The state supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean on_window_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
/**
 * @brief On window key released.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param controller The controller supplied by the caller.
 * @param keyval The keyval supplied by the caller.
 * @param keycode The keycode supplied by the caller.
 * @param state The state supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_window_key_released(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

#endif /* GRAPTOS_APP_PRIVATE_H */
