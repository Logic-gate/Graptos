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

void update_policy_buttons(EditorWindow *win);
void apply_tab_policy_to_all_tabs(EditorWindow *win);
void app_window_apply_css(EditorWindow *win);
void apply_preferences_to_all_tabs(EditorWindow *win);
gboolean auto_save_timeout_cb(gpointer user_data);
void restart_auto_save_timer(EditorWindow *win);
GtkWidget *menu_small_label(const char *text);
GtkWidget *build_top_bar(EditorWindow *win);
GtkWidget *build_search_panel(EditorWindow *win);
GtkWidget *build_tool_panel(EditorWindow *win);
GtkWidget *build_bottom_bar(EditorWindow *win);
void action_toggle_codex_panel(GtkWidget *widget, gpointer user_data);
int combo_index_for_syntax(EditorWindow *win, SyntaxDef *syntax);
void populate_syntax_combo(EditorWindow *win, EditorTab *tab);
void set_search_panel(EditorWindow *win, gboolean visible, gboolean replace_mode);
void action_new(GtkWidget *widget, gpointer user_data);
void action_open(GtkWidget *widget, gpointer user_data);
void action_open_folder(GtkWidget *widget, gpointer user_data);
void action_open_folder_new_instance(GtkWidget *widget, gpointer user_data);
void action_save(GtkWidget *widget, gpointer user_data);
void action_save_as(GtkWidget *widget, gpointer user_data);
void action_show_find(GtkWidget *widget, gpointer user_data);
void action_show_replace(GtkWidget *widget, gpointer user_data);
void action_hide_search(GtkWidget *widget, gpointer user_data);
void action_find_next(GtkWidget *widget, gpointer user_data);
void action_find_prev(GtkWidget *widget, gpointer user_data);
void action_replace(GtkWidget *widget, gpointer user_data);
void action_replace_all(GtkWidget *widget, gpointer user_data);
void action_comment(GtkWidget *widget, gpointer user_data);
void action_reload_syntax(GtkWidget *widget, gpointer user_data);
void action_syntax_diagnostics(GtkWidget *widget, gpointer user_data);
void set_tab_policy(EditorWindow *win, guint width, gboolean insert_spaces);
void action_tab_spaces_2(GtkWidget *widget, gpointer user_data);
void action_tab_spaces_4(GtkWidget *widget, gpointer user_data);
void action_tab_spaces_8(GtkWidget *widget, gpointer user_data);
void action_tab_hard_tabs(GtkWidget *widget, gpointer user_data);
void action_toggle_autocomplete(GtkWidget *widget, gpointer user_data);
void action_toggle_autosave(GtkWidget *widget, gpointer user_data);
void action_toggle_backup(GtkWidget *widget, gpointer user_data);
char *rgba_to_hex(const GdkRGBA *rgba);
void choose_color_for_slot(EditorWindow *win, GtkWidget *parent_widget, const char *title, char **slot);
void action_choose_background(GtkWidget *widget, gpointer user_data);
void action_choose_sidebar_background(GtkWidget *widget, gpointer user_data);
void action_choose_tabbar_background(GtkWidget *widget, gpointer user_data);
void action_choose_scroll_preview_background(GtkWidget *widget, gpointer user_data);
void action_choose_popover_background(GtkWidget *widget, gpointer user_data);
void action_reset_background(GtkWidget *widget, gpointer user_data);
void action_reset_all_backgrounds(GtkWidget *widget, gpointer user_data);
GtkWidget *pref_section(const char *text);
GtkWidget *pref_tab_label(const char *text);
void action_about(GtkWidget *widget, gpointer user_data);
void action_show_preferences(GtkWidget *widget, gpointer user_data);
void action_project_search(GtkWidget *widget, gpointer user_data);
void action_toggle_minimap(GtkWidget *widget, gpointer user_data);
void action_toggle_preview(GtkWidget *widget, gpointer user_data);
void action_render_latex(GtkWidget *widget, gpointer user_data);
void on_switch_page(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer user_data);
void on_syntax_changed(GtkDropDown *drop_down,
                       GParamSpec *pspec,
                       gpointer user_data);
gboolean on_window_close_request(GtkWindow *window, gpointer user_data);
void switch_page_delta(EditorWindow *win, int delta);
gboolean on_window_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);

#endif /* CLEAF_APP_PRIVATE_H */
