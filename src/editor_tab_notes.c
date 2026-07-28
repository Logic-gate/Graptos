/**
 * @file src/editor_tab_notes.c
 * @brief Graptoς editor tab notes module.
 * @details Notes sit beside source text until the user explicitly converts
 *          them into comments. The tab owns the transient popovers, while the
 *          storage module owns the project-local text database format.
 */

#include "editor_tab_private.h"

#include <glib/gstdio.h>

/**
 * @brief Note popover state.
 */
typedef struct {
    EditorTab *tab; /**< Tab that owns the note edit. */
    EditorNote *note; /**< Existing note when editing, or NULL when adding. */
    GtkWidget *popover; /**< One-shot popover. */
    GtkWidget *view; /**< Text view used for note input. */
    GtkWidget *status; /**< Status label below the note input. */
    gint start_line; /**< Zero-based first covered line. */
    gint start_col; /**< Zero-based start column. */
    gint end_line; /**< Zero-based last covered line. */
    gint end_col; /**< Zero-based end column. */
} NotePopoverState;

/**
 * @brief Free note popover state.
 * @param data Callback data supplied by GTK.
 */
static void note_popover_state_free(gpointer data) {
    NotePopoverState *state = data;
    g_free(state);
}

/**
 * @brief Return the note database path for a tab.
 * @param tab The editor tab whose file path is inspected.
 * @return Newly allocated database path, or NULL.
 */
static char *editor_tab_note_db_path(EditorTab *tab) {
    if (!tab || !tab->win || !tab->win->project_root || !tab->file_path) return NULL;
    return editor_notes_db_path(tab->win->project_root, tab->file_path);
}

void editor_tab_load_notes(EditorTab *tab) {
    if (!tab) return;
    g_autofree char *path = editor_tab_note_db_path(tab);
    GPtrArray *loaded = editor_notes_load(path);
    if (!loaded) loaded = g_ptr_array_new_with_free_func(editor_note_free);
    if (tab->notes) g_ptr_array_free(tab->notes, TRUE);
    tab->notes = loaded;
    tab->last_line_count = tab->buffer ? (guint)gtk_text_buffer_get_line_count(tab->buffer) : 0u;
    if (tab->gutter) gtk_widget_queue_draw(tab->gutter);
}

void editor_tab_save_notes(EditorTab *tab) {
    if (!tab) return;
    g_autofree char *path = editor_tab_note_db_path(tab);
    if (!path) {
        app_window_set_status(tab->win, "Open a project before saving notes.");
        return;
    }
    g_autoptr(GError) error = NULL;
    if (!editor_notes_save(path, tab->notes, &error)) {
        app_window_report_error(tab->win, "Could not save notes",
                                error ? error->message : path, FALSE);
    }
}

/**
 * @brief Compute note range from cursor or selection.
 * @param tab The editor tab whose buffer is inspected.
 * @param start_line_out Output first line.
 * @param start_col_out Output start column.
 * @param end_line_out Output last line.
 * @param end_col_out Output end column.
 */
static void note_range_from_selection(EditorTab *tab,
                                      gint *start_line_out,
                                      gint *start_col_out,
                                      gint *end_line_out,
                                      gint *end_col_out) {
    GtkTextIter start;
    GtkTextIter end;
    gboolean has_selection = tab && tab->buffer &&
        gtk_text_buffer_get_selection_bounds(tab->buffer, &start, &end);
    if (!has_selection) {
        GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &start, mark);
        end = start;
    }
    gint start_line = gtk_text_iter_get_line(&start);
    gint end_line = gtk_text_iter_get_line(&end);
    gint start_col = gtk_text_iter_get_line_offset(&start);
    gint end_col = gtk_text_iter_get_line_offset(&end);
    if (has_selection && end_col == 0 && end_line > start_line) {
        end_line--;
        GtkTextIter line_end;
        gtk_text_buffer_get_iter_at_line(tab->buffer, &line_end, end_line);
        if (!gtk_text_iter_ends_line(&line_end)) gtk_text_iter_forward_to_line_end(&line_end);
        end_col = gtk_text_iter_get_line_offset(&line_end);
    }
    if (start_line_out) *start_line_out = start_line;
    if (start_col_out) *start_col_out = start_col;
    if (end_line_out) *end_line_out = end_line;
    if (end_col_out) *end_col_out = end_col;
}

/**
 * @brief Read note text from a popover text view.
 * @param state Note popover state.
 * @return Newly allocated note text.
 */
static char *note_popover_text(NotePopoverState *state) {
    if (!state || !state->view) return g_strdup("");
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->view));
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

/**
 * @brief Close a note popover.
 * @param state Note popover state.
 */
static void note_popover_close(NotePopoverState *state) {
    if (!state || !state->popover) return;
    graptos_popover_hide(state->popover);
    graptos_widget_destroy(state->popover);
}

/**
 * @brief Close context popovers before a note action continues.
 * @details Note actions may be launched from the text view context menu or the
 *          gutter context menu. Closing the origin first prevents stacked menus
 *          from staying visible behind the editor popover.
 * @param tab The editor tab that owns possible context popovers.
 * @param source Button that may carry the origin popover.
 */
static void note_close_origin_popovers(EditorTab *tab, GtkWidget *source) {
    if (source) {
        GtkWidget *origin = g_object_get_data(G_OBJECT(source),
                                              "graptos-note-origin-popover");
        if (origin && GTK_IS_POPOVER(origin)) {
            if (tab && tab->text_view &&
                g_object_get_data(G_OBJECT(tab->text_view),
                                  "graptos-context-popover") == origin) {
                g_object_set_data(G_OBJECT(tab->text_view),
                                  "graptos-context-popover", NULL);
            }
            if (tab && tab->gutter &&
                g_object_get_data(G_OBJECT(tab->gutter),
                                  "graptos-context-popover") == origin) {
                g_object_set_data(G_OBJECT(tab->gutter),
                                  "graptos-context-popover", NULL);
            }
            graptos_widget_destroy(origin);
        }
    }
    if (tab && tab->text_view) {
        GtkWidget *text_popover =
            g_object_get_data(G_OBJECT(tab->text_view), "graptos-context-popover");
        if (text_popover && GTK_IS_POPOVER(text_popover)) {
            graptos_widget_destroy(text_popover);
            g_object_set_data(G_OBJECT(tab->text_view),
                              "graptos-context-popover", NULL);
        }
    }
    if (tab && tab->gutter) {
        GtkWidget *gutter_popover =
            g_object_get_data(G_OBJECT(tab->gutter), "graptos-context-popover");
        if (gutter_popover && GTK_IS_POPOVER(gutter_popover)) {
            graptos_widget_destroy(gutter_popover);
            g_object_set_data(G_OBJECT(tab->gutter),
                              "graptos-context-popover", NULL);
        }
    }
}

/**
 * @brief Save note popover contents.
 * @param button Button that emitted the signal.
 * @param user_data NotePopoverState.
 */
static void note_popover_save(GtkButton *button, gpointer user_data) {
    (void)button;
    NotePopoverState *state = user_data;
    if (!state || !state->tab) return;
    g_autofree char *text = note_popover_text(state);
    g_strstrip(text);
    if (!text || text[0] == '\0') {
        if (state->status) gtk_label_set_text(GTK_LABEL(state->status), "Note text is empty.");
        return;
    }
    if (state->note) {
        state->note->start_line = state->start_line;
        state->note->start_col = state->start_col;
        state->note->end_line = state->end_line;
        state->note->end_col = state->end_col;
        g_free(state->note->note);
        state->note->note = g_strdup(text);
    } else {
        EditorNote *note = editor_note_new(editor_notes_next_id(state->tab->notes),
                                           state->start_line,
                                           state->start_col,
                                           state->end_line,
                                           state->end_col,
                                           text);
        if (note) g_ptr_array_add(state->tab->notes, note);
    }
    editor_tab_save_notes(state->tab);
    if (state->tab->gutter) gtk_widget_queue_draw(state->tab->gutter);
    app_window_set_status(state->tab->win, "Note saved.");
    note_popover_close(state);
}

/**
 * @brief Cancel note popover edit.
 * @param button Button that emitted the signal.
 * @param user_data NotePopoverState.
 */
static void note_popover_cancel(GtkButton *button, gpointer user_data) {
    (void)button;
    note_popover_close(user_data);
}

/**
 * @brief Handle note input key presses.
 * @param controller Key controller.
 * @param keyval Key value.
 * @param keycode Key code.
 * @param state Modifier state.
 * @param user_data NotePopoverState.
 * @return TRUE when the key was handled.
 */
static gboolean note_input_key_pressed(GtkEventControllerKey *controller,
                                       guint keyval,
                                       guint keycode,
                                       GdkModifierType state,
                                       gpointer user_data) {
    (void)controller;
    (void)keycode;
    NotePopoverState *popover_state = user_data;
    (void)state;
    if (keyval == GDK_KEY_Escape) {
        note_popover_close(popover_state);
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Show a note editor popover.
 * @param tab The editor tab that owns the note.
 * @param note Existing note, or NULL for a new note.
 * @param line_hint Optional zero-based line used for gutter placement.
 */
static void editor_tab_show_note_popover(EditorTab *tab,
                                         EditorNote *note,
                                         gint line_hint) {
    if (!tab || !tab->text_view || !tab->popover_parent) return;
    NotePopoverState *state = g_new0(NotePopoverState, 1);
    if (!state) return;
    state->tab = tab;
    state->note = note;
    if (note) {
        state->start_line = note->start_line;
        state->start_col = note->start_col;
        state->end_line = note->end_line;
        state->end_col = note->end_col;
    } else {
        note_range_from_selection(tab, &state->start_line, &state->start_col,
                                  &state->end_line, &state->end_col);
    }

    GtkWidget *popover = gtk_popover_new();
    state->popover = popover;
    graptos_popover_attach(popover, tab->popover_parent);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "graptos-note-popover");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    graptos_set_all_margins(box, 8);
    GtkWidget *view = gtk_text_view_new();
    state->view = view;
    gtk_widget_add_css_class(view, "graptos-note-input");
    gtk_widget_set_size_request(view, 360, 130);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    if (note && note->note) gtk_text_buffer_set_text(buffer, note->note, -1);
    GtkEventController *keys = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
    g_signal_connect(keys, "key-pressed",
                     G_CALLBACK(note_input_key_pressed), state);
    gtk_widget_add_controller(view, keys);
    gtk_box_append(GTK_BOX(box), view);

    state->status = gtk_label_new("Escape closes without saving.");
    gtk_widget_add_css_class(state->status, "graptos-note-status");
    gtk_label_set_xalign(GTK_LABEL(state->status), 0.0f);
    gtk_box_append(GTK_BOX(box), state->status);

    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *cancel = graptos_flat_button_new("Cancel", NULL,
                                                G_CALLBACK(note_popover_cancel),
                                                state);
    GtkWidget *save = graptos_flat_button_new("Save", NULL,
                                              G_CALLBACK(note_popover_save),
                                              state);
    gtk_box_append(GTK_BOX(actions), cancel);
    gtk_box_append(GTK_BOX(actions), save);
    gtk_box_append(GTK_BOX(box), actions);
    gtk_popover_set_child(GTK_POPOVER(popover), box);
    g_object_set_data_full(G_OBJECT(popover), "graptos-note-state",
                           state, note_popover_state_free);

    gint target = note ? note->start_line : line_hint;
    if (target < 0) target = state->start_line;
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line(tab->buffer, &iter, MAX(0, target));
    gtk_text_buffer_place_cursor(tab->buffer, &iter);
    editor_tab_place_popover_at_cursor(tab, popover);
    graptos_popover_show(popover);
    gtk_widget_grab_focus(view);
}

void editor_tab_show_add_note(EditorTab *tab) {
    note_close_origin_popovers(tab, NULL);
    editor_tab_show_note_popover(tab, NULL, -1);
}

/**
 * @brief Delete a note.
 * @param button Button that emitted the signal.
 * @param user_data Note pointer stored on the button.
 */
static void note_delete_clicked(GtkButton *button, gpointer user_data) {
    EditorTab *tab = g_object_get_data(G_OBJECT(button), "graptos-note-tab");
    note_close_origin_popovers(tab, GTK_WIDGET(button));
    EditorNote *note = user_data;
    if (!tab || !tab->notes || !note) return;
    for (guint i = 0u; i < tab->notes->len; i++) {
        if (g_ptr_array_index(tab->notes, i) == note) {
            g_ptr_array_remove_index(tab->notes, i);
            break;
        }
    }
    editor_tab_save_notes(tab);
    if (tab->gutter) gtk_widget_queue_draw(tab->gutter);
    app_window_set_status(tab->win, "Note deleted.");
}

/**
 * @brief Edit a note from a context popover.
 * @param button Button that emitted the signal.
 * @param user_data Note pointer.
 */
static void note_edit_clicked(GtkButton *button, gpointer user_data) {
    EditorTab *tab = g_object_get_data(G_OBJECT(button), "graptos-note-tab");
    note_close_origin_popovers(tab, GTK_WIDGET(button));
    editor_tab_show_note_popover(tab, user_data, -1);
}

/**
 * @brief Convert one note to language comments.
 * @param button Button that emitted the signal.
 * @param user_data Note pointer.
 */
static void note_convert_clicked(GtkButton *button, gpointer user_data) {
    EditorTab *tab = g_object_get_data(G_OBJECT(button), "graptos-note-tab");
    note_close_origin_popovers(tab, GTK_WIDGET(button));
    EditorNote *note = user_data;
    if (!tab || !note || tab->locked || !tab->buffer) return;
    const char *comment = "#";
    if (tab->active_syntax && tab->active_syntax->line_comment &&
        tab->active_syntax->line_comment[0] != '\0') {
        comment = tab->active_syntax->line_comment;
    }
    GtkTextIter line_start;
    gtk_text_buffer_get_iter_at_line(tab->buffer, &line_start, note->start_line);
    GtkTextIter probe = line_start;
    while (!gtk_text_iter_ends_line(&probe)) {
        gunichar ch = gtk_text_iter_get_char(&probe);
        if (ch != ' ' && ch != '\t') break;
        gtk_text_iter_forward_char(&probe);
    }
    g_autofree char *indent = gtk_text_buffer_get_text(tab->buffer,
                                                       &line_start,
                                                       &probe,
                                                       FALSE);
    g_auto(GStrv) lines = g_strsplit(note->note ? note->note : "", "\n", -1);
    gtk_text_buffer_begin_user_action(tab->buffer);
    for (guint i = 0u; lines && lines[i]; i++) {
        g_autofree char *line = g_strdup_printf("%s%s %s\n",
                                                indent ? indent : "",
                                                comment,
                                                lines[i]);
        gtk_text_buffer_insert(tab->buffer, &line_start, line, -1);
    }
    gtk_text_buffer_end_user_action(tab->buffer);
    app_window_set_status(tab->win, "Note converted to comment.");
}

/**
 * @brief Add a context button bound to a note.
 * @param box Context popover box.
 * @param tab Editor tab.
 * @param note Note pointer.
 * @param label Button label.
 * @param callback Button callback.
 */
static void note_context_append(GtkWidget *box,
                                EditorTab *tab,
                                EditorNote *note,
                                const char *label,
                                GCallback callback,
                                GtkWidget *origin_popover) {
    GtkWidget *button = graptos_flat_button_new(label, NULL, callback, note);
    g_object_set_data(G_OBJECT(button), "graptos-note-tab", tab);
    g_object_set_data(G_OBJECT(button), "graptos-note-origin-popover",
                      origin_popover);
    gtk_box_append(GTK_BOX(box), button);
}

/**
 * @brief Destroy a one-shot note context popover.
 * @param popover Popover supplied by GTK.
 * @param user_data Callback data supplied by GTK.
 */
static void note_context_popover_closed(GtkPopover *popover,
                                        gpointer user_data) {
    GtkWidget *owner = user_data;
    if (owner) {
        GtkWidget *stored = g_object_get_data(G_OBJECT(owner),
                                              "graptos-context-popover");
        if (stored == GTK_WIDGET(popover)) {
            g_object_set_data(G_OBJECT(owner), "graptos-context-popover", NULL);
        }
    }
    graptos_widget_destroy(GTK_WIDGET(popover));
}

void on_gutter_right_click(GtkGestureClick *gesture,
                           int n_press,
                           double x,
                           double y,
                           gpointer user_data) {
    (void)n_press;
    (void)x;
    EditorTab *tab = user_data;
    GtkWidget *widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    if (!tab || !widget || !tab->text_view || !tab->buffer) return;
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);

    GtkTextView *view = GTK_TEXT_VIEW(tab->text_view);
    GdkRectangle visible;
    gtk_text_view_get_visible_rect(view, &visible);
    GtkTextIter iter;
    gint line_y = 0;
    gtk_text_view_get_line_at_y(view, &iter, visible.y + (gint)y, &line_y);
    gint line = gtk_text_iter_get_line(&iter);
    EditorNote *note = editor_notes_find_for_line(tab->notes, line);

    GtkWidget *popover = gtk_popover_new();
    graptos_popover_attach(popover, widget);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "graptos-context-popover");
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    graptos_set_all_margins(box, 6);
    if (note) {
        note_context_append(box, tab, note, "Convert to Comment",
                            G_CALLBACK(note_convert_clicked), popover);
        note_context_append(box, tab, note, "Edit Note",
                            G_CALLBACK(note_edit_clicked), popover);
        note_context_append(box, tab, note, "Delete Note",
                            G_CALLBACK(note_delete_clicked), popover);
    } else {
        GtkTextIter target;
        gtk_text_buffer_get_iter_at_line(tab->buffer, &target, line);
        gtk_text_buffer_place_cursor(tab->buffer, &target);
        GtkWidget *add = graptos_flat_button_new("Add Note", NULL,
                                                 G_CALLBACK(menu_add_note), tab);
        g_object_set_data(G_OBJECT(add), "graptos-note-origin-popover",
                          popover);
        gtk_box_append(GTK_BOX(box), add);
    }
    GdkRectangle rect = { .x = 1, .y = (int)y, .width = 1, .height = 1 };
    gtk_popover_set_child(GTK_POPOVER(popover), box);
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    g_object_set_data(G_OBJECT(widget), "graptos-context-popover", popover);
    g_signal_connect(popover, "closed",
                     G_CALLBACK(note_context_popover_closed), widget);
    graptos_popover_show(popover);
}

void on_gutter_motion(GtkEventControllerMotion *controller,
                      double x,
                      double y,
                      gpointer user_data) {
    EditorTab *tab = user_data;
    GtkWidget *widget = gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(controller));
    if (!tab || !widget || !tab->text_view || !tab->buffer) return;

    if (x > 18.0) {
        gtk_widget_set_tooltip_text(widget, NULL);
        return;
    }

    GtkTextView *view = GTK_TEXT_VIEW(tab->text_view);
    GdkRectangle visible;
    gtk_text_view_get_visible_rect(view, &visible);
    GtkTextIter iter;
    gint line_y = 0;
    gtk_text_view_get_line_at_y(view, &iter, visible.y + (gint)y, &line_y);
    EditorNote *note = editor_notes_find_for_line(tab->notes,
                                                  gtk_text_iter_get_line(&iter));
    gtk_widget_set_tooltip_text(widget, note ? note->note : NULL);
}
