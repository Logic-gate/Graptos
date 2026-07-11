#ifndef CLEAF_EDITOR_TAB_PRIVATE_H
#define CLEAF_EDITOR_TAB_PRIVATE_H

#include "editor_tab.h"
#include "app.h"
#include "dialogs.h"
#include "index.h"
#include "import_complete.h"
#include "ui.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pango/pangocairo.h>

#define CLEAF_MAX_UNDO_STATES 100u
#define CLEAF_LIVE_FEATURE_MAX_CHARS (32u * 1024u)
#define CLEAF_FULL_HIGHLIGHT_MAX_CHARS (256u * 1024u)
#define CLEAF_MAX_UNDO_CAPTURE_BYTES (32u * 1024u)
#define CLEAF_HIGHLIGHT_DELAY_MS 180u
#define CLEAF_COMPLETION_DELAY_MS 900u
#define CLEAF_MINIMAP_DELAY_MS 1500u
#define CLEAF_PREVIEW_DELAY_MS 1500u
#define CLEAF_DIAGNOSTICS_DELAY_MS 1500u
#define CLEAF_SELECTION_MATCH_DELAY_MS 750u
#define CLEAF_HOVER_DELAY_MS 650u
#define CLEAF_DIAGNOSTICS_MAX_CHARS CLEAF_LIVE_FEATURE_MAX_CHARS
#define CLEAF_MINIMAP_MAX_BYTES (256u * 1024u)
#define CLEAF_SELECTION_MATCH_MAX_CHARS CLEAF_LIVE_FEATURE_MAX_CHARS
#define CLEAF_AUTO_COMPLETION_MAX_CHARS (16u * 1024u)

char *buffer_text(EditorTab *tab);
gboolean write_all_fd(int fd, const char *data, gsize len);
gboolean write_text_atomic(const char *path, const char *text, GError **error);
char *autosave_path_for_tab(EditorTab *tab);
void clear_stack(GPtrArray *stack);
void push_limited(GPtrArray *stack, char *state);
char *pop_stack(GPtrArray *stack);
void reset_undo_state(EditorTab *tab);
int decimal_digits(int value);
gboolean editor_tab_large_file_mode(EditorTab *tab);
gboolean editor_tab_live_features_allowed(EditorTab *tab);
gboolean editor_tab_highlighting_allowed(EditorTab *tab);
gboolean editor_tab_reference_features_allowed(EditorTab *tab);
void editor_tab_cancel_live_work(EditorTab *tab);
void editor_tab_schedule_lightweight_ui_refresh(EditorTab *tab);
gboolean editor_tab_lightweight_ui_timeout_cb(gpointer user_data);
void update_gutter_width(EditorTab *tab);
void insert_text_tagless(GtkTextBuffer *buffer, GtkTextIter *where, const char *text);
void popup_append_item(GtkWidget *menu, const char *label, GCallback callback, gpointer data);
void menu_undo(GtkWidget *w, gpointer data);
void menu_redo(GtkWidget *w, gpointer data);
void menu_cut(GtkWidget *w, gpointer data);
void menu_copy(GtkWidget *w, gpointer data);
void menu_paste(GtkWidget *w, gpointer data);
void menu_select_all(GtkWidget *w, gpointer data);
void menu_cut_line(GtkWidget *w, gpointer data);
void menu_paste_line(GtkWidget *w, gpointer data);
void menu_comment(GtkWidget *w, gpointer data);
void menu_complete(GtkWidget *w, gpointer data);
gboolean hex_to_rgba(const char *text, GdkRGBA *rgba);
void on_color_swatch_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
void hide_color_preview(EditorTab *tab);
void cancel_hover_hide(EditorTab *tab);
void hide_hover_preview(EditorTab *tab);
gboolean hover_transition_timeout_cb(gpointer user_data);
void schedule_hover_transition_hide(EditorTab *tab);
char *word_at_iter(GtkTextBuffer *buffer, GtkTextIter *iter);
void hover_clear_rows(EditorTab *tab);
void editor_tab_jump_to_line_internal(EditorTab *tab, guint line);
void hover_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
GtkWidget *reference_row_new(IndexReference *ref);
gboolean hover_timeout_cb(gpointer user_data);
void on_text_view_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
void on_text_view_leave(GtkEventControllerMotion *controller, gpointer user_data);
void on_hover_popover_enter(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
void on_hover_popover_leave(GtkEventControllerMotion *controller, gpointer user_data);
GtkWidget *color_preview_row_new(EditorTab *tab, const char *hex);
void show_color_preview_in_hover(EditorTab *tab, const char *hex, GtkTextIter *where);
void maybe_show_color_preview(EditorTab *tab);
void editor_tab_show_reference_at_pointer_or_cursor(EditorTab *tab);
void on_text_view_right_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
void on_minimap_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
void on_minimap_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
void on_gutter_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
void on_vadjustment_value_changed(GtkAdjustment *adjustment, gpointer user_data);
void ensure_match_tag(EditorTab *tab);
void ensure_minimap_match_tag(EditorTab *tab);
void clear_minimap_matches(EditorTab *tab);
void clear_selection_matches(EditorTab *tab);
void apply_minimap_match(EditorTab *tab, GtkTextIter *start, GtkTextIter *end);
void update_selection_matches(EditorTab *tab);
gboolean selection_match_timeout_cb(gpointer user_data);
void editor_tab_schedule_selection_matches(EditorTab *tab);
void ensure_diagnostic_tag(EditorTab *tab);
void clear_syntax_diagnostics(EditorTab *tab);
void editor_tab_apply_syntax_diagnostics(EditorTab *tab);
gboolean diagnostics_timeout_cb(gpointer user_data);
void editor_tab_schedule_syntax_diagnostics(EditorTab *tab);
void update_minimap_text(EditorTab *tab);
gboolean minimap_timeout_cb(gpointer user_data);
gboolean preview_timeout_cb(gpointer user_data);
void editor_tab_schedule_minimap_update(EditorTab *tab);
void editor_tab_schedule_preview_update(EditorTab *tab);

void preview_ensure_tags(EditorTab *tab);
gboolean preview_is_supported(EditorTab *tab);
gboolean editor_tab_is_latex(EditorTab *tab);
void preview_render_markdown(EditorTab *tab, const char *text);
void preview_render_latex(EditorTab *tab, const char *text);
gboolean completion_is_visible(EditorTab *tab);
void completion_clear_rows(EditorTab *tab);
void completion_insert_word(EditorTab *tab, const char *word);
void completion_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
void completion_add_row(EditorTab *tab, const char *word);
gboolean completion_timeout_cb(gpointer user_data);
void editor_tab_schedule_completion(EditorTab *tab);
void completion_accept_selected(EditorTab *tab);
void completion_select_delta(EditorTab *tab, int delta);
char *tab_unit(EditorTab *tab);
void selection_line_bounds(EditorTab *tab, int *start_line_out, int *end_line_out, gboolean *has_selection_out);
void insert_tab_or_indent(EditorTab *tab);
void unindent_line(EditorTab *tab, int line);
void unindent_selection_or_line(EditorTab *tab);
gboolean on_text_view_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
void on_text_view_key_released(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
void on_close_button_clicked(GtkWidget *widget, gpointer user_data);
gboolean highlight_timeout_cb(gpointer user_data);
void on_mark_set(GtkTextBuffer *buffer, GtkTextIter *location, GtkTextMark *mark, gpointer user_data);
void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data);
gboolean write_backup_if_needed(EditorTab *tab, const char *path);
gboolean save_to_path(EditorTab *tab, const char *path);
gboolean select_match(EditorTab *tab, GtkTextIter *start, GtkTextIter *end);


gboolean editor_tab_text_rect_to_popover_parent(EditorTab *tab,
                                                GdkRectangle *rect);

#endif /* CLEAF_EDITOR_TAB_PRIVATE_H */
