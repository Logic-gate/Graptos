#ifndef CLEAF_APP_H
#define CLEAF_APP_H

#include <gtk/gtk.h>
#include "editor_tab.h"
#include "syntax.h"

#ifndef APP_VERSION
#define APP_VERSION "0.16.3"
#endif

#define CLEAF_APP_ID "io.github.cleaf.Editor"

typedef struct _EditorWindow {
    GtkApplication *application;
    GtkWidget *window;
    GtkWidget *top_title_label;
    GtkWidget *notebook;
    GtkWidget *project_revealer;
    GtkWidget *project_list;
    GHashTable *project_expanded;
    GHashTable *locked_paths;
    GHashTable *git_file_status; /* short Git marker */
    GtkWidget *status_label;
    GtkWidget *syntax_combo;
    GtkWidget *indent_status_label;
    GtkWidget *indent_dropdown;
    GtkWidget *autocomplete_button;
    GtkWidget *autosave_button;
    GtkWidget *backup_button;
    GtkWidget *minimap_button;
    GtkWidget *preview_button;
    GtkWidget *syntax_override_button;
    GtkWidget *search_revealer;
    GtkWidget *tool_revealer;
    GtkWidget *tool_panel;
    GtkWidget *find_entry;
    GtkWidget *replace_label;
    GtkWidget *replace_entry;
    GtkWidget *replace_button;
    GtkWidget *replace_all_button;
    GPtrArray *syntaxes; /* Syntax definitions */
    gboolean syntax_combo_updating;
    gboolean controls_updating;
    gboolean replace_mode;
    guint tab_width;
    gboolean insert_spaces;
    gboolean autocomplete_enabled;
    gboolean auto_save_enabled;
    gboolean backup_enabled;
    gboolean use_system_interface_font;
    gboolean minimap_enabled;
    gboolean preview_enabled;
    gboolean use_gtksourceview_highlighting; /* GtkSourceView stays enabled */
    gboolean use_yaml_style_overrides;
    char *editor_bg_color;
    char *editor_fg_color;
    char *editor_gutter_bg_color;
    char *editor_gutter_fg_color;
    char *editor_current_line_bg_color;
    char *editor_selection_bg_color;
    char *editor_selection_fg_color;
    char *editor_cursor_color;
    char *sidebar_bg_color;
    char *tabbar_bg_color;
    char *tabbar_fg_color;
    char *tab_active_bg_color;
    char *tab_active_fg_color;
    char *topbar_bg_color;
    char *topbar_fg_color;
    char *bottombar_bg_color;
    char *bottombar_fg_color;
    char *button_bg_color;
    char *button_fg_color;
    char *button_hover_bg_color;
    char *button_active_bg_color;
    char *input_bg_color;
    char *input_fg_color;
    char *input_border_color;
    char *project_tree_fg_color;
    char *project_tree_selected_bg_color;
    char *project_tree_selected_fg_color;
    char *scroll_preview_bg_color;
    char *scroll_preview_fg_color;
    char *popover_bg_color;
    char *ref_popover_bg_color;
    char *ref_popover_fg_color;
    char *ref_popover_heading_color;
    char *ref_popover_title_color;
    char *ref_popover_kind_color;
    char *ref_popover_snippet_color;
    char *completion_popover_bg_color;
    char *completion_popover_fg_color;
    char *completion_popover_detail_color;
    char *completion_selection_bg_color;
    char *completion_selection_fg_color;
    char *dialog_bg_color;
    char *dialog_fg_color;
    char *dialog_border_color;
    char *dialog_title_color;
    char *search_match_bg_color;
    char *search_match_fg_color;
    char *diagnostic_warning_bg_color;
    char *diagnostic_warning_fg_color;
    char *codex_preview_bg_color;
    char *codex_preview_fg_color;
    char *codex_prompt_bg_color;
    char *project_root;
    GPtrArray *project_roots; /* char*: all open project root */
    guint auto_save_timeout;
    struct _CodexClient *codex_client;
    struct _CodexPanel *codex_panel;
} EditorWindow;

EditorWindow *app_window_new(GtkApplication *application);
void app_window_free(EditorWindow *win);
void app_window_update_ui(EditorWindow *win);
void app_window_reload_syntaxes(EditorWindow *win);
gboolean app_window_open_file(EditorWindow *win, const char *path);
EditorTab *app_window_current_tab(EditorWindow *win);
void app_window_add_tab(EditorWindow *win, EditorTab *tab, gboolean switch_to_tab);
void app_window_close_tab(EditorWindow *win, EditorTab *tab);
gboolean app_window_close_all_tabs(EditorWindow *win);
void app_window_set_status(EditorWindow *win, const char *text);
gboolean app_window_is_file_locked(EditorWindow *win, const char *path);
void app_window_set_file_locked(EditorWindow *win, const char *path, gboolean locked);
void app_window_note_path_renamed(EditorWindow *win, const char *old_path, const char *new_path);
GtkWindow *app_window_gtk(EditorWindow *win);

#endif /* CLEAF_APP_H */
