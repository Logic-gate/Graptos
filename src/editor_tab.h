/**
 * @file src/editor_tab.h
 * @brief Editor tab public API and state structure.
 */

#ifndef CLEAF_EDITOR_TAB_H
#define CLEAF_EDITOR_TAB_H

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
 * @brief Editor tab type definition.
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
    GtkWidget *preview_scrolled; /**< Preview scrolled. */
    GtkWidget *preview_view; /**< Preview view. */
    GtkTextBuffer *preview_buffer; /**< Preview buffer. */
    GtkWidget *tab_widget; /**< Tab widget. */
    GtkWidget *tab_title; /**< Tab title. */

    char *file_path; /**< File path. */
    char *last_snapshot; /**< Last snapshot. */
    char *kill_buffer; /**< Kill buffer. */
    char *search_text; /**< Search text. */
    char *completion_prefix; /**< Completion prefix. */
    char *hover_word; /**< Hover word. */
    SyntaxDef *active_syntax; /**< Active syntax. */

    GPtrArray *undo_stack; /**< char*. */
    GPtrArray *redo_stack; /**< char*. */
    guint highlight_timeout; /**< Highlight timeout. */
    guint completion_timeout; /**< Completion timeout. */
    guint minimap_timeout; /**< Minimap timeout. */
    guint preview_timeout; /**< Preview timeout. */
    guint diagnostics_timeout; /**< Diagnostics timeout. */
    guint selection_match_timeout; /**< Selection match timeout. */
    guint hover_timeout; /**< Hover timeout. */
    guint hover_hide_timeout; /**< Hover hide timeout. */
    guint ui_refresh_timeout; /**< Ui refresh timeout. */
    guint tab_width; /**< Tab width. */
    guint diagnostic_warnings; /**< Diagnostic warnings. */
    guint cached_gutter_width; /**< Cached gutter width. */
    GdkRGBA color_preview_rgba; /**< Color preview rgba. */
    gint hover_x; /**< Hover x. */
    gint hover_y; /**< Hover y. */
    gdouble pointer_x; /**< Pointer x. */
    gdouble pointer_y; /**< Pointer y. */
    gboolean modified; /**< Modified. */
    gboolean applying_change; /**< Applying change. */
    gboolean manual_syntax_override; /**< Manual syntax override. */
    gboolean backup_enabled; /**< Backup enabled. */
    gboolean wrap_enabled; /**< Wrap enabled. */
    gboolean autocomplete_enabled; /**< Autocomplete enabled. */
    gboolean completion_manual; /**< Completion manual. */
    gboolean insert_spaces; /**< Insert spaces. */
    gboolean color_preview_valid; /**< Color preview valid. */
    gboolean hover_pointer_inside; /**< Hover pointer inside. */
    gboolean hover_pinned; /**< Hover pinned. */
    gboolean inspect_reference_active; /**< Inspect reference active. */
    gboolean minimap_updating; /**< Minimap updating. */
    gboolean preview_updating; /**< Preview updating. */
    gboolean locked; /**< Locked. */
    gboolean hover_popover_locked; /**< Hover popover locked. */
    gboolean pointer_valid; /**< Pointer valid. */
    gboolean selection_matches_active; /**< Selection matches active. */
    gboolean diagnostics_active; /**< Diagnostics active. */
    gboolean custom_highlight_active; /**< Custom highlight active. */
    gboolean low_latency_mode_active; /**< Low latency mode active. */
};

/**
 * @brief Editor tab new.
 */
EditorTab *editor_tab_new(EditorWindow *win);
/**
 * @brief Editor tab destroy popovers.
 */
void editor_tab_destroy_popovers(EditorTab *tab);
/**
 * @brief Editor tab free.
 */
void editor_tab_free(EditorTab *tab);
/**
 * @brief Editor tab load file.
 */
gboolean editor_tab_load_file(EditorTab *tab, const char *path);
/**
 * @brief Editor tab save.
 */
gboolean editor_tab_save(EditorTab *tab, gboolean force_dialog);
/**
 * @brief Editor tab confirm close.
 */
gboolean editor_tab_confirm_close(EditorTab *tab);
/**
 * @brief Editor tab update title.
 */
void editor_tab_update_title(EditorTab *tab);
/**
 * @brief Editor tab update status.
 */
void editor_tab_update_status(EditorTab *tab);
/**
 * @brief Editor tab set locked.
 */
void editor_tab_set_locked(EditorTab *tab, gboolean locked);
/**
 * @brief Editor tab is locked.
 */
gboolean editor_tab_is_locked(EditorTab *tab);
/**
 * @brief Editor tab set syntax.
 */
void editor_tab_set_syntax(EditorTab *tab, SyntaxDef *syntax, gboolean manual);
/**
 * @brief Editor tab auto select syntax.
 */
void editor_tab_auto_select_syntax(EditorTab *tab);
/**
 * @brief Editor tab schedule highlight.
 */
void editor_tab_schedule_highlight(EditorTab *tab);
/**
 * @brief Editor tab apply highlight.
 */
void editor_tab_apply_highlight(EditorTab *tab);
/**
 * @brief Editor tab update highlight engine.
 */
void editor_tab_update_highlight_engine(EditorTab *tab);
/**
 * @brief Editor tab find.
 */
void editor_tab_find(EditorTab *tab, const char *query, gboolean backwards);
/**
 * @brief Editor tab replace current.
 */
void editor_tab_replace_current(EditorTab *tab, const char *find, const char *replace);
/**
 * @brief Editor tab replace all.
 */
void editor_tab_replace_all(EditorTab *tab, const char *find, const char *replace);
/**
 * @brief Editor tab toggle comment.
 */
void editor_tab_toggle_comment(EditorTab *tab);
/**
 * @brief Editor tab go to line.
 */
void editor_tab_go_to_line(EditorTab *tab);
/**
 * @brief Editor tab justify paragraph.
 */
void editor_tab_justify_paragraph(EditorTab *tab);
/**
 * @brief Editor tab cut line.
 */
void editor_tab_cut_line(EditorTab *tab);
/**
 * @brief Editor tab paste cut line.
 */
void editor_tab_paste_cut_line(EditorTab *tab);
/**
 * @brief Editor tab undo.
 */
void editor_tab_undo(EditorTab *tab);
/**
 * @brief Editor tab redo.
 */
void editor_tab_redo(EditorTab *tab);
/**
 * @brief Editor tab select all.
 */
void editor_tab_select_all(EditorTab *tab);
/**
 * @brief Editor tab show completion.
 */
void editor_tab_show_completion(EditorTab *tab, gboolean manual);
/**
 * @brief Editor tab hide completion.
 */
void editor_tab_hide_completion(EditorTab *tab);
/**
 * @brief Editor tab set tab policy.
 */
void editor_tab_set_tab_policy(EditorTab *tab, guint width, gboolean insert_spaces);
/**
 * @brief Editor tab apply preferences.
 */
void editor_tab_apply_preferences(EditorTab *tab);
/**
 * @brief Editor tab set minimap visible.
 */
void editor_tab_set_minimap_visible(EditorTab *tab, gboolean visible);
/**
 * @brief Editor tab set preview visible.
 */
void editor_tab_set_preview_visible(EditorTab *tab, gboolean visible);
/**
 * @brief Editor tab update preview.
 */
void editor_tab_update_preview(EditorTab *tab);
/**
 * @brief Editor tab is latex.
 */
gboolean editor_tab_is_latex(EditorTab *tab);
/**
 * @brief Editor tab render latex.
 */
void editor_tab_render_latex(EditorTab *tab);
/**
 * @brief Editor tab set backup enabled.
 */
void editor_tab_set_backup_enabled(EditorTab *tab, gboolean enabled);
/**
 * @brief Editor tab auto save.
 */
gboolean editor_tab_auto_save(EditorTab *tab);
/**
 * @brief Editor tab cut clipboard.
 */
void editor_tab_cut_clipboard(EditorTab *tab);
/**
 * @brief Editor tab copy clipboard.
 */
void editor_tab_copy_clipboard(EditorTab *tab);
/**
 * @brief Editor tab paste clipboard.
 */
void editor_tab_paste_clipboard(EditorTab *tab);
/**
 * @brief Editor tab basename.
 */
char *editor_tab_basename(EditorTab *tab);
/**
 * @brief Editor tab jump to line.
 */
void editor_tab_jump_to_line(EditorTab *tab, guint line);

#endif /* CLEAF_EDITOR_TAB_H */
