/**
 * @file src/editor_tab.h
 * @brief Editor tab public API and state structure.
 * @details Editor tabs hold the real editing surface. We split the implementation by
 *          lifecycle, input, rendering, preview, and transient UI because each part has
 *          different timing and cleanup pressure.
 */

#ifndef GRAPTOS_EDITOR_TAB_H
#define GRAPTOS_EDITOR_TAB_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include "syntax.h"
#include "completion.h"

/**
 * @brief Editor tab type definition.
 */
typedef struct _EditorWindow EditorWindow;

/**
 * @brief Editor tab type definition.
 */
typedef struct _EditorTab EditorTab;

/**
 * @brief Editor diagnostic range and message.
 */
typedef struct {
    gint start_offset; /**< Inclusive character offset. */
    gint end_offset; /**< Exclusive character offset. */
    char *message; /**< Human-readable diagnostic message. */
} EditorDiagnostic;

/**
 * @brief Editor tab type definition.
 * @details A tab owns the real buffer and all UI that follows that buffer:
 *          completion, hover, preview, minimap, diagnostics, and timers. The
 *          window can move the widgets around, but the editing state stays here.
 */
struct _EditorTab {
    EditorWindow *win; /**< Win. */
    GtkWidget *box; /**< Box. */
    GtkWidget *editor_area; /**< Editor area. */
    GtkWidget *gutter; /**< Gutter. */
    GtkWidget *scrolled; /**< Scrolled. */
    GtkWidget *text_view; /**< Text view. */
    GtkWidget *popover_parent; /**< Popover parent. */
    GtkTextBuffer *buffer; /**< Buffer. */
    GtkSourceBuffer *source_buffer; /**< Source buffer. */
    GtkWidget *completion_popover; /**< Completion popover. */
    GtkWidget *completion_list; /**< Completion list. */
    GtkWidget *completion_scrolled; /**< Completion scrolled. */
    GtkWidget *hover_popover; /**< Hover popover. */
    GtkWidget *hover_list; /**< Hover list. */
    GtkWidget *minimap_scrolled; /**< Minimap scrolled. */
    GtkWidget *minimap_view; /**< Minimap view. */
    GtkTextBuffer *minimap_buffer; /**< Minimap buffer. */
    GtkWidget *editor_content; /**< Editor and minimap row inside the preview splitter. */
    GtkWidget *preview_paned; /**< Resizable editor/preview splitter. */
    GtkWidget *preview_box; /**< Preview container with header controls. */
    GtkWidget *preview_detach_button; /**< Preview detach or reattach button. */
    GtkWidget *preview_window; /**< Detached preview window, or NULL while attached. */
    GtkWidget *preview_scrolled; /**< Preview scrolled. */
    GtkWidget *preview_view; /**< Preview view. */
    GtkTextBuffer *preview_buffer; /**< Preview buffer. */
    GtkWidget *tab_widget; /**< Tab widget. */
    GtkWidget *tab_lock_icon; /**< Tab lock icon. */
    GtkWidget *tab_title; /**< Tab title. */

    char *file_path; /**< File path. */
    char *display_title; /**< Generated tab title used when no file path exists. */
    char *display_title_markup; /**< Optional generated tab title markup for the tab label. */
    char *last_snapshot; /**< Last snapshot. */
    char *kill_buffer; /**< Kill buffer. */
    char *search_text; /**< Search text. */
    char *completion_prefix; /**< Completion prefix. */
    char *lsp_completion_retry_key; /**< Active LSP completion retry key. */
    char *hover_word; /**< Hover word. */
    SyntaxDef *active_syntax; /**< Active syntax. */

    GPtrArray *undo_stack; /**< char*. */
    GPtrArray *redo_stack; /**< char*. */
    GPtrArray *tile_group; /**< EditorTab*: saved tile group owned by this tab. */
    GPtrArray *color_literal_tag_names; /**< Color literal tag names. */
    GPtrArray *diagnostics; /**< EditorDiagnostic*. */
    guint highlight_timeout; /**< Highlight timeout. */
    guint completion_timeout; /**< Completion timeout. */
    guint minimap_timeout; /**< Minimap timeout. */
    guint preview_timeout; /**< Preview timeout. */
    guint diagnostics_timeout; /**< Diagnostics timeout. */
    guint selection_match_timeout; /**< Selection match timeout. */
    guint color_literal_timeout; /**< Color literal highlight timeout. */
    guint regex_tester_timeout; /**< Regex tester timeout. */
    guint lsp_change_timeout; /**< Debounced LSP didChange timeout. */
    guint lsp_completion_retry_timeout; /**< Delayed LSP completion retry timeout. */
    guint preview_reparent_idle; /**< Deferred preview attach/detach source id. */
    guint lsp_completion_retry_count; /**< Number of retries for the active LSP completion key. */
    guint hover_timeout; /**< Hover timeout. */
    guint hover_hide_timeout; /**< Hover hide timeout. */
    guint hover_lsp_fallback_timeout; /**< Hover LSP fallback timeout. */
    guint ui_refresh_timeout; /**< Ui refresh timeout. */
    guint tab_width; /**< Tab width. */
    guint diagnostic_warnings; /**< Diagnostic warnings. */
    guint cached_gutter_width; /**< Cached gutter width. */
    guint lsp_version; /**< LSP document version. */
    guint hover_lsp_line; /**< Hover LSP line for reference lookup. */
    guint hover_lsp_character; /**< Hover LSP character for reference lookup. */
    guint hover_lsp_serial; /**< Hover LSP request serial. */
    GdkRGBA color_preview_rgba; /**< Color preview rgba. */
    gdouble pointer_x; /**< Pointer x. */
    gdouble pointer_y; /**< Pointer y. */
    gboolean modified; /**< Modified. */
    gboolean applying_change; /**< Applying change. */
    gboolean disposing; /**< TRUE once tab teardown has started. */
    gboolean manual_syntax_override; /**< Manual syntax override. */
    gboolean backup_enabled; /**< Backup enabled. */
    gboolean wrap_enabled; /**< Wrap enabled. */
    gboolean autocomplete_enabled; /**< Autocomplete enabled. */
    gboolean completion_manual; /**< Completion manual. */
    gboolean completion_refreshing_from_lsp; /**< Completion is rebuilding from an LSP response. */
    gboolean insert_spaces; /**< Insert spaces. */
    gboolean color_preview_valid; /**< Color preview valid. */
    gboolean regex_tester_active; /**< Regex tester popover active. */
    gboolean hover_pointer_inside; /**< Hover pointer inside. */
    gboolean hover_pinned; /**< Hover pinned. */
    gboolean inspect_reference_active; /**< Inspect reference active. */
    gboolean minimap_updating; /**< Minimap updating. */
    gboolean preview_updating; /**< Preview updating. */
    gboolean preview_detached; /**< TRUE while the preview container lives in its own window. */
    gboolean locked; /**< Locked. */
    gboolean folded_tile_member; /**< TRUE when this tab is owned by a visible tile group host. */
    gboolean notebook_refs_held; /**< TRUE when folded widgets are reffed outside the notebook. */
    gboolean hover_popover_locked; /**< Hover popover locked. */
    gboolean pointer_valid; /**< Pointer valid. */
    gboolean hover_lsp_position_valid; /**< Hover LSP position valid. */
    gboolean selection_matches_active; /**< Selection matches active. */
    gboolean color_literals_active; /**< Color literals active. */
    gboolean diagnostics_active; /**< Diagnostics active. */
    gboolean custom_highlight_active; /**< Custom highlight active. */
    gboolean low_latency_mode_active; /**< Low latency mode active. */
};

/**
 * @brief Editor tab new.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
EditorTab *editor_tab_new(EditorWindow *win);
/**
 * @brief Editor tab destroy popovers.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_destroy_popovers(EditorTab *tab);
/**
 * @brief Editor tab free.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_free(EditorTab *tab);
/**
 * @brief Editor tab load file.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_load_file(EditorTab *tab, const char *path);
/**
 * @brief Editor tab save.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param force_dialog The force dialog supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_save(EditorTab *tab, gboolean force_dialog);
/**
 * @brief Editor tab confirm close.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_confirm_close(EditorTab *tab);
/**
 * @brief Editor tab update title.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_update_title(EditorTab *tab);
/**
 * @brief Set a generated display title for a tab.
 * @details Generated tabs, such as Git diff and Codex review tabs, do not have
 *          a file path. Keeping their title in the tab state prevents normal UI
 *          refreshes from rebuilding the title as Untitled.
 * @param tab The editor tab whose title should be overridden.
 * @param plain_title Plain text title used by status bars and window titles.
 * @param markup_title Optional Pango markup used by the tab label.
 */
void editor_tab_set_display_title(EditorTab *tab,
                                  const char *plain_title,
                                  const char *markup_title);
/**
 * @brief Editor tab update status.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_update_status(EditorTab *tab);
/**
 * @brief Editor tab set locked.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param locked The locked supplied by the caller.
 */
void editor_tab_set_locked(EditorTab *tab, gboolean locked);
/**
 * @brief Editor tab is locked.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_is_locked(EditorTab *tab);
/**
 * @brief Editor tab set syntax.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param syntax The syntax definition used by the editor path.
 * @param manual The manual supplied by the caller.
 */
void editor_tab_set_syntax(EditorTab *tab, SyntaxDef *syntax, gboolean manual);
/**
 * @brief Editor tab auto select syntax.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_auto_select_syntax(EditorTab *tab);
/**
 * @brief Editor tab schedule highlight.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_highlight(EditorTab *tab);
/**
 * @brief Editor tab apply highlight.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_apply_highlight(EditorTab *tab);
/**
 * @brief Editor tab update highlight engine.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_update_highlight_engine(EditorTab *tab);
/**
 * @brief Editor tab find.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param query The query supplied by the caller.
 * @param backwards The backwards supplied by the caller.
 */
void editor_tab_find(EditorTab *tab, const char *query, gboolean backwards);
/**
 * @brief Editor tab replace current.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param find The find supplied by the caller.
 * @param replace The replace supplied by the caller.
 */
void editor_tab_replace_current(EditorTab *tab, const char *find, const char *replace);
/**
 * @brief Editor tab replace all.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param find The find supplied by the caller.
 * @param replace The replace supplied by the caller.
 */
void editor_tab_replace_all(EditorTab *tab, const char *find, const char *replace);
/**
 * @brief Editor tab toggle comment.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_toggle_comment(EditorTab *tab);
/**
 * @brief Editor tab go to line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_go_to_line(EditorTab *tab);
/**
 * @brief Format the selected code or surrounding syntax block.
 * @details Formatting is syntax-aware and applies as one undoable buffer edit.
 * @param tab The editor tab whose buffer should be formatted.
 */
void editor_tab_format_code(EditorTab *tab);
/**
 * @brief Editor tab justify paragraph.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_justify_paragraph(EditorTab *tab);
/**
 * @brief Editor tab cut line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_cut_line(EditorTab *tab);
/**
 * @brief Editor tab paste cut line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_paste_cut_line(EditorTab *tab);
/**
 * @brief Editor tab undo.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_undo(EditorTab *tab);
/**
 * @brief Editor tab redo.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_redo(EditorTab *tab);
/**
 * @brief Editor tab select all.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_select_all(EditorTab *tab);
/**
 * @brief Editor tab show completion.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param manual The manual supplied by the caller.
 */
void editor_tab_show_completion(EditorTab *tab, gboolean manual);
/**
 * @brief Editor tab hide completion.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_hide_completion(EditorTab *tab);
/**
 * @brief Editor tab set tab policy.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param width The width supplied by the caller.
 * @param insert_spaces The insert spaces supplied by the caller.
 */
void editor_tab_set_tab_policy(EditorTab *tab, guint width, gboolean insert_spaces);
/**
 * @brief Editor tab apply preferences.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_apply_preferences(EditorTab *tab);
/**
 * @brief Editor tab set minimap visible.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param visible The visible supplied by the caller.
 */
void editor_tab_set_minimap_visible(EditorTab *tab, gboolean visible);
/**
 * @brief Editor tab set preview visible.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param visible The visible supplied by the caller.
 */
void editor_tab_set_preview_visible(EditorTab *tab, gboolean visible);
/**
 * @brief Editor tab update preview.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_update_preview(EditorTab *tab);
/**
 * @brief Editor tab is latex.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_is_latex(EditorTab *tab);
/**
 * @brief Editor tab render latex.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_render_latex(EditorTab *tab);
/**
 * @brief Editor tab set backup enabled.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param enabled The enabled supplied by the caller.
 */
void editor_tab_set_backup_enabled(EditorTab *tab, gboolean enabled);
/**
 * @brief Editor tab auto save.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_auto_save(EditorTab *tab);
/**
 * @brief Editor tab cut clipboard.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_cut_clipboard(EditorTab *tab);
/**
 * @brief Editor tab copy clipboard.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_copy_clipboard(EditorTab *tab);
/**
 * @brief Editor tab paste clipboard.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_paste_clipboard(EditorTab *tab);
/**
 * @brief Editor tab basename.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *editor_tab_basename(EditorTab *tab);
/**
 * @brief Editor tab jump to line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 */
void editor_tab_jump_to_line(EditorTab *tab, guint line);

#endif /* GRAPTOS_EDITOR_TAB_H */
