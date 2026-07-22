/**
 * @file src/editor_tab_keys.c
 * @brief Graptoς editor tab keys module.
 * @details Key handling sits next to the buffer because indentation, auto-pairs, and
 *          navigation all mutate text. We keep those edits explicit so GTKSourceView is not
 *          surprised by hidden side effects.
 */

#include "editor_tab_private.h"


/**
 * @brief Text between iters.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param start The start supplied by the caller.
 * @param end The end supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *text_between_iters(GtkTextBuffer *buffer,
                                const GtkTextIter *start,
                                const GtkTextIter *end) {
    if (!buffer || !start || !end) return g_strdup("");
    return gtk_text_buffer_get_text(buffer, start, end, FALSE);
}

/**
 * @brief Line leading indent.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *line_leading_indent(const char *line) {
    if (!line) return g_strdup("");
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    return g_strndup(line, (gsize)(p - line));
}

/**
 * @brief Trimmed copy.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *trimmed_copy(const char *text) {
    char *copy = g_strdup(text ? text : "");
    if (!copy) return NULL;
    return g_strstrip(copy);
}

/**
 * @brief Syntax has suffix token.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tokens The tokens supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param fallback The fallback supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
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

/**
 * @brief Syntax has prefix token.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tokens The tokens supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param fallback The fallback supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
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

/**
 * @brief First indent opener.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *first_indent_opener(EditorTab *tab) {
    if (tab && tab->active_syntax && tab->active_syntax->indent_openers &&
        tab->active_syntax->indent_openers->len > 0u) {
        const char *token = g_ptr_array_index(tab->active_syntax->indent_openers, 0u);
        if (token && token[0] != '\0') return token;
    }
    return "{";
}

/**
 * @brief First indent closer.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *first_indent_closer(EditorTab *tab) {
    if (tab && tab->active_syntax && tab->active_syntax->indent_closers &&
        tab->active_syntax->indent_closers->len > 0u) {
        const char *token = g_ptr_array_index(tab->active_syntax->indent_closers, 0u);
        if (token && token[0] != '\0') return token;
    }
    return "}";
}

/**
 * @brief Auto indent enabled.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean auto_indent_enabled(EditorTab *tab) {
    return !tab || !tab->active_syntax || tab->active_syntax->auto_indent;
}

/**
 * @brief Insert block newline.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param iter The text iterator that anchors the lookup.
 * @param base_indent The base indent supplied by the caller.
 * @param unit The unit supplied by the caller.
 */
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

/**
 * @brief Line before has adjacent indent pair.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param trimmed_before The trimmed before supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
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

/**
 * @brief Handle return key.
 * @details Auto-indent is handled as one user action so undo feels natural.
 *          The special empty-line case handles `{|}` style pairs by opening a
 *          block with the cursor on the useful middle line.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
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

/**
 * @brief Menu undo.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_undo(GtkWidget *w, gpointer data) { (void)w; editor_tab_undo(data); }

/**
 * @brief Menu redo.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_redo(GtkWidget *w, gpointer data) { (void)w; editor_tab_redo(data); }

/**
 * @brief Menu cut.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_cut(GtkWidget *w, gpointer data) { (void)w; editor_tab_cut_clipboard(data); }

/**
 * @brief Menu copy.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_copy(GtkWidget *w, gpointer data) { (void)w; editor_tab_copy_clipboard(data); }

/**
 * @brief Menu paste.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_paste(GtkWidget *w, gpointer data) { (void)w; editor_tab_paste_clipboard(data); }

/**
 * @brief Menu select all.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_select_all(GtkWidget *w, gpointer data) { (void)w; editor_tab_select_all(data); }

/**
 * @brief Menu cut line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_cut_line(GtkWidget *w, gpointer data) { (void)w; editor_tab_cut_line(data); }

/**
 * @brief Menu paste line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_paste_line(GtkWidget *w, gpointer data) { (void)w; editor_tab_paste_cut_line(data); }

/**
 * @brief Menu comment.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_comment(GtkWidget *w, gpointer data) { (void)w; editor_tab_toggle_comment(data); }

/**
 * @brief Menu complete.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param w The w supplied by the caller.
 * @param data The callback context passed by the caller.
 */
void menu_complete(GtkWidget *w, gpointer data) { (void)w; editor_tab_show_completion(data, TRUE); }


/**
 * @brief Tab unit.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *tab_unit(EditorTab *tab) {
    if (!tab || !tab->insert_spaces) return g_strdup("\t");
    guint width = tab->tab_width == 0u ? 4u : tab->tab_width;
    if (width > 16u) width = 16u;
    return g_strnfill(width, ' ');
}


/**
 * @brief Selection line bounds.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param start_line_out Output storage filled when the operation can provide a value.
 * @param end_line_out Output storage filled when the operation can provide a value.
 * @param has_selection_out Output storage filled when the operation can provide a value.
 */
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


/**
 * @brief Insert tab or indent.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
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


/**
 * @brief Unindent line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 */
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


/**
 * @brief Unindent selection or line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
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


/**
 * @brief Key event is plain editor text.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param state The state supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean key_event_is_plain_editor_text(GdkModifierType state) {
    GdkModifierType blocked =
        GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_META_MASK |
        GDK_SUPER_MASK | GDK_HYPER_MASK;
    return (state & blocked) == 0;
}

/**
 * @brief Text for keyval.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param keyval The keyval supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *text_for_keyval(guint keyval) {
    gunichar ch = gdk_keyval_to_unicode(keyval);
    if (ch == 0u || g_unichar_iscntrl(ch)) return NULL;

    char utf8[8] = {0};
    int len = g_unichar_to_utf8(ch, utf8);
    if (len <= 0) return NULL;
    utf8[len] = '\0';
    return g_strdup(utf8);
}

/**
 * @brief Syntax pair matching open text.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static SyntaxPair *syntax_pair_for_open(EditorTab *tab, const char *text) {
    static SyntaxPair fallback_pairs[] = {
        { "(", ")" },
        { "{", "}" },
        { "[", "]" }
    };

    if (!text) {
        return NULL;
    }

    if (tab && tab->active_syntax && tab->active_syntax->close_pairs &&
        tab->active_syntax->close_pairs->len > 0u) {
        for (guint i = 0u; i < tab->active_syntax->close_pairs->len; i++) {
            SyntaxPair *pair = g_ptr_array_index(tab->active_syntax->close_pairs,
                                                 i);
            if (pair && g_strcmp0(pair->open, text) == 0 &&
                pair->close && pair->close[0] != '\0') {
                return pair;
            }
        }
        return NULL;
    }

    for (guint i = 0u; i < G_N_ELEMENTS(fallback_pairs); i++) {
        if (g_strcmp0(fallback_pairs[i].open, text) == 0) {
            return &fallback_pairs[i];
        }
    }
    return NULL;
}

/**
 * @brief Syntax pair matching close text.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static SyntaxPair *syntax_pair_for_close(EditorTab *tab, const char *text) {
    static SyntaxPair fallback_pairs[] = {
        { "(", ")" },
        { "{", "}" },
        { "[", "]" }
    };

    if (!text) {
        return NULL;
    }

    if (tab && tab->active_syntax && tab->active_syntax->close_pairs &&
        tab->active_syntax->close_pairs->len > 0u) {
        for (guint i = 0u; i < tab->active_syntax->close_pairs->len; i++) {
            SyntaxPair *pair = g_ptr_array_index(tab->active_syntax->close_pairs,
                                                 i);
            if (pair && g_strcmp0(pair->close, text) == 0) return pair;
        }
        return NULL;
    }

    for (guint i = 0u; i < G_N_ELEMENTS(fallback_pairs); i++) {
        if (g_strcmp0(fallback_pairs[i].close, text) == 0) {
            return &fallback_pairs[i];
        }
    }
    return NULL;
}

/**
 * @brief Cursor is before text.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param text The text fragment supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean cursor_is_before_text(EditorTab *tab, const char *text) {
    if (!tab || !tab->buffer || !text || text[0] == '\0') return FALSE;

    GtkTextIter start;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &start, mark);

    GtkTextIter end = start;
    gint chars = (gint)g_utf8_strlen(text, -1);
    if (chars <= 0 || !gtk_text_iter_forward_chars(&end, chars)) {
        return FALSE;
    }

    char *next = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
    gboolean same = g_strcmp0(next, text) == 0;
    g_free(next);
    return same;
}

/**
 * @brief Move cursor forward by text width.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param text The text fragment supplied by the caller.
 */
static void move_cursor_over_text(EditorTab *tab, const char *text) {
    if (!tab || !tab->buffer || !text) return;

    GtkTextIter iter;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &iter, mark);
    gint chars = (gint)g_utf8_strlen(text, -1);
    if (chars > 0 && gtk_text_iter_forward_chars(&iter, chars)) {
        gtk_text_buffer_place_cursor(tab->buffer, &iter);
    }
}

/**
 * @brief Insert close pair around cursor or selection.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param pair The pair supplied by the caller.
 */
static void insert_close_pair(EditorTab *tab, SyntaxPair *pair) {
    if (!tab || !tab->buffer || !pair || !pair->open || !pair->close) return;

    GtkTextIter start;
    GtkTextIter end;
    gboolean has_selection =
        gtk_text_buffer_get_selection_bounds(tab->buffer, &start, &end);

    gtk_text_buffer_begin_user_action(tab->buffer);
    if (has_selection) {
        gtk_text_buffer_insert(tab->buffer, &end, pair->close, -1);
        gtk_text_buffer_insert(tab->buffer, &start, pair->open, -1);
    } else {
        GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &start, mark);
        gtk_text_buffer_insert(tab->buffer, &start, pair->open, -1);
        GtkTextMark *cursor_mark =
            gtk_text_buffer_create_mark(tab->buffer, NULL, &start, TRUE);
        gtk_text_buffer_insert(tab->buffer, &start, pair->close, -1);
        GtkTextIter cursor;
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, cursor_mark);
        gtk_text_buffer_place_cursor(tab->buffer, &cursor);
        gtk_text_buffer_delete_mark(tab->buffer, cursor_mark);
    }
    gtk_text_buffer_end_user_action(tab->buffer);
}

/**
 * @brief Handle close pair key.
 * @details Close pairs come from syntax data, not hardcoded language rules.
 *          When the cursor is already before the closer, typing it steps over
 *          the existing character instead of duplicating it.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param keyval The keyval supplied by the caller.
 * @param state The state supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean handle_close_pair_key(EditorTab *tab, guint keyval,
                                      GdkModifierType state) {
    if (!tab || tab->locked || !key_event_is_plain_editor_text(state)) {
        return FALSE;
    }

    char *text = text_for_keyval(keyval);
    if (!text) return FALSE;

    SyntaxPair *open_pair = syntax_pair_for_open(tab, text);
    if (open_pair) {
        insert_close_pair(tab, open_pair);
        g_free(text);
        return TRUE;
    }

    SyntaxPair *close_pair = syntax_pair_for_close(tab, text);
    if (close_pair && cursor_is_before_text(tab, text)) {
        move_cursor_over_text(tab, text);
        g_free(text);
        return TRUE;
    }

    g_free(text);
    return FALSE;
}

/**
 * @brief On text view key pressed.
 * @details This is the editor's traffic cop. Completion, Alt-inspection,
 *          close-pairs, indentation, and normal text insertion all compete for
 *          the same key event, so the order here is part of the behavior.
 * @param controller The controller supplied by the caller.
 * @param keyval The keyval supplied by the caller.
 * @param keycode The keycode supplied by the caller.
 * @param state The state supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean on_text_view_key_pressed(GtkEventControllerKey *controller,
                                  guint keyval, guint keycode,
                                  GdkModifierType state,
                                  gpointer user_data) {
    (void)controller;
    (void)keycode;
    EditorTab *tab = user_data;
    if (!tab) return FALSE;

    guint key = gdk_keyval_to_lower(keyval);
    gboolean alt = (state & GDK_ALT_MASK) != 0;
    gboolean shift = (state & GDK_SHIFT_MASK) != 0;

    if (alt || keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R) {
        tab->inspect_reference_active = TRUE;
        maybe_show_color_preview(tab);
        editor_tab_show_reference_at_pointer_or_cursor(tab);
    }

    if (completion_is_visible(tab)) {
        if (key == GDK_KEY_Escape) { editor_tab_hide_completion(tab); return TRUE; }
        if (key == GDK_KEY_Down) { completion_select_delta(tab, 1); return TRUE; }
        if (key == GDK_KEY_Up) { completion_select_delta(tab, -1); return TRUE; }
        if (key == GDK_KEY_Tab && !shift) {
            completion_accept_selected(tab);
            return TRUE;
        }
        editor_tab_hide_completion(tab);
    }

    if (handle_close_pair_key(tab, keyval, state)) return TRUE;
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


/**
 * @brief On text view key released.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param controller The controller supplied by the caller.
 * @param keyval The keyval supplied by the caller.
 * @param keycode The keycode supplied by the caller.
 * @param state The state supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void on_text_view_key_released(GtkEventControllerKey *controller,
                                guint keyval, guint keycode,
                                GdkModifierType state, gpointer user_data) {
    (void)controller;
    (void)keycode;
    (void)state;
    EditorTab *tab = user_data;
    if (!tab) return;
    if (keyval == GDK_KEY_Alt_L || keyval == GDK_KEY_Alt_R) {
        tab->inspect_reference_active = FALSE;
        if (!tab->hover_pointer_inside) hide_hover_preview(tab);
    }
}
