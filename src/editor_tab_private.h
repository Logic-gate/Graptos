/**
 * @file src/editor_tab_private.h
 * @brief Internal editor tab helpers and callbacks.
 */

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

/**
 * @brief Cleaf max undo states macro.
 */
#define CLEAF_MAX_UNDO_STATES 100u
/**
 * @brief Cleaf live feature max chars macro.
 */
#define CLEAF_LIVE_FEATURE_MAX_CHARS (32u * 1024u)
/**
 * @brief Cleaf full highlight max chars macro.
 */
#define CLEAF_FULL_HIGHLIGHT_MAX_CHARS (256u * 1024u)
/**
 * @brief Cleaf max undo capture bytes macro.
 */
#define CLEAF_MAX_UNDO_CAPTURE_BYTES (32u * 1024u)
/**
 * @brief Cleaf highlight delay ms macro.
 */
#define CLEAF_HIGHLIGHT_DELAY_MS 180u
/**
 * @brief Cleaf completion delay ms macro.
 */
#define CLEAF_COMPLETION_DELAY_MS 900u
/**
 * @brief Cleaf minimap delay ms macro.
 */
#define CLEAF_MINIMAP_DELAY_MS 1500u
/**
 * @brief Cleaf preview delay ms macro.
 */
#define CLEAF_PREVIEW_DELAY_MS 1500u
/**
 * @brief Cleaf diagnostics delay ms macro.
 */
#define CLEAF_DIAGNOSTICS_DELAY_MS 1500u
/**
 * @brief Cleaf selection match delay ms macro.
 */
#define CLEAF_SELECTION_MATCH_DELAY_MS 750u
/**
 * @brief Cleaf hover delay ms macro.
 */
#define CLEAF_HOVER_DELAY_MS 650u
/**
 * @brief Cleaf diagnostics max chars macro.
 */
#define CLEAF_DIAGNOSTICS_MAX_CHARS CLEAF_LIVE_FEATURE_MAX_CHARS
/**
 * @brief Cleaf minimap max bytes macro.
 */
#define CLEAF_MINIMAP_MAX_BYTES (256u * 1024u)
/**
 * @brief Cleaf selection match max chars macro.
 */
#define CLEAF_SELECTION_MATCH_MAX_CHARS CLEAF_LIVE_FEATURE_MAX_CHARS
/**
 * @brief Cleaf auto completion max chars macro.
 */
#define CLEAF_AUTO_COMPLETION_MAX_CHARS (16u * 1024u)

/**
 * @brief Buffer text.
 */
char *buffer_text(EditorTab *tab);
/**
 * @brief Write all fd.
 */
gboolean write_all_fd(int fd, const char *data, gsize len);
/**
 * @brief Write text atomic.
 */
gboolean write_text_atomic(const char *path, const char *text, GError **error);
/**
 * @brief Autosave path for tab.
 */
char *autosave_path_for_tab(EditorTab *tab);
/**
 * @brief Clear stack.
 */
void clear_stack(GPtrArray *stack);
/**
 * @brief Push limited.
 */
void push_limited(GPtrArray *stack, char *state);
/**
 * @brief Pop stack.
 */
char *pop_stack(GPtrArray *stack);
/**
 * @brief Reset undo state.
 */
void reset_undo_state(EditorTab *tab);
/**
 * @brief Decimal digits.
 */
int decimal_digits(int value);
/**
 * @brief Editor tab large file mode.
 */
gboolean editor_tab_large_file_mode(EditorTab *tab);
/**
 * @brief Editor tab live features allowed.
 */
gboolean editor_tab_live_features_allowed(EditorTab *tab);
/**
 * @brief Editor tab highlighting allowed.
 */
gboolean editor_tab_highlighting_allowed(EditorTab *tab);
/**
 * @brief Editor tab reference features allowed.
 */
gboolean editor_tab_reference_features_allowed(EditorTab *tab);
/**
 * @brief Editor tab cancel live work.
 */
void editor_tab_cancel_live_work(EditorTab *tab);
/**
 * @brief Editor tab schedule lightweight ui refresh.
 */
void editor_tab_schedule_lightweight_ui_refresh(EditorTab *tab);
/**
 * @brief Editor tab lightweight ui timeout cb.
 */
gboolean editor_tab_lightweight_ui_timeout_cb(gpointer user_data);
/**
 * @brief Update gutter width.
 */
void update_gutter_width(EditorTab *tab);
/**
 * @brief Insert text tagless.
 */
void insert_text_tagless(GtkTextBuffer *buffer, GtkTextIter *where, const char *text);
/**
 * @brief Popup append item.
 */
void popup_append_item(GtkWidget *menu, const char *label, GCallback callback, gpointer data);
/**
 * @brief Menu undo.
 */
void menu_undo(GtkWidget *w, gpointer data);
/**
 * @brief Menu redo.
 */
void menu_redo(GtkWidget *w, gpointer data);
/**
 * @brief Menu cut.
 */
void menu_cut(GtkWidget *w, gpointer data);
/**
 * @brief Menu copy.
 */
void menu_copy(GtkWidget *w, gpointer data);
/**
 * @brief Menu paste.
 */
void menu_paste(GtkWidget *w, gpointer data);
/**
 * @brief Menu select all.
 */
void menu_select_all(GtkWidget *w, gpointer data);
/**
 * @brief Menu cut line.
 */
void menu_cut_line(GtkWidget *w, gpointer data);
/**
 * @brief Menu paste line.
 */
void menu_paste_line(GtkWidget *w, gpointer data);
/**
 * @brief Menu comment.
 */
void menu_comment(GtkWidget *w, gpointer data);
/**
 * @brief Menu complete.
 */
void menu_complete(GtkWidget *w, gpointer data);
/**
 * @brief Hex to rgba.
 */
gboolean hex_to_rgba(const char *text, GdkRGBA *rgba);
/**
 * @brief On color swatch draw.
 */
void on_color_swatch_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
/**
 * @brief Hide color preview.
 */
void hide_color_preview(EditorTab *tab);
/**
 * @brief Cancel hover hide.
 */
void cancel_hover_hide(EditorTab *tab);
/**
 * @brief Hide hover preview.
 */
void hide_hover_preview(EditorTab *tab);
/**
 * @brief Hover transition timeout cb.
 */
gboolean hover_transition_timeout_cb(gpointer user_data);
/**
 * @brief Schedule hover transition hide.
 */
void schedule_hover_transition_hide(EditorTab *tab);
/**
 * @brief Word at iter.
 */
char *word_at_iter(GtkTextBuffer *buffer, GtkTextIter *iter);
/**
 * @brief Hover clear rows.
 */
void hover_clear_rows(EditorTab *tab);
/**
 * @brief Editor tab jump to line internal.
 */
void editor_tab_jump_to_line_internal(EditorTab *tab, guint line);
/**
 * @brief Hover row activated.
 */
void hover_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
/**
 * @brief Reference row new.
 */
GtkWidget *reference_row_new(IndexReference *ref);
/**
 * @brief Hover timeout cb.
 */
gboolean hover_timeout_cb(gpointer user_data);
/**
 * @brief On text view motion.
 */
void on_text_view_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
/**
 * @brief On text view leave.
 */
void on_text_view_leave(GtkEventControllerMotion *controller, gpointer user_data);
/**
 * @brief On hover popover enter.
 */
void on_hover_popover_enter(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
/**
 * @brief On hover popover leave.
 */
void on_hover_popover_leave(GtkEventControllerMotion *controller, gpointer user_data);
/**
 * @brief Color preview row new.
 */
GtkWidget *color_preview_row_new(EditorTab *tab, const char *hex);
/**
 * @brief Show color preview in hover.
 */
void show_color_preview_in_hover(EditorTab *tab, const char *hex, GtkTextIter *where);
/**
 * @brief Maybe show color preview.
 */
void maybe_show_color_preview(EditorTab *tab);
/**
 * @brief Editor tab show reference at pointer or cursor.
 */
void editor_tab_show_reference_at_pointer_or_cursor(EditorTab *tab);
/**
 * @brief On text view right click.
 */
void on_text_view_right_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
/**
 * @brief On minimap click.
 */
void on_minimap_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
/**
 * @brief On minimap draw.
 */
void on_minimap_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
/**
 * @brief On gutter draw.
 */
void on_gutter_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
/**
 * @brief On vadjustment value changed.
 */
void on_vadjustment_value_changed(GtkAdjustment *adjustment, gpointer user_data);
/**
 * @brief Ensure match tag.
 */
void ensure_match_tag(EditorTab *tab);
/**
 * @brief Ensure minimap match tag.
 */
void ensure_minimap_match_tag(EditorTab *tab);
/**
 * @brief Clear minimap matches.
 */
void clear_minimap_matches(EditorTab *tab);
/**
 * @brief Clear selection matches.
 */
void clear_selection_matches(EditorTab *tab);
/**
 * @brief Apply minimap match.
 */
void apply_minimap_match(EditorTab *tab, GtkTextIter *start, GtkTextIter *end);
/**
 * @brief Update selection matches.
 */
void update_selection_matches(EditorTab *tab);
/**
 * @brief Selection match timeout cb.
 */
gboolean selection_match_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule selection matches.
 */
void editor_tab_schedule_selection_matches(EditorTab *tab);
/**
 * @brief Ensure diagnostic tag.
 */
void ensure_diagnostic_tag(EditorTab *tab);
/**
 * @brief Clear syntax diagnostics.
 */
void clear_syntax_diagnostics(EditorTab *tab);
/**
 * @brief Editor tab apply syntax diagnostics.
 */
void editor_tab_apply_syntax_diagnostics(EditorTab *tab);
/**
 * @brief Diagnostics timeout cb.
 */
gboolean diagnostics_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule syntax diagnostics.
 */
void editor_tab_schedule_syntax_diagnostics(EditorTab *tab);
/**
 * @brief Update minimap text.
 */
void update_minimap_text(EditorTab *tab);
/**
 * @brief Minimap timeout cb.
 */
gboolean minimap_timeout_cb(gpointer user_data);
/**
 * @brief Preview timeout cb.
 */
gboolean preview_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule minimap update.
 */
void editor_tab_schedule_minimap_update(EditorTab *tab);
/**
 * @brief Editor tab schedule preview update.
 */
void editor_tab_schedule_preview_update(EditorTab *tab);

/**
 * @brief Preview ensure tags.
 */
void preview_ensure_tags(EditorTab *tab);
/**
 * @brief Preview is supported.
 */
gboolean preview_is_supported(EditorTab *tab);
/**
 * @brief Editor tab is latex.
 */
gboolean editor_tab_is_latex(EditorTab *tab);
/**
 * @brief Preview render markdown.
 */
void preview_render_markdown(EditorTab *tab, const char *text);
/**
 * @brief Preview render latex.
 */
void preview_render_latex(EditorTab *tab, const char *text);
/**
 * @brief Completion is visible.
 */
gboolean completion_is_visible(EditorTab *tab);
/**
 * @brief Completion clear rows.
 */
void completion_clear_rows(EditorTab *tab);
/**
 * @brief Completion insert word.
 */
void completion_insert_word(EditorTab *tab, const char *word);
/**
 * @brief Completion row activated.
 */
void completion_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
/**
 * @brief Completion add row.
 */
void completion_add_row(EditorTab *tab, const char *word);
/**
 * @brief Completion timeout cb.
 */
gboolean completion_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule completion.
 */
void editor_tab_schedule_completion(EditorTab *tab);
/**
 * @brief Completion accept selected.
 */
void completion_accept_selected(EditorTab *tab);
/**
 * @brief Completion select delta.
 */
void completion_select_delta(EditorTab *tab, int delta);
/**
 * @brief Tab unit.
 */
char *tab_unit(EditorTab *tab);
/**
 * @brief Selection line bounds.
 */
void selection_line_bounds(EditorTab *tab, int *start_line_out, int *end_line_out, gboolean *has_selection_out);
/**
 * @brief Insert tab or indent.
 */
void insert_tab_or_indent(EditorTab *tab);
/**
 * @brief Unindent line.
 */
void unindent_line(EditorTab *tab, int line);
/**
 * @brief Unindent selection or line.
 */
void unindent_selection_or_line(EditorTab *tab);
/**
 * @brief On text view key pressed.
 */
gboolean on_text_view_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
/**
 * @brief On text view key released.
 */
void on_text_view_key_released(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
/**
 * @brief On close button clicked.
 */
void on_close_button_clicked(GtkWidget *widget, gpointer user_data);
/**
 * @brief Highlight timeout cb.
 */
gboolean highlight_timeout_cb(gpointer user_data);
/**
 * @brief On mark set.
 */
void on_mark_set(GtkTextBuffer *buffer, GtkTextIter *location, GtkTextMark *mark, gpointer user_data);
/**
 * @brief On buffer changed.
 */
void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data);
/**
 * @brief Write backup if needed.
 */
gboolean write_backup_if_needed(EditorTab *tab, const char *path);
/**
 * @brief Save to path.
 */
gboolean save_to_path(EditorTab *tab, const char *path);
/**
 * @brief Select match.
 */
gboolean select_match(EditorTab *tab, GtkTextIter *start, GtkTextIter *end);


/**
 * @brief Editor tab text rect to popover parent.
 */
gboolean editor_tab_text_rect_to_popover_parent(EditorTab *tab,
                                                GdkRectangle *rect);

#endif /* CLEAF_EDITOR_TAB_PRIVATE_H */
