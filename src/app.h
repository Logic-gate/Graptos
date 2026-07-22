/**
 * @file src/app.h
 * @brief Main application window state and public window API.
 * @details The app window is the meeting point for most features. We keep that coordination
 *          here so editor tabs, project state, dialogs, terminals, Git, and AI do not all
 *          learn about each other directly.
 */

#ifndef GRAPTOS_APP_H
#define GRAPTOS_APP_H

#include <gtk/gtk.h>
#include "version.h"
#include "editor_tab.h"
#include "syntax.h"

/**
 * @brief Graptoς display name macro.
 */
#define GRAPTOS_DISPLAY_NAME "Graptoς"

/**
 * @brief Graptoς app id macro.
 */
#define GRAPTOS_APP_ID "io.github.graptos.Editor"

/**
 * @brief App type definition.
 * @details This is intentionally a large window-owned state object. GTK code is
 *          already callback-heavy; keeping cross-feature state in one owner is
 *          easier to audit than spreading notebook, terminal, project, theme,
 *          AI, and LSP pointers through small global singletons.
 */
typedef struct _EditorWindow {
    GtkApplication *application; /**< Application. */
    GtkWidget *window; /**< Window. */
    GtkWidget *top_title_label; /**< Top title label. */
    GtkWidget *notebook; /**< Notebook. */
    GtkWidget *tile_box; /**< Active tile-mode editor box. */
    GtkWidget *project_revealer; /**< Project revealer. */
    GtkWidget *project_list; /**< Project list. */
    GHashTable *project_expanded; /**< Project expanded. */
    GHashTable *project_monitors; /**< GFileMonitor values keyed by watched project directories. */
    GHashTable *locked_paths; /**< Locked paths. */
    GHashTable *git_file_status; /**< short Git marker. */
    GtkWidget *status_label; /**< Status label. */
    char *status_error_title; /**< Latest status error title. */
    char *status_error_detail; /**< Latest status error detail. */
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
    GPtrArray *tabs; /**< EditorTab*: all open tabs, including folded tile members. */
    gboolean syntax_combo_updating; /**< Syntax combo updating. */
    gboolean controls_updating; /**< Controls updating. */
    gboolean replace_mode; /**< Replace mode. */
    guint tab_width; /**< Tab width. */
    gboolean insert_spaces; /**< Insert spaces. */
    gboolean autocomplete_enabled; /**< Autocomplete enabled. */
    gboolean regex_tester_enabled; /**< Regex tester enabled. */
    gboolean diagnostics_enabled; /**< Syntax and LSP diagnostics enabled. */
    gboolean auto_save_enabled; /**< Auto save enabled. */
    guint auto_save_interval_seconds; /**< Auto-save timer interval from config. */
    gboolean backup_enabled; /**< Backup enabled. */
    gboolean use_system_interface_font; /**< Use system interface font. */
    gboolean debug_mode; /**< Runtime debug mode enabled from the command line. */
    gboolean terminal_dynamic_directory; /**< Terminal follows active file directories. */
    gboolean tile_mode; /**< Multiple editor tabs are tiled in the active page. */
    gboolean tile_internal_switch; /**< Notebook switch originated from tile rebuild. */
    gboolean tile_shift_down; /**< Shift is currently held for tab tiling. */
    gboolean minimap_enabled; /**< Minimap enabled. */
    gboolean preview_enabled; /**< Preview enabled. */
    gboolean use_gtksourceview_highlighting; /**< GtkSourceView stays enabled. */
    gboolean use_yaml_style_overrides; /**< Use yaml style overrides. */
    char *ui_font; /**< UI font description. */
    char *editor_font; /**< Editor font description, or empty for inherited editor default. */
    char *preview_font; /**< Preview font description, or empty for inherited preview default. */
    char *terminal_font; /**< Terminal font description, or empty for VTE default. */
    char *code_font; /**< Code snippet and auxiliary code font description. */
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
    char *status_error_color; /**< Status error color. */
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
    char *git_status_modified_color; /**< Git modified indicator color. */
    char *git_status_added_color; /**< Git added indicator color. */
    char *git_status_deleted_color; /**< Git deleted indicator color. */
    char *git_status_renamed_color; /**< Git renamed indicator color. */
    char *git_status_conflict_color; /**< Git conflict indicator color. */
    char *git_status_untracked_color; /**< Git untracked indicator color. */
    char *git_status_staged_color; /**< Git staged indicator color. */
    char *scroll_preview_bg_color; /**< Scroll preview bg color. */
    char *scroll_preview_fg_color; /**< Scroll preview fg color. */
    char *popover_bg_color; /**< Popover bg color. */
    char *popover_border_color; /**< Generic popover border color. */
    char *tooltip_bg_color; /**< Tooltip background color. */
    char *tooltip_fg_color; /**< Tooltip foreground color. */
    char *tooltip_border_color; /**< Tooltip border color. */
    char *ref_popover_bg_color; /**< Ref popover bg color. */
    char *ref_popover_fg_color; /**< Ref popover fg color. */
    char *ref_popover_heading_color; /**< Ref popover heading color. */
    char *ref_popover_title_color; /**< Ref popover title color. */
    char *ref_popover_kind_color; /**< Ref popover kind color. */
    char *ref_popover_snippet_color; /**< Ref popover snippet color. */
    char *ref_popover_hover_bg_color; /**< Ref popover hover background color. */
    char *ref_popover_hover_fg_color; /**< Ref popover hover foreground color. */
    char *completion_popover_bg_color; /**< Completion popover bg color. */
    char *completion_popover_fg_color; /**< Completion popover fg color. */
    char *completion_popover_detail_color; /**< Completion popover detail color. */
    char *completion_selection_bg_color; /**< Completion selection bg color. */
    char *completion_selection_fg_color; /**< Completion selection fg color. */
    char *dialog_bg_color; /**< Dialog bg color. */
    char *dialog_fg_color; /**< Dialog fg color. */
    char *dialog_border_color; /**< Dialog border color. */
    char *dialog_title_color; /**< Dialog title color. */
    char *dialog_body_color; /**< Dialog body text color. */
    char *dialog_muted_color; /**< Dialog muted/supporting text color. */
    char *dialog_output_color; /**< Dialog output text color. */
    char *git_output_bg_color; /**< Git command output background color. */
    char *dialog_action_color; /**< Dialog action text color. */
    char *dialog_destructive_action_color; /**< Dialog destructive action text color. */
    char *dialog_input_fg_color; /**< Dialog input foreground color. */
    char *dialog_input_bg_color; /**< Dialog input background color. */
    char *search_match_bg_color; /**< Search match bg color. */
    char *search_match_fg_color; /**< Search match fg color. */
    char *diagnostic_warning_bg_color; /**< Diagnostic warning bg color. */
    char *diagnostic_warning_fg_color; /**< Diagnostic warning fg color. */
    char *codex_preview_bg_color; /**< Codex preview bg color. */
    char *codex_preview_fg_color; /**< Codex preview fg color. */
    char *codex_prompt_bg_color; /**< Codex prompt bg color. */
    guint lsp_completion_max_results; /**< Maximum LSP completion items kept for UI. */
    guint lsp_completion_max_retries; /**< Maximum retries after an empty LSP completion. */
    guint lsp_completion_retry_delay_ms; /**< Delay before retrying empty LSP completion. */
    guint lsp_references_max_results; /**< Maximum LSP reference locations rendered. */
    guint lsp_change_delay_ms; /**< Debounce delay before sending didChange to LSP. */
    char *project_root; /**< Project root. */
    GPtrArray *project_roots; /**< char*: all open project root. */
    guint project_refresh_timeout; /**< Debounced project tree filesystem refresh source. */
    guint auto_save_timeout; /**< Auto save timeout. */
    guint terminal_sync_timeout; /**< Deferred terminal-directory sync source. */
    guint tile_max_tabs; /**< Maximum tabs shown in tile mode. */
    GPtrArray *tile_tabs; /**< EditorTab*: selected tile tabs. */
    EditorTab *tile_host_tab; /**< Tab whose page currently hosts tile widgets. */
    EditorTab *tile_suppress_label_tab; /**< Tab-label click to ignore after Shift notebook switch. */
    EditorTab *active_tab; /**< Last known active notebook tab. */
    gboolean closing; /**< TRUE once the window is shutting down. */
    struct _CodexClient *codex_client; /**< Codex client. */
    struct _LspClient *lsp_client; /**< Optional language server protocol client. */
    struct _CodexPanel *codex_panel; /**< Codex panel. */
    struct _TerminalPanel *terminal_panel; /**< Integrated terminal panel. */
} EditorWindow;

/**
 * @brief App window new.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param application The application supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
EditorWindow *app_window_new(GtkApplication *application);
/**
 * @brief App window free.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_free(EditorWindow *win);
/**
 * @brief Return the number of open editor tabs.
 * @details Folded tile members are still open documents even when they are no
 *          longer visible notebook pages. Whole-window features use this count
 *          instead of treating the notebook as the document owner.
 * @param win The window whose tab registry is being inspected.
 * @return Number of open editor tabs.
 */
guint app_window_tab_count(EditorWindow *win);
/**
 * @brief Return an open editor tab by registry index.
 * @details The registry includes both visible notebook pages and folded tile
 *          members, so callers can reach every live document.
 * @param win The window whose tab registry is being inspected.
 * @param index Registry index.
 * @return The tab at index, or NULL.
 */
EditorTab *app_window_tab_at(EditorWindow *win, guint index);
/**
 * @brief App window update ui.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_update_ui(EditorWindow *win);
/**
 * @brief App window reload syntaxes.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_reload_syntaxes(EditorWindow *win);
/**
 * @brief Handle a tab-label click, including Shift-click tile selection.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param state The state supplied by the caller.
 */
void app_window_handle_tab_label_click(EditorWindow *win,
                                       EditorTab *tab,
                                       GdkModifierType state);
/**
 * @brief Handle a Shift-modified notebook switch for tile selection.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param previous The previous supplied by the caller.
 * @param switched The switched supplied by the caller.
 */
void app_window_handle_shift_tab_switch(EditorWindow *win,
                                        EditorTab *previous,
                                        EditorTab *switched);
/**
 * @brief Refresh tile mode after active tab or tab set changes.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_update_tiles(EditorWindow *win);
/**
 * @brief Leave tile mode and restore normal tab layout.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_clear_tiles(EditorWindow *win);
/**
 * @brief Restore a saved tile group for the given host tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param host The host supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean app_window_restore_saved_tile_group(EditorWindow *win, EditorTab *host);

/**
 * @brief Remove a closing tab from active and saved tile groups.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param closing_tab The closing tab supplied by the caller.
 */
void app_window_forget_tile_tab(EditorWindow *win, EditorTab *closing_tab);
/**
 * @brief App window open file.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean app_window_open_file(EditorWindow *win, const char *path);
/**
 * @brief App window current tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
EditorTab *app_window_current_tab(EditorWindow *win);
/**
 * @brief App window add tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param switch_to_tab The switch to tab supplied by the caller.
 */
void app_window_add_tab(EditorWindow *win, EditorTab *tab, gboolean switch_to_tab);
/**
 * @brief App window close tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void app_window_close_tab(EditorWindow *win, EditorTab *tab);
/**
 * @brief App window close all tabs.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean app_window_close_all_tabs(EditorWindow *win);
/**
 * @brief App window set status.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param text The text fragment supplied by the caller.
 */
void app_window_set_status(EditorWindow *win, const char *text);
/**
 * @brief App window set error status.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param short_text The short text supplied by the caller.
 * @param detail The detail supplied by the caller.
 */
void app_window_set_error_status(EditorWindow *win,
                                 const char *short_text,
                                 const char *detail);
/**
 * @brief Report an application error through status and, optionally, a dialog.
 * @details User-triggered failures need one consistent path for persistent
 *          status text and immediate detail dialogs. Keeping both outputs
 *          together prevents file, Git, and project actions from drifting into
 *          separate error handling styles.
 * @param win The window that owns the status bar and parent dialog.
 * @param short_text The short error title shown in the status bar and dialog.
 * @param detail The detailed error message shown in tooltips and dialogs.
 * @param show_dialog TRUE to show the error dialog immediately.
 */
void app_window_report_error(EditorWindow *win,
                             const char *short_text,
                             const char *detail,
                             gboolean show_dialog);
/**
 * @brief App window clear error status.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_clear_error_status(EditorWindow *win);
/**
 * @brief App window show status error.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_show_status_error(EditorWindow *win);
/**
 * @brief App window is file locked.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean app_window_is_file_locked(EditorWindow *win, const char *path);
/**
 * @brief App window set file locked.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @param locked The locked supplied by the caller.
 */
void app_window_set_file_locked(EditorWindow *win, const char *path, gboolean locked);
/**
 * @brief App window note path renamed.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param old_path The old path supplied by the caller.
 * @param new_path The new path supplied by the caller.
 */
void app_window_note_path_renamed(EditorWindow *win, const char *old_path, const char *new_path);
/**
 * @brief App window gtk.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWindow *app_window_gtk(EditorWindow *win);

#endif /* GRAPTOS_APP_H */
