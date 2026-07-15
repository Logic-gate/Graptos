/**
 * @file src/app_private.h
 * @brief Internal application window helpers and action declarations.
 */

#ifndef CLEAF_APP_PRIVATE_H
#define CLEAF_APP_PRIVATE_H

#include "app.h"
#include "dialogs.h"
#include "ui.h"
#include "project.h"
#include "git.h"
#include "config.h"
#include "codex_client.h"
#include "codex_panel.h"
#include "codex_review.h"

/**
 * @brief Update policy buttons.
 */
void update_policy_buttons(EditorWindow *win);
/**
 * @brief Apply tab policy to all tabs.
 */
void apply_tab_policy_to_all_tabs(EditorWindow *win);
/**
 * @brief App window apply css.
 */
void app_window_apply_css(EditorWindow *win);
/**
 * @brief Apply preferences to all tabs.
 */
void apply_preferences_to_all_tabs(EditorWindow *win);
/**
 * @brief Auto save timeout cb.
 */
gboolean auto_save_timeout_cb(gpointer user_data);
/**
 * @brief Restart auto save timer.
 */
void restart_auto_save_timer(EditorWindow *win);
/**
 * @brief Menu small label.
 */
GtkWidget *menu_small_label(const char *text);
/**
 * @brief Build top bar.
 */
GtkWidget *build_top_bar(EditorWindow *win);
/**
 * @brief Build search panel.
 */
GtkWidget *build_search_panel(EditorWindow *win);
/**
 * @brief Build tool panel.
 */
GtkWidget *build_tool_panel(EditorWindow *win);
/**
 * @brief Build bottom bar.
 */
GtkWidget *build_bottom_bar(EditorWindow *win);
/**
 * @brief Action toggle codex panel.
 */
void action_toggle_codex_panel(GtkWidget *widget, gpointer user_data);
/**
 * @brief Combo index for syntax.
 */
int combo_index_for_syntax(EditorWindow *win, SyntaxDef *syntax);
/**
 * @brief Populate syntax combo.
 */
void populate_syntax_combo(EditorWindow *win, EditorTab *tab);
/**
 * @brief Set search panel.
 */
void set_search_panel(EditorWindow *win, gboolean visible, gboolean replace_mode);
/**
 * @brief Action new.
 */
void action_new(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action open.
 */
void action_open(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action open folder.
 */
void action_open_folder(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action open folder new instance.
 */
void action_open_folder_new_instance(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action save.
 */
void action_save(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action save as.
 */
void action_save_as(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action show find.
 */
void action_show_find(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action show replace.
 */
void action_show_replace(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action hide search.
 */
void action_hide_search(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action find next.
 */
void action_find_next(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action find prev.
 */
void action_find_prev(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action replace.
 */
void action_replace(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action replace all.
 */
void action_replace_all(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action comment.
 */
void action_comment(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action reload syntax.
 */
void action_reload_syntax(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action syntax diagnostics.
 */
void action_syntax_diagnostics(GtkWidget *widget, gpointer user_data);
/**
 * @brief Set tab policy.
 */
void set_tab_policy(EditorWindow *win, guint width, gboolean insert_spaces);
/**
 * @brief Action tab spaces 2.
 */
void action_tab_spaces_2(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action tab spaces 4.
 */
void action_tab_spaces_4(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action tab spaces 8.
 */
void action_tab_spaces_8(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action tab hard tabs.
 */
void action_tab_hard_tabs(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle autocomplete.
 */
void action_toggle_autocomplete(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle autosave.
 */
void action_toggle_autosave(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle backup.
 */
void action_toggle_backup(GtkWidget *widget, gpointer user_data);
/**
 * @brief Rgba to hex.
 */
char *rgba_to_hex(const GdkRGBA *rgba);
/**
 * @brief Choose color for slot.
 */
void choose_color_for_slot(EditorWindow *win, GtkWidget *parent_widget, const char *title, char **slot);
/**
 * @brief Action choose background.
 */
void action_choose_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action choose sidebar background.
 */
void action_choose_sidebar_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action choose tabbar background.
 */
void action_choose_tabbar_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action choose scroll preview background.
 */
void action_choose_scroll_preview_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action choose popover background.
 */
void action_choose_popover_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action reset background.
 */
void action_reset_background(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action reset all backgrounds.
 */
void action_reset_all_backgrounds(GtkWidget *widget, gpointer user_data);
/**
 * @brief Pref section.
 */
GtkWidget *pref_section(const char *text);
/**
 * @brief Pref tab label.
 */
GtkWidget *pref_tab_label(const char *text);
/**
 * @brief Action about.
 */
void action_about(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action show preferences.
 */
void action_show_preferences(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action project search.
 */
void action_project_search(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle minimap.
 */
void action_toggle_minimap(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action toggle preview.
 */
void action_toggle_preview(GtkWidget *widget, gpointer user_data);
/**
 * @brief Action render latex.
 */
void action_render_latex(GtkWidget *widget, gpointer user_data);
/**
 * @brief On switch page.
 */
void on_switch_page(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data);
/**
 * @brief On syntax changed.
 */
void on_syntax_changed(GtkDropDown *drop_down,
                       GParamSpec *pspec,
                       gpointer user_data);
/**
 * @brief On window close request.
 */
gboolean on_window_close_request(GtkWindow *window, gpointer user_data);
/**
 * @brief Switch page delta.
 */
void switch_page_delta(EditorWindow *win, int delta);
/**
 * @brief On window key pressed.
 */
gboolean on_window_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

#endif /* CLEAF_APP_PRIVATE_H */
