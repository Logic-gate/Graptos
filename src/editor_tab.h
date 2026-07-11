#ifndef CLEAF_EDITOR_TAB_H
#define CLEAF_EDITOR_TAB_H

#include <gtk/gtk.h>
#include <gtksourceview/gtksource.h>
#include "syntax.h"
#include "completion.h"

typedef struct _EditorWindow EditorWindow;

typedef struct _EditorTab EditorTab;

struct _EditorTab {
    EditorWindow *win;
    GtkWidget *box;
    GtkWidget *editor_area;
    GtkWidget *gutter;
    GtkWidget *scrolled;
    GtkWidget *text_view;
    GtkWidget *popover_parent;
    GtkTextBuffer *buffer;
    GtkSourceBuffer *source_buffer;
    GtkWidget *completion_popover;
    GtkWidget *completion_list;
    GtkWidget *completion_scrolled;
    GtkWidget *hover_popover;
    GtkWidget *hover_list;
    GtkWidget *minimap_scrolled;
    GtkWidget *minimap_view;
    GtkTextBuffer *minimap_buffer;
    GtkWidget *preview_scrolled;
    GtkWidget *preview_view;
    GtkTextBuffer *preview_buffer;
    GtkWidget *tab_widget;
    GtkWidget *tab_title;

    char *file_path;
    char *last_snapshot;
    char *kill_buffer;
    char *search_text;
    char *completion_prefix;
    char *hover_word;
    SyntaxDef *active_syntax;

    GPtrArray *undo_stack; /* char* */
    GPtrArray *redo_stack; /* char* */
    guint highlight_timeout;
    guint completion_timeout;
    guint minimap_timeout;
    guint preview_timeout;
    guint diagnostics_timeout;
    guint selection_match_timeout;
    guint hover_timeout;
    guint hover_hide_timeout;
    guint ui_refresh_timeout;
    guint tab_width;
    guint diagnostic_warnings;
    guint cached_gutter_width;
    GdkRGBA color_preview_rgba;
    gint hover_x;
    gint hover_y;
    gdouble pointer_x;
    gdouble pointer_y;
    gboolean modified;
    gboolean applying_change;
    gboolean manual_syntax_override;
    gboolean backup_enabled;
    gboolean wrap_enabled;
    gboolean autocomplete_enabled;
    gboolean completion_manual;
    gboolean insert_spaces;
    gboolean color_preview_valid;
    gboolean hover_pointer_inside;
    gboolean hover_pinned;
    gboolean inspect_reference_active;
    gboolean minimap_updating;
    gboolean preview_updating;
    gboolean locked;
    gboolean hover_popover_locked;
    gboolean pointer_valid;
    gboolean selection_matches_active;
    gboolean diagnostics_active;
    gboolean custom_highlight_active;
    gboolean low_latency_mode_active;
};

EditorTab *editor_tab_new(EditorWindow *win);
void editor_tab_destroy_popovers(EditorTab *tab);
void editor_tab_free(EditorTab *tab);
gboolean editor_tab_load_file(EditorTab *tab, const char *path);
gboolean editor_tab_save(EditorTab *tab, gboolean force_dialog);
gboolean editor_tab_confirm_close(EditorTab *tab);
void editor_tab_update_title(EditorTab *tab);
void editor_tab_update_status(EditorTab *tab);
void editor_tab_set_locked(EditorTab *tab, gboolean locked);
gboolean editor_tab_is_locked(EditorTab *tab);
void editor_tab_set_syntax(EditorTab *tab, SyntaxDef *syntax, gboolean manual);
void editor_tab_auto_select_syntax(EditorTab *tab);
void editor_tab_schedule_highlight(EditorTab *tab);
void editor_tab_apply_highlight(EditorTab *tab);
void editor_tab_update_highlight_engine(EditorTab *tab);
void editor_tab_find(EditorTab *tab, const char *query, gboolean backwards);
void editor_tab_replace_current(EditorTab *tab, const char *find, const char *replace);
void editor_tab_replace_all(EditorTab *tab, const char *find, const char *replace);
void editor_tab_toggle_comment(EditorTab *tab);
void editor_tab_go_to_line(EditorTab *tab);
void editor_tab_justify_paragraph(EditorTab *tab);
void editor_tab_cut_line(EditorTab *tab);
void editor_tab_paste_cut_line(EditorTab *tab);
void editor_tab_undo(EditorTab *tab);
void editor_tab_redo(EditorTab *tab);
void editor_tab_select_all(EditorTab *tab);
void editor_tab_show_completion(EditorTab *tab, gboolean manual);
void editor_tab_hide_completion(EditorTab *tab);
void editor_tab_set_tab_policy(EditorTab *tab, guint width, gboolean insert_spaces);
void editor_tab_apply_preferences(EditorTab *tab);
void editor_tab_set_minimap_visible(EditorTab *tab, gboolean visible);
void editor_tab_set_preview_visible(EditorTab *tab, gboolean visible);
void editor_tab_update_preview(EditorTab *tab);
gboolean editor_tab_is_latex(EditorTab *tab);
void editor_tab_render_latex(EditorTab *tab);
void editor_tab_set_backup_enabled(EditorTab *tab, gboolean enabled);
gboolean editor_tab_auto_save(EditorTab *tab);
void editor_tab_cut_clipboard(EditorTab *tab);
void editor_tab_copy_clipboard(EditorTab *tab);
void editor_tab_paste_clipboard(EditorTab *tab);
char *editor_tab_basename(EditorTab *tab);
void editor_tab_jump_to_line(EditorTab *tab, guint line);

#endif /* CLEAF_EDITOR_TAB_H */
