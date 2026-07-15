/**
 * @file src/app.h
 * @brief Main application window state and public window API.
 */

#ifndef CLEAF_APP_H
#define CLEAF_APP_H

#include <gtk/gtk.h>
#include "editor_tab.h"
#include "syntax.h"

#ifndef APP_VERSION
#define APP_VERSION "0.16.3"
#endif

/**
 * @brief Cleaf app id macro.
 */
#define CLEAF_APP_ID "io.github.cleaf.Editor"

/**
 * @brief App type definition.
 */
typedef struct _EditorWindow {
    GtkApplication *application; /**< Application. */
    GtkWidget *window; /**< Window. */
    GtkWidget *top_title_label; /**< Top title label. */
    GtkWidget *notebook; /**< Notebook. */
    GtkWidget *project_revealer; /**< Project revealer. */
    GtkWidget *project_list; /**< Project list. */
    GHashTable *project_expanded; /**< Project expanded. */
    GHashTable *locked_paths; /**< Locked paths. */
    GHashTable *git_file_status; /**< short Git marker. */
    GtkWidget *status_label; /**< Status label. */
    GtkWidget *syntax_combo; /**< Syntax combo. */
    GtkWidget *indent_status_label; /**< Indent status label. */
    GtkWidget *indent_dropdown; /**< Indent dropdown. */
    GtkWidget *autocomplete_button; /**< Autocomplete button. */
    GtkWidget *autosave_button; /**< Autosave button. */
    GtkWidget *backup_button; /**< Backup button. */
    GtkWidget *minimap_button; /**< Minimap button. */
    GtkWidget *preview_button; /**< Preview button. */
    GtkWidget *syntax_override_button; /**< Syntax override button. */
    GtkWidget *search_revealer; /**< Search revealer. */
    GtkWidget *tool_revealer; /**< Tool revealer. */
    GtkWidget *tool_panel; /**< Tool panel. */
    GtkWidget *find_entry; /**< Find entry. */
    GtkWidget *replace_label; /**< Replace label. */
    GtkWidget *replace_entry; /**< Replace entry. */
    GtkWidget *replace_button; /**< Replace button. */
    GtkWidget *replace_all_button; /**< Replace all button. */
    GPtrArray *syntaxes; /**< Syntax definitions. */
    gboolean syntax_combo_updating; /**< Syntax combo updating. */
    gboolean controls_updating; /**< Controls updating. */
    gboolean replace_mode; /**< Replace mode. */
    guint tab_width; /**< Tab width. */
    gboolean insert_spaces; /**< Insert spaces. */
    gboolean autocomplete_enabled; /**< Autocomplete enabled. */
    gboolean auto_save_enabled; /**< Auto save enabled. */
    gboolean backup_enabled; /**< Backup enabled. */
    gboolean use_system_interface_font; /**< Use system interface font. */
    gboolean minimap_enabled; /**< Minimap enabled. */
    gboolean preview_enabled; /**< Preview enabled. */
    gboolean use_gtksourceview_highlighting; /**< GtkSourceView stays enabled. */
    gboolean use_yaml_style_overrides; /**< Use yaml style overrides. */
    char *editor_bg_color; /**< Editor bg color. */
    char *editor_fg_color; /**< Editor fg color. */
    char *editor_gutter_bg_color; /**< Editor gutter bg color. */
    char *editor_gutter_fg_color; /**< Editor gutter fg color. */
    char *editor_current_line_bg_color; /**< Editor current line bg color. */
    char *editor_selection_bg_color; /**< Editor selection bg color. */
    char *editor_selection_fg_color; /**< Editor selection fg color. */
    char *editor_cursor_color; /**< Editor cursor color. */
    char *sidebar_bg_color; /**< Sidebar bg color. */
    char *tabbar_bg_color; /**< Tabbar bg color. */
    char *tabbar_fg_color; /**< Tabbar fg color. */
    char *tab_active_bg_color; /**< Tab active bg color. */
    char *tab_active_fg_color; /**< Tab active fg color. */
    char *topbar_bg_color; /**< Topbar bg color. */
    char *topbar_fg_color; /**< Topbar fg color. */
    char *bottombar_bg_color; /**< Bottombar bg color. */
    char *bottombar_fg_color; /**< Bottombar fg color. */
    char *button_bg_color; /**< Button bg color. */
    char *button_fg_color; /**< Button fg color. */
    char *button_hover_bg_color; /**< Button hover bg color. */
    char *button_active_bg_color; /**< Button active bg color. */
    char *input_bg_color; /**< Input bg color. */
    char *input_fg_color; /**< Input fg color. */
    char *input_border_color; /**< Input border color. */
    char *project_tree_fg_color; /**< Project tree fg color. */
    char *project_tree_selected_bg_color; /**< Project tree selected bg color. */
    char *project_tree_selected_fg_color; /**< Project tree selected fg color. */
    char *scroll_preview_bg_color; /**< Scroll preview bg color. */
    char *scroll_preview_fg_color; /**< Scroll preview fg color. */
    char *popover_bg_color; /**< Popover bg color. */
    char *ref_popover_bg_color; /**< Ref popover bg color. */
    char *ref_popover_fg_color; /**< Ref popover fg color. */
    char *ref_popover_heading_color; /**< Ref popover heading color. */
    char *ref_popover_title_color; /**< Ref popover title color. */
    char *ref_popover_kind_color; /**< Ref popover kind color. */
    char *ref_popover_snippet_color; /**< Ref popover snippet color. */
    char *completion_popover_bg_color; /**< Completion popover bg color. */
    char *completion_popover_fg_color; /**< Completion popover fg color. */
    char *completion_popover_detail_color; /**< Completion popover detail color. */
    char *completion_selection_bg_color; /**< Completion selection bg color. */
    char *completion_selection_fg_color; /**< Completion selection fg color. */
    char *dialog_bg_color; /**< Dialog bg color. */
    char *dialog_fg_color; /**< Dialog fg color. */
    char *dialog_border_color; /**< Dialog border color. */
    char *dialog_title_color; /**< Dialog title color. */
    char *search_match_bg_color; /**< Search match bg color. */
    char *search_match_fg_color; /**< Search match fg color. */
    char *diagnostic_warning_bg_color; /**< Diagnostic warning bg color. */
    char *diagnostic_warning_fg_color; /**< Diagnostic warning fg color. */
    char *codex_preview_bg_color; /**< Codex preview bg color. */
    char *codex_preview_fg_color; /**< Codex preview fg color. */
    char *codex_prompt_bg_color; /**< Codex prompt bg color. */
    char *project_root; /**< Project root. */
    GPtrArray *project_roots; /**< char*: all open project root. */
    guint auto_save_timeout; /**< Auto save timeout. */
    struct _CodexClient *codex_client; /**< Codex client. */
    struct _CodexPanel *codex_panel; /**< Codex panel. */
} EditorWindow;

/**
 * @brief App window new.
 */
EditorWindow *app_window_new(GtkApplication *application);
/**
 * @brief App window free.
 */
void app_window_free(EditorWindow *win);
/**
 * @brief App window update ui.
 */
void app_window_update_ui(EditorWindow *win);
/**
 * @brief App window reload syntaxes.
 */
void app_window_reload_syntaxes(EditorWindow *win);
/**
 * @brief App window open file.
 */
gboolean app_window_open_file(EditorWindow *win, const char *path);
/**
 * @brief App window current tab.
 */
EditorTab *app_window_current_tab(EditorWindow *win);
/**
 * @brief App window add tab.
 */
void app_window_add_tab(EditorWindow *win, EditorTab *tab, gboolean switch_to_tab);
/**
 * @brief App window close tab.
 */
void app_window_close_tab(EditorWindow *win, EditorTab *tab);
/**
 * @brief App window close all tabs.
 */
gboolean app_window_close_all_tabs(EditorWindow *win);
/**
 * @brief App window set status.
 */
void app_window_set_status(EditorWindow *win, const char *text);
/**
 * @brief App window is file locked.
 */
gboolean app_window_is_file_locked(EditorWindow *win, const char *path);
/**
 * @brief App window set file locked.
 */
void app_window_set_file_locked(EditorWindow *win, const char *path, gboolean locked);
/**
 * @brief App window note path renamed.
 */
void app_window_note_path_renamed(EditorWindow *win, const char *old_path, const char *new_path);
/**
 * @brief App window gtk.
 */
GtkWindow *app_window_gtk(EditorWindow *win);

#endif /* CLEAF_APP_H */
