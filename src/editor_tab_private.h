/**
 * @file src/editor_tab_private.h
 * @brief Internal editor tab helpers and callbacks.
 * @details Editor tabs hold the real editing surface. We split the implementation by
 *          lifecycle, input, rendering, preview, and transient UI because each part has
 *          different timing and cleanup pressure.
 */

#ifndef GRAPTOS_EDITOR_TAB_PRIVATE_H
#define GRAPTOS_EDITOR_TAB_PRIVATE_H

#include "editor_tab.h"
#include "app.h"
#include "dialogs.h"
#include "formatter.h"
#include "index.h"
#include "import_complete.h"
#include "lsp_client.h"
#include "ui.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pango/pangocairo.h>

/**
 * @brief Graptoς max undo states macro.
 */
#define GRAPTOS_MAX_UNDO_STATES 100u
/**
 * @brief Graptoς live feature max chars macro.
 */
#define GRAPTOS_LIVE_FEATURE_MAX_CHARS (32u * 1024u)
/**
 * @brief Graptoς full highlight max chars macro.
 */
#define GRAPTOS_FULL_HIGHLIGHT_MAX_CHARS (256u * 1024u)
/**
 * @brief Graptoς max undo capture bytes macro.
 */
#define GRAPTOS_MAX_UNDO_CAPTURE_BYTES (32u * 1024u)
/**
 * @brief Graptoς highlight delay ms macro.
 */
#define GRAPTOS_HIGHLIGHT_DELAY_MS 180u
/**
 * @brief Graptoς completion delay ms macro.
 */
#define GRAPTOS_COMPLETION_DELAY_MS 900u
/**
 * @brief Graptoς minimap delay ms macro.
 */
#define GRAPTOS_MINIMAP_DELAY_MS 1500u
/**
 * @brief Graptoς preview delay ms macro.
 */
#define GRAPTOS_PREVIEW_DELAY_MS 1500u
/**
 * @brief Graptoς diagnostics delay ms macro.
 */
#define GRAPTOS_DIAGNOSTICS_DELAY_MS 1500u
/**
 * @brief Graptoς selection match delay ms macro.
 */
#define GRAPTOS_SELECTION_MATCH_DELAY_MS 750u
/**
 * @brief Graptoς color literal delay ms macro.
 */
#define GRAPTOS_COLOR_LITERAL_DELAY_MS 350u
/**
 * @brief Graptoς hover delay ms macro.
 */
#define GRAPTOS_HOVER_DELAY_MS 650u
/**
 * @brief Graptoς diagnostics max chars macro.
 */
#define GRAPTOS_DIAGNOSTICS_MAX_CHARS GRAPTOS_LIVE_FEATURE_MAX_CHARS
/**
 * @brief Graptoς minimap max bytes macro.
 */
#define GRAPTOS_MINIMAP_MAX_BYTES (256u * 1024u)
/**
 * @brief Graptoς selection match max chars macro.
 */
#define GRAPTOS_SELECTION_MATCH_MAX_CHARS GRAPTOS_LIVE_FEATURE_MAX_CHARS
/**
 * @brief Graptoς color literal max chars macro.
 */
#define GRAPTOS_COLOR_LITERAL_MAX_CHARS GRAPTOS_LIVE_FEATURE_MAX_CHARS
/**
 * @brief Graptoς auto completion max chars macro.
 */
#define GRAPTOS_AUTO_COMPLETION_MAX_CHARS (16u * 1024u)

/**
 * @brief Buffer text.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *buffer_text(EditorTab *tab);
/**
 * @brief Write all fd.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param fd The fd supplied by the caller.
 * @param data The callback context passed by the caller.
 * @param len The len supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean write_all_fd(int fd, const char *data, gsize len);
/**
 * @brief Write text atomic.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param path The filesystem path supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param error Return location for an optional error; left untouched on success unless GTK sets it.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean write_text_atomic(const char *path, const char *text, GError **error);
/**
 * @brief Autosave path for tab.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *autosave_path_for_tab(EditorTab *tab);
/**
 * @brief Clear stack.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param stack The stack supplied by the caller.
 */
void clear_stack(GPtrArray *stack);
/**
 * @brief Push limited.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param stack The stack supplied by the caller.
 * @param state The state supplied by the caller.
 */
void push_limited(GPtrArray *stack, char *state);
/**
 * @brief Pop stack.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param stack The stack supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *pop_stack(GPtrArray *stack);
/**
 * @brief Reset undo state.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void reset_undo_state(EditorTab *tab);
/**
 * @brief Decimal digits.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param value The value being parsed, stored, or applied.
 * @return The computed value requested by the caller.
 */
int decimal_digits(int value);
/**
 * @brief Editor tab large file mode.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_large_file_mode(EditorTab *tab);
/**
 * @brief Editor tab live features allowed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_live_features_allowed(EditorTab *tab);
/**
 * @brief Editor tab highlighting allowed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_highlighting_allowed(EditorTab *tab);
/**
 * @brief Editor tab reference features allowed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_reference_features_allowed(EditorTab *tab);
/**
 * @brief Editor tab cancel live work.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_cancel_live_work(EditorTab *tab);
/**
 * @brief Editor tab schedule lightweight ui refresh.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_lightweight_ui_refresh(EditorTab *tab);
/**
 * @brief Editor tab lightweight ui timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_lightweight_ui_timeout_cb(gpointer user_data);
/**
 * @brief Update gutter width.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void update_gutter_width(EditorTab *tab);
/**
 * @brief Insert text tagless.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param where The where supplied by the caller.
 * @param text The text fragment supplied by the caller.
 */
void insert_text_tagless(GtkTextBuffer *buffer, GtkTextIter *where, const char *text);
/**
 * @brief Popup append item.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param menu The menu supplied by the caller.
 * @param label The label supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param data The callback context passed by the caller.
 */
void popup_append_item(GtkWidget *menu, const char *label, GCallback callback, gpointer data);
/**
 * @brief Menu undo.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_undo(GtkWidget *w, gpointer data);
/**
 * @brief Menu redo.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_redo(GtkWidget *w, gpointer data);
/**
 * @brief Menu cut.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_cut(GtkWidget *w, gpointer data);
/**
 * @brief Menu copy.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_copy(GtkWidget *w, gpointer data);
/**
 * @brief Menu paste.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_paste(GtkWidget *w, gpointer data);
/**
 * @brief Menu select all.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_select_all(GtkWidget *w, gpointer data);
/**
 * @brief Menu cut line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_cut_line(GtkWidget *w, gpointer data);
/**
 * @brief Menu paste line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_paste_line(GtkWidget *w, gpointer data);
/**
 * @brief Menu comment.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_comment(GtkWidget *w, gpointer data);
/**
 * @brief Menu complete.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_complete(GtkWidget *w, gpointer data);
/**
 * @brief Hex to rgba.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @param rgba The rgba supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean hex_to_rgba(const char *text, GdkRGBA *rgba);
/**
 * @brief On color swatch draw.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param area The area supplied by the caller.
 * @param cr The cr supplied by the caller.
 * @param width The width supplied by the caller.
 * @param height The height supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_color_swatch_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
/**
 * @brief Hide color preview.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void hide_color_preview(EditorTab *tab);
/**
 * @brief Cancel hover hide.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void cancel_hover_hide(EditorTab *tab);
/**
 * @brief Hide hover preview.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void hide_hover_preview(EditorTab *tab);
/**
 * @brief Hover transition timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean hover_transition_timeout_cb(gpointer user_data);
/**
 * @brief Schedule hover transition hide.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void schedule_hover_transition_hide(EditorTab *tab);
/**
 * @brief Word at iter.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param iter The text iterator that anchors the lookup.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *word_at_iter(GtkTextBuffer *buffer, GtkTextIter *iter);
/**
 * @brief Hover clear rows.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void hover_clear_rows(EditorTab *tab);
/**
 * @brief Editor tab jump to line internal.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 */
void editor_tab_jump_to_line_internal(EditorTab *tab, guint line);
/**
 * @brief Hover row activated.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param box The box supplied by the caller.
 * @param row The row supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void hover_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
/**
 * @brief Reference row new.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param ref The ref supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *reference_row_new(IndexReference *ref);
/**
 * @brief Hover timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean hover_timeout_cb(gpointer user_data);
/**
 * @brief On text view motion.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_text_view_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
/**
 * @brief On text view leave.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_text_view_leave(GtkEventControllerMotion *controller, gpointer user_data);
/**
 * @brief On hover popover enter.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_hover_popover_enter(GtkEventControllerMotion *controller, double x, double y, gpointer user_data);
/**
 * @brief On hover popover leave.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_hover_popover_leave(GtkEventControllerMotion *controller, gpointer user_data);
/**
 * @brief Color preview row new.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param hex The hex supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *color_preview_row_new(EditorTab *tab, const char *hex);
/**
 * @brief Show color preview in hover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param hex The hex supplied by the caller.
 * @param where The where supplied by the caller.
 */
void show_color_preview_in_hover(EditorTab *tab, const char *hex, GtkTextIter *where);
/**
 * @brief Maybe show color preview.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void maybe_show_color_preview(EditorTab *tab);
/**
 * @brief Maybe show regex tester.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean maybe_show_regex_tester(EditorTab *tab);
/**
 * @brief Regex tester timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean regex_tester_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule regex tester.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_regex_tester(EditorTab *tab);
/**
 * @brief Editor tab show reference at pointer or cursor.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_show_reference_at_pointer_or_cursor(EditorTab *tab);
/**
 * @brief On text view right click.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param gesture The gesture supplied by the caller.
 * @param n_press The n press supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_text_view_right_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
/**
 * @brief On minimap click.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param gesture The gesture supplied by the caller.
 * @param n_press The n press supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_minimap_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data);
/**
 * @brief On minimap draw.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param area The area supplied by the caller.
 * @param cr The cr supplied by the caller.
 * @param width The width supplied by the caller.
 * @param height The height supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_minimap_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
/**
 * @brief On gutter draw.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param area The area supplied by the caller.
 * @param cr The cr supplied by the caller.
 * @param width The width supplied by the caller.
 * @param height The height supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_gutter_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
/**
 * @brief On vadjustment value changed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param adjustment The adjustment supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_vadjustment_value_changed(GtkAdjustment *adjustment, gpointer user_data);
/**
 * @brief Ensure match tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void ensure_match_tag(EditorTab *tab);
/**
 * @brief Ensure minimap match tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void ensure_minimap_match_tag(EditorTab *tab);
/**
 * @brief Clear minimap matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void clear_minimap_matches(EditorTab *tab);
/**
 * @brief Clear selection matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void clear_selection_matches(EditorTab *tab);
/**
 * @brief Apply minimap match.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param start The start supplied by the caller.
 * @param end The end supplied by the caller.
 */
void apply_minimap_match(EditorTab *tab, GtkTextIter *start, GtkTextIter *end);
/**
 * @brief Update selection matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void update_selection_matches(EditorTab *tab);
/**
 * @brief Selection match timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean selection_match_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule selection matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_selection_matches(EditorTab *tab);
/**
 * @brief Clear color literals.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void clear_color_literals(EditorTab *tab);
/**
 * @brief Update color literals.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void update_color_literals(EditorTab *tab);
/**
 * @brief Color literal timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean color_literal_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule color literals.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_color_literals(EditorTab *tab);
/**
 * @brief Ensure diagnostic tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void ensure_diagnostic_tag(EditorTab *tab);
/**
 * @brief Free an editor diagnostic.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param data The callback context passed by the caller.
 */
void editor_diagnostic_free(gpointer data);
/**
 * @brief Find diagnostic at an iterator.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param iter The text iterator that anchors the lookup.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
EditorDiagnostic *editor_tab_diagnostic_at_iter(EditorTab *tab, GtkTextIter *iter);
/**
 * @brief Show diagnostic hover at an iterator.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param iter The text iterator that anchors the lookup.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_show_diagnostic_hover_at_iter(EditorTab *tab, GtkTextIter *iter);
/**
 * @brief Clear syntax diagnostics.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void clear_syntax_diagnostics(EditorTab *tab);
/**
 * @brief Apply one external diagnostic range.
 *
 * @return TRUE when the diagnostic range was applied to the buffer.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param start_line The start line supplied by the caller.
 * @param start_character The start character supplied by the caller.
 * @param end_line The end line supplied by the caller.
 * @param end_character The end character supplied by the caller.
 * @param message The message supplied by the caller.
 */
gboolean editor_tab_apply_external_diagnostic(EditorTab *tab,
                                              gint start_line,
                                              gint start_character,
                                              gint end_line,
                                              gint end_character,
                                              const char *message);
/**
 * @brief Editor tab apply syntax diagnostics.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_apply_syntax_diagnostics(EditorTab *tab);
/**
 * @brief Diagnostics timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean diagnostics_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule syntax diagnostics.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_syntax_diagnostics(EditorTab *tab);
/**
 * @brief LSP change timeout callback.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean lsp_change_timeout_cb(gpointer user_data);
/**
 * @brief Schedule debounced LSP document change.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_lsp_change(EditorTab *tab);
/**
 * @brief Update minimap text.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void update_minimap_text(EditorTab *tab);
/**
 * @brief Minimap timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean minimap_timeout_cb(gpointer user_data);
/**
 * @brief Preview timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean preview_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule minimap update.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_minimap_update(EditorTab *tab);
/**
 * @brief Editor tab schedule preview update.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_preview_update(EditorTab *tab);

/**
 * @brief Detach the rendered preview into a separate window.
 * @details Markdown and LaTeX previews are buffer-owned state. Detaching moves
 *          only the preview container, so the renderer keeps updating the same
 *          GtkTextBuffer while the user compares source and rendered output.
 * @param tab The editor tab whose preview should move into a window.
 */
void editor_tab_preview_detach(EditorTab *tab);
/**
 * @brief Reattach the rendered preview to the editor splitter.
 * @details Reattaching returns the preview widget to the tab-owned paned
 *          layout and destroys the temporary preview window without touching
 *          the rendered buffer contents.
 * @param tab The editor tab whose preview should return to the tab layout.
 */
void editor_tab_preview_reattach(EditorTab *tab);
/**
 * @brief Close any detached preview window owned by the tab.
 * @details Tab teardown must remove the preview box from transient windows
 *          before the tab memory is freed, otherwise late window callbacks can
 *          reach a stale EditorTab pointer.
 * @param tab The editor tab whose detached preview should be closed.
 */
void editor_tab_preview_close_detached(EditorTab *tab);
/**
 * @brief Toggle preview attach state from an idle callback.
 * @details The preview detach button is part of the widget tree being moved.
 *          Running the reparent after the click event returns avoids GTK
 *          coordinate calculations against a widget that changed native
 *          surfaces mid-event.
 * @param user_data The editor tab that owns the preview widgets.
 * @return G_SOURCE_REMOVE after the deferred move has completed.
 */
gboolean preview_reparent_idle_cb(gpointer user_data);

/**
 * @brief Preview ensure tags.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void preview_ensure_tags(EditorTab *tab);
/**
 * @brief Preview is supported.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean preview_is_supported(EditorTab *tab);
/**
 * @brief Editor tab is latex.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_is_latex(EditorTab *tab);
/**
 * @brief Preview render markdown.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param text The text fragment supplied by the caller.
 */
void preview_render_markdown(EditorTab *tab, const char *text);
/**
 * @brief Preview render latex.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param text The text fragment supplied by the caller.
 */
void preview_render_latex(EditorTab *tab, const char *text);
/**
 * @brief Completion is visible.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean completion_is_visible(EditorTab *tab);
/**
 * @brief Completion clear rows.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void completion_clear_rows(EditorTab *tab);
/**
 * @brief Completion insert word.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 */
void completion_insert_word(EditorTab *tab, const char *word);
/**
 * @brief Completion row activated.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param box The box supplied by the caller.
 * @param row The row supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void completion_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data);
/**
 * @brief Completion add row.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 */
void completion_add_row(EditorTab *tab, const char *word);
/**
 * @brief Completion timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean completion_timeout_cb(gpointer user_data);
/**
 * @brief Editor tab schedule completion.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_completion(EditorTab *tab);
/**
 * @brief Completion accept selected.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void completion_accept_selected(EditorTab *tab);
/**
 * @brief Completion select delta.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param delta The delta supplied by the caller.
 */
void completion_select_delta(EditorTab *tab, int delta);
/**
 * @brief Tab unit.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *tab_unit(EditorTab *tab);
/**
 * @brief Selection line bounds.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param start_line_out Output storage filled when the operation can provide a value.
 * @param end_line_out Output storage filled when the operation can provide a value.
 * @param has_selection_out Output storage filled when the operation can provide a value.
 */
void selection_line_bounds(EditorTab *tab, int *start_line_out, int *end_line_out, gboolean *has_selection_out);
/**
 * @brief Insert tab or indent.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void insert_tab_or_indent(EditorTab *tab);
/**
 * @brief Unindent line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 */
void unindent_line(EditorTab *tab, int line);
/**
 * @brief Unindent selection or line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void unindent_selection_or_line(EditorTab *tab);
/**
 * @brief On text view key pressed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param keyval The keyval supplied by the caller.
 * @param keycode The keycode supplied by the caller.
 * @param state The state supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean on_text_view_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
/**
 * @brief On text view key released.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param keyval The keyval supplied by the caller.
 * @param keycode The keycode supplied by the caller.
 * @param state The state supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_text_view_key_released(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data);
/**
 * @brief On close button clicked.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_close_button_clicked(GtkWidget *widget, gpointer user_data);
/**
 * @brief Highlight timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean highlight_timeout_cb(gpointer user_data);
/**
 * @brief On mark set.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param location The location supplied by the caller.
 * @param mark The mark supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_mark_set(GtkTextBuffer *buffer, GtkTextIter *location, GtkTextMark *mark, gpointer user_data);
/**
 * @brief On buffer changed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_buffer_changed(GtkTextBuffer *buffer, gpointer user_data);
/**
 * @brief Write backup if needed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean write_backup_if_needed(EditorTab *tab, const char *path);
/**
 * @brief Save to path.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean save_to_path(EditorTab *tab, const char *path);
/**
 * @brief Select match.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param start The start supplied by the caller.
 * @param end The end supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean select_match(EditorTab *tab, GtkTextIter *start, GtkTextIter *end);


/**
 * @brief Editor tab text rect to popover parent.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param rect The rect supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_text_rect_to_popover_parent(EditorTab *tab,
                                                GdkRectangle *rect);
/**
 * @brief Editor tab cursor rect to popover parent.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param rect The rect supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_cursor_rect_to_popover_parent(EditorTab *tab,
                                                  GdkRectangle *rect);
/**
 * @brief Editor tab place popover at cursor.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param popover The popover supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_place_popover_at_cursor(EditorTab *tab,
                                            GtkWidget *popover);
/**
 * @brief Editor tab reposition visible cursor popovers.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_reposition_visible_cursor_popovers(EditorTab *tab);

#endif /* GRAPTOS_EDITOR_TAB_PRIVATE_H */
