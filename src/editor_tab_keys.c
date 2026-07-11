#include "editor_tab_private.h"


static char *text_between_iters(GtkTextBuffer *buffer,
                                const GtkTextIter *start,
                                const GtkTextIter *end) {
    if (!buffer || !start || !end) return g_strdup("");
    return gtk_text_buffer_get_text(buffer, start, end, FALSE);
}

static char *line_leading_indent(const char *line) {
    if (!line) return g_strdup("");
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    return g_strndup(line, (gsize)(p - line));
}

static char *trimmed_copy(const char *text) {
    char *copy = g_strdup(text ? text : "");
    if (!copy) return NULL;
    return g_strstrip(copy);
}

static gboolean syntax_has_suffix_token(GPtrArray *tokens,
                                        const char *text,
                                        const char *fallback) {
    if (!text) return FALSE;
    if (tokens && tokens->len > 0u) {
        for (guint i = 0u; i < tokens->len; i++) {
            const char *token = g_ptr_array_index(tokens, i);
            if (token && token[0] != '\0' && g_str_has_suffix(text, token)) return TRUE;
        }
        return FALSE;
    }
    return fallback && g_str_has_suffix(text, fallback);
}

static gboolean syntax_has_prefix_token(GPtrArray *tokens,
                                        const char *text,
                                        const char *fallback) {
    if (!text) return FALSE;
    if (tokens && tokens->len > 0u) {
        for (guint i = 0u; i < tokens->len; i++) {
            const char *token = g_ptr_array_index(tokens, i);
            if (token && token[0] != '\0' && g_str_has_prefix(text, token)) return TRUE;
        }
        return FALSE;
    }
    return fallback && g_str_has_prefix(text, fallback);
}

static const char *first_indent_opener(EditorTab *tab) {
    if (tab && tab->active_syntax && tab->active_syntax->indent_openers &&
        tab->active_syntax->indent_openers->len > 0u) {
        const char *token = g_ptr_array_index(tab->active_syntax->indent_openers, 0u);
        if (token && token[0] != '\0') return token;
    }
    return "{";
}

static const char *first_indent_closer(EditorTab *tab) {
    if (tab && tab->active_syntax && tab->active_syntax->indent_closers &&
        tab->active_syntax->indent_closers->len > 0u) {
        const char *token = g_ptr_array_index(tab->active_syntax->indent_closers, 0u);
        if (token && token[0] != '\0') return token;
    }
    return "}";
}

static gboolean auto_indent_enabled(EditorTab *tab) {
    return !tab || !tab->active_syntax || tab->active_syntax->auto_indent;
}

static void insert_block_newline(EditorTab *tab,
                                 GtkTextIter *iter,
                                 const char *base_indent,
                                 const char *unit) {
    GtkTextBuffer *buffer = tab->buffer;
    gtk_text_buffer_insert(buffer, iter, "\n", -1);
    if (base_indent) gtk_text_buffer_insert(buffer, iter, base_indent, -1);
    if (unit) gtk_text_buffer_insert(buffer, iter, unit, -1);

    GtkTextMark *cursor_mark = gtk_text_buffer_create_mark(buffer, NULL, iter, TRUE);

    gtk_text_buffer_insert(buffer, iter, "\n", -1);
    if (base_indent) gtk_text_buffer_insert(buffer, iter, base_indent, -1);

    GtkTextIter cursor;
    gtk_text_buffer_get_iter_at_mark(buffer, &cursor, cursor_mark);
    gtk_text_buffer_place_cursor(buffer, &cursor);
    gtk_text_buffer_delete_mark(buffer, cursor_mark);
}

static gboolean line_before_has_adjacent_indent_pair(EditorTab *tab,
                                                     const char *trimmed_before) {
    if (!tab || !trimmed_before) return FALSE;
    const char *closer = first_indent_closer(tab);
    const char *opener = first_indent_opener(tab);
    if (!g_str_has_suffix(trimmed_before, closer)) return FALSE;

    gsize len = strlen(trimmed_before);
    gsize closer_len = strlen(closer);
    if (closer_len >= len) return FALSE;

    char *without_closer = g_strndup(trimmed_before, len - closer_len);
    if (!without_closer) return FALSE;
    g_strstrip(without_closer);
    gboolean result = g_str_has_suffix(without_closer, opener);
    g_free(without_closer);
    return result;
}

static gboolean handle_return_key(EditorTab *tab) {
    if (!tab || tab->locked || !tab->buffer || !auto_indent_enabled(tab)) return FALSE;

    GtkTextIter cursor;
    GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, insert);

    GtkTextIter line_start = cursor;
    gtk_text_iter_set_line_offset(&line_start, 0);
    GtkTextIter line_end = cursor;
    if (!gtk_text_iter_ends_line(&line_end)) gtk_text_iter_forward_to_line_end(&line_end);

    char *before = text_between_iters(tab->buffer, &line_start, &cursor);
    char *after = text_between_iters(tab->buffer, &cursor, &line_end);
    char *base_indent = line_leading_indent(before);
    char *unit = tab_unit(tab);
    char *before_trimmed = trimmed_copy(before);
    char *after_trimmed = trimmed_copy(after);

    gboolean opens_block = syntax_has_suffix_token(tab->active_syntax ? tab->active_syntax->indent_openers : NULL,
                                                   before_trimmed, "{");
    gboolean closes_after = syntax_has_prefix_token(tab->active_syntax ? tab->active_syntax->indent_closers : NULL,
                                                    after_trimmed, "}");
    gtk_text_buffer_begin_user_action(tab->buffer);

    if (before_trimmed && after_trimmed && after_trimmed[0] == '\0' &&
        line_before_has_adjacent_indent_pair(tab, before_trimmed)) {
        GtkTextIter delete_start = cursor;
        if (gtk_text_iter_backward_char(&delete_start)) {
            GtkTextIter delete_end = cursor;
            gtk_text_buffer_delete(tab->buffer, &delete_start, &delete_end);
            cursor = delete_start;
            insert_block_newline(tab, &cursor, base_indent, unit);
        }
    } else if (opens_block && closes_after) {
        insert_block_newline(tab, &cursor, base_indent, unit);
    } else {
        gtk_text_buffer_insert(tab->buffer, &cursor, "\n", -1);
        if (base_indent) gtk_text_buffer_insert(tab->buffer, &cursor, base_indent, -1);
        if (opens_block && unit) gtk_text_buffer_insert(tab->buffer, &cursor, unit, -1);
    }

    gtk_text_buffer_end_user_action(tab->buffer);

    g_free(before);
    g_free(after);
    g_free(base_indent);
    g_free(unit);
    g_free(before_trimmed);
    g_free(after_trimmed);
    return TRUE;
}

void menu_undo(GtkWidget *w, gpointer data) { (void)w; editor_tab_undo(data); }

void menu_redo(GtkWidget *w, gpointer data) { (void)w; editor_tab_redo(data); }

void menu_cut(GtkWidget *w, gpointer data) { (void)w; editor_tab_cut_clipboard(data); }

void menu_copy(GtkWidget *w, gpointer data) { (void)w; editor_tab_copy_clipboard(data); }

void menu_paste(GtkWidget *w, gpointer data) { (void)w; editor_tab_paste_clipboard(data); }

void menu_select_all(GtkWidget *w, gpointer data) { (void)w; editor_tab_select_all(data); }

void menu_cut_line(GtkWidget *w, gpointer data) { (void)w; editor_tab_cut_line(data); }

void menu_paste_line(GtkWidget *w, gpointer data) { (void)w; editor_tab_paste_cut_line(data); }

void menu_comment(GtkWidget *w, gpointer data) { (void)w; editor_tab_toggle_comment(data); }

void menu_complete(GtkWidget *w, gpointer data) { (void)w; editor_tab_show_completion(data, TRUE); }


char *tab_unit(EditorTab *tab) {
    if (!tab || !tab->insert_spaces) return g_strdup("\t");
    guint width = tab->tab_width == 0u ? 4u : tab->tab_width;
    if (width > 16u) width = 16u;
    return g_strnfill(width, ' ');
}


void selection_line_bounds(EditorTab *tab, int *start_line_out, int *end_line_out, gboolean *has_selection_out) {
    GtkTextIter start;
    GtkTextIter end;
    gboolean has_selection = gtk_text_buffer_get_selection_bounds(tab->buffer, &start, &end);
    if (!has_selection) {
        GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &start, mark);
        end = start;
    }
    int start_line = gtk_text_iter_get_line(&start);
    int end_line = gtk_text_iter_get_line(&end);
    if (has_selection && gtk_text_iter_get_line_offset(&end) == 0 && end_line > start_line) end_line--;
    if (start_line_out) *start_line_out = start_line;
    if (end_line_out) *end_line_out = end_line;
    if (has_selection_out) *has_selection_out = has_selection;
}


void insert_tab_or_indent(EditorTab *tab) {
    if (!tab || tab->locked) return;
    char *unit = tab_unit(tab);
    if (!unit) return;

    int start_line = 0;
    int end_line = 0;
    gboolean has_selection = FALSE;
    selection_line_bounds(tab, &start_line, &end_line, &has_selection);

    gtk_text_buffer_begin_user_action(tab->buffer);
    if (has_selection) {
        for (int line = start_line; line <= end_line; line++) {
            GtkTextIter iter;
            gtk_text_buffer_get_iter_at_line(tab->buffer, &iter, line);
            gtk_text_buffer_insert(tab->buffer, &iter, unit, -1);
        }
    } else {
        GtkTextIter iter;
        GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &iter, mark);
        gtk_text_buffer_insert(tab->buffer, &iter, unit, -1);
    }
    gtk_text_buffer_end_user_action(tab->buffer);
    g_free(unit);
}


void unindent_line(EditorTab *tab, int line) {
    if (!tab || tab->locked || line < 0) return;
    GtkTextIter start;
    gtk_text_buffer_get_iter_at_line(tab->buffer, &start, line);
    GtkTextIter end = start;

    if (!gtk_text_iter_ends_line(&end) && gtk_text_iter_get_char(&end) == '\t') {
        gtk_text_iter_forward_char(&end);
        gtk_text_buffer_delete(tab->buffer, &start, &end);
        return;
    }

    guint width = tab->tab_width == 0u ? 4u : tab->tab_width;
    if (width > 16u) width = 16u;
    guint spaces = 0u;
    while (spaces < width && !gtk_text_iter_ends_line(&end) && gtk_text_iter_get_char(&end) == ' ') {
        gtk_text_iter_forward_char(&end);
        spaces++;
    }
    if (spaces > 0u) gtk_text_buffer_delete(tab->buffer, &start, &end);
}


void unindent_selection_or_line(EditorTab *tab) {
    if (!tab || tab->locked) return;
    int start_line = 0;
    int end_line = 0;
    gboolean has_selection = FALSE;
    selection_line_bounds(tab, &start_line, &end_line, &has_selection);
    (void)has_selection;

    gtk_text_buffer_begin_user_action(tab->buffer);
    for (int line = start_line; line <= end_line; line++) unindent_line(tab, line);
    gtk_text_buffer_end_user_action(tab->buffer);
}


gboolean on_text_view_key_pressed(GtkEventControllerKey *controller,
                                  guint keyval, guint keycode,
                                  GdkModifierType state,
                                  gpointer user_data) {
    (void)controller;
    (void)keycode;
    EditorTab *tab = user_data;
    if (!tab) return FALSE;

    guint key = gdk_keyval_to_lower(keyval);
    gboolean ctrl = (state & GDK_CONTROL_MASK) != 0;
    gboolean alt = (state & GDK_ALT_MASK) != 0;
    gboolean shift = (state & GDK_SHIFT_MASK) != 0;

    if (alt || keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R) {
        tab->inspect_reference_active = TRUE;
        maybe_show_color_preview(tab);
        editor_tab_show_reference_at_pointer_or_cursor(tab);
    }

    if (completion_is_visible(tab)) {
        if (key == GDK_KEY_Escape) { editor_tab_hide_completion(tab); return TRUE; }
        if (!tab->completion_manual) {
            editor_tab_hide_completion(tab);
        } else {
            if (key == GDK_KEY_Down) { completion_select_delta(tab, 1); return TRUE; }
            if (key == GDK_KEY_Up) { completion_select_delta(tab, -1); return TRUE; }
            if (key == GDK_KEY_Return || key == GDK_KEY_KP_Enter || key == GDK_KEY_Tab) {
                completion_accept_selected(tab);
                return TRUE;
            }
        }
    }

    if (ctrl && key == GDK_KEY_space) {
        editor_tab_show_completion(tab, TRUE);
        return TRUE;
    }
    if (key == GDK_KEY_Return || key == GDK_KEY_KP_Enter) {
        return handle_return_key(tab);
    }
    if (keyval == GDK_KEY_ISO_Left_Tab || (key == GDK_KEY_Tab && shift)) {
        unindent_selection_or_line(tab);
        return TRUE;
    }
    if (key == GDK_KEY_Tab) {
        insert_tab_or_indent(tab);
        return TRUE;
    }
    return FALSE;
}


void on_text_view_key_released(GtkEventControllerKey *controller,
                                guint keyval, guint keycode,
                                GdkModifierType state, gpointer user_data) {
    (void)controller;
    (void)keycode;
    (void)state;
    EditorTab *tab = user_data;
    if (!tab) return;
    guint key = gdk_keyval_to_lower(keyval);
    if (keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R) {
        tab->inspect_reference_active = FALSE;
        if (!tab->hover_pointer_inside) hide_hover_preview(tab);
    }
}


