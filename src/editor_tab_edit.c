/**
 * @file src/editor_tab_edit.c
 * @brief Graptoς editor tab edit module.
 * @details Editor tabs hold the real editing surface. We split the implementation by
 *          lifecycle, input, rendering, preview, and transient UI because each part has
 *          different timing and cleanup pressure.
 */

#include "editor_tab_private.h"

/**
 * @brief Editor tab cut clipboard.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_cut_clipboard(EditorTab *tab) {
    if (!tab || tab->locked) return;
    GdkClipboard *clipboard = gtk_widget_get_clipboard(tab->text_view);
    gtk_text_buffer_cut_clipboard(tab->buffer, clipboard, TRUE);
}


/**
 * @brief Editor tab copy clipboard.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_copy_clipboard(EditorTab *tab) {
    if (!tab) return;
    GdkClipboard *clipboard = gtk_widget_get_clipboard(tab->text_view);
    gtk_text_buffer_copy_clipboard(tab->buffer, clipboard);
}


/**
 * @brief Editor tab paste clipboard.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_paste_clipboard(EditorTab *tab) {
    if (!tab || tab->locked) return;
    GdkClipboard *clipboard = gtk_widget_get_clipboard(tab->text_view);
    gtk_text_buffer_paste_clipboard(tab->buffer, clipboard, NULL, TRUE);
}


/**
 * @brief Editor tab select all.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_select_all(EditorTab *tab) {
    if (!tab) return;
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
    gtk_text_buffer_select_range(tab->buffer, &start, &end);
}


/**
 * @brief Editor tab cut line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_cut_line(EditorTab *tab) {
    if (!tab || tab->locked) return;
    GtkTextIter iter;
    GtkTextIter start;
    GtkTextIter end;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &iter, mark);
    start = iter;
    gtk_text_iter_set_line_offset(&start, 0);
    end = start;
    if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
    if (!gtk_text_iter_is_end(&end)) gtk_text_iter_forward_char(&end);
    g_free(tab->kill_buffer);
    tab->kill_buffer = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
    gtk_text_buffer_delete(tab->buffer, &start, &end);
}


/**
 * @brief Editor tab paste cut line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_paste_cut_line(EditorTab *tab) {
    if (!tab || tab->locked || !tab->kill_buffer) return;
    GtkTextIter iter;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &iter, mark);
    insert_text_tagless(tab->buffer, &iter, tab->kill_buffer);
}


/**
 * @brief Select match.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param start The start supplied by the caller.
 * @param end The end supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean select_match(EditorTab *tab, GtkTextIter *start, GtkTextIter *end) {
    if (!tab || !start || !end) return FALSE;
    gtk_text_buffer_select_range(tab->buffer, start, end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(tab->text_view), start, 0.18, FALSE, 0.0, 0.0);
    return TRUE;
}


/**
 * @brief Editor tab find.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param query The query supplied by the caller.
 * @param backwards The backwards supplied by the caller.
 */
void editor_tab_find(EditorTab *tab, const char *query, gboolean backwards) {
    if (!tab || !query || query[0] == '\0') return;
    g_free(tab->search_text);
    g_free(tab->completion_prefix);
    tab->search_text = g_strdup(query);

    GtkTextIter insert;
    GtkTextIter match_start;
    GtkTextIter match_end;
    GtkTextIter doc_start;
    GtkTextIter doc_end;
    GtkTextSearchFlags flags = GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_CASE_INSENSITIVE;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &insert, mark);
    gtk_text_buffer_get_bounds(tab->buffer, &doc_start, &doc_end);

    gboolean found = FALSE;
    if (backwards) {
        found = gtk_text_iter_backward_search(&insert, query, flags, &match_start, &match_end, &doc_start);
        if (!found) found = gtk_text_iter_backward_search(&doc_end, query, flags, &match_start, &match_end, &doc_start);
    } else {
        if (gtk_text_buffer_get_has_selection(tab->buffer)) {
            GtkTextIter sel_start;
            GtkTextIter sel_end;
            gtk_text_buffer_get_selection_bounds(tab->buffer, &sel_start, &sel_end);
            insert = sel_end;
        }
        found = gtk_text_iter_forward_search(&insert, query, flags, &match_start, &match_end, &doc_end);
        if (!found) found = gtk_text_iter_forward_search(&doc_start, query, flags, &match_start, &match_end, &doc_end);
    }
    if (found) {
        select_match(tab, &match_start, &match_end);
    } else {
        dialog_info(app_window_gtk(tab->win), "Text not found", query);
    }
}


/**
 * @brief Editor tab replace current.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param find The find supplied by the caller.
 * @param replace The replace supplied by the caller.
 */
void editor_tab_replace_current(EditorTab *tab, const char *find, const char *replace) {
    if (!tab || tab->locked || !find || find[0] == '\0' || !replace) return;
    GtkTextIter start;
    GtkTextIter end;
    if (gtk_text_buffer_get_selection_bounds(tab->buffer, &start, &end)) {
        char *selected = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
        if (selected && g_ascii_strcasecmp(selected, find) == 0) {
            gtk_text_buffer_begin_user_action(tab->buffer);
            gtk_text_buffer_delete(tab->buffer, &start, &end);
            gtk_text_buffer_insert(tab->buffer, &start, replace, -1);
            gtk_text_buffer_end_user_action(tab->buffer);
        }
        g_free(selected);
    }
    editor_tab_find(tab, find, FALSE);
}


/**
 * @brief Editor tab replace all.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param find The find supplied by the caller.
 * @param replace The replace supplied by the caller.
 */
void editor_tab_replace_all(EditorTab *tab, const char *find, const char *replace) {
    if (!tab || tab->locked || !find || find[0] == '\0' || !replace) return;
    GtkTextIter search_from;
    GtkTextIter doc_end;
    GtkTextIter match_start;
    GtkTextIter match_end;
    GtkTextSearchFlags flags = GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_CASE_INSENSITIVE;
    gtk_text_buffer_get_start_iter(tab->buffer, &search_from);
    gtk_text_buffer_get_end_iter(tab->buffer, &doc_end);
    guint count = 0;
    gtk_text_buffer_begin_user_action(tab->buffer);
    while (gtk_text_iter_forward_search(&search_from, find, flags, &match_start, &match_end, &doc_end)) {
        gtk_text_buffer_delete(tab->buffer, &match_start, &match_end);
        gtk_text_buffer_insert(tab->buffer, &match_start, replace, -1);
        search_from = match_start;
        count++;
        if (count > 100000u) break;
    }
    gtk_text_buffer_end_user_action(tab->buffer);
    char *msg = g_strdup_printf("Replaced %u occurrence%s.", count, count == 1u ? "" : "s");
    dialog_info(app_window_gtk(tab->win), "Replace all complete", msg);
    g_free(msg);
}


/**
 * @brief Editor tab toggle comment.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_toggle_comment(EditorTab *tab) {
    if (!tab || tab->locked) return;
    const char *comment = "#";
    if (tab->active_syntax && tab->active_syntax->line_comment && tab->active_syntax->line_comment[0] != '\0') {
        comment = tab->active_syntax->line_comment;
    }

    GtkTextIter sel_start;
    GtkTextIter sel_end;
    if (!gtk_text_buffer_get_selection_bounds(tab->buffer, &sel_start, &sel_end)) {
        GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
        gtk_text_buffer_get_iter_at_mark(tab->buffer, &sel_start, mark);
        sel_end = sel_start;
    }

    int start_line = gtk_text_iter_get_line(&sel_start);
    int end_line = gtk_text_iter_get_line(&sel_end);
    if (gtk_text_iter_get_line_offset(&sel_end) == 0 && end_line > start_line) end_line--;

    gtk_text_buffer_begin_user_action(tab->buffer);
    for (int line = start_line; line <= end_line; line++) {
        GtkTextIter line_start;
        gtk_text_buffer_get_iter_at_line(tab->buffer, &line_start, line);
        GtkTextIter probe = line_start;
        while (!gtk_text_iter_ends_line(&probe)) {
            gunichar c = gtk_text_iter_get_char(&probe);
            if (!g_unichar_isspace(c)) break;
            gtk_text_iter_forward_char(&probe);
        }
        GtkTextIter after = probe;
        gboolean already = TRUE;
        for (const char *p = comment; *p; p = g_utf8_next_char(p)) {
            if (gtk_text_iter_is_end(&after) || gtk_text_iter_get_char(&after) != g_utf8_get_char(p)) {
                already = FALSE;
                break;
            }
            gtk_text_iter_forward_char(&after);
        }
        if (already) {
            gtk_text_buffer_delete(tab->buffer, &probe, &after);
            GtkTextIter space = probe;
            if (!gtk_text_iter_ends_line(&space) && gtk_text_iter_get_char(&space) == ' ') {
                GtkTextIter next = space;
                gtk_text_iter_forward_char(&next);
                gtk_text_buffer_delete(tab->buffer, &space, &next);
            }
        } else {
            gtk_text_buffer_insert(tab->buffer, &probe, comment, -1);
            gtk_text_buffer_insert(tab->buffer, &probe, " ", 1);
        }
    }
    gtk_text_buffer_end_user_action(tab->buffer);
}


/**
 * @brief Editor tab go to line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_go_to_line(EditorTab *tab) {
    if (!tab) return;
    char *line_text = dialog_prompt_text(app_window_gtk(tab->win), "Go to Line", "Line:", NULL);
    if (!line_text) return;
    char *end = NULL;
    errno = 0;
    long requested = strtol(line_text, &end, 10);
    if (errno == 0 && end != line_text && requested > 0) {
        int max_lines = gtk_text_buffer_get_line_count(tab->buffer);
        if (requested > max_lines) requested = max_lines;
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_line(tab->buffer, &iter, (int)requested - 1);
        gtk_text_buffer_place_cursor(tab->buffer, &iter);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(tab->text_view), &iter, 0.2, FALSE, 0.0, 0.0);
    }
    g_free(line_text);
}

/**
 * @brief Convert a byte offset in UTF-8 text to a GtkTextBuffer character offset.
 * @details Formatter range detection works in bytes because it scans C-like
 *          source. GTK iter offsets are characters, so the editor bridges the
 *          two representations at the buffer boundary.
 * @param text UTF-8 buffer text.
 * @param byte_offset Byte offset into text.
 * @return Character offset suitable for GtkTextBuffer APIs.
 */
static gint char_offset_for_byte_offset(const char *text, gsize byte_offset) {
    if (!text) return 0;
    gsize len = strlen(text);
    if (byte_offset > len) byte_offset = len;
    return (gint)g_utf8_strlen(text, (gssize)byte_offset);
}

/**
 * @brief Move a byte offset to the start of its physical line.
 * @details Formatting a discovered brace block should include the declaration
 *          or control line that owns the opening brace, otherwise `func() {`
 *          outside the selected bytes would remain unformatted.
 * @param text Full buffer text.
 * @param byte_offset Offset inside text.
 * @return Byte offset at the start of the containing line.
 */
static gsize byte_line_start(const char *text, gsize byte_offset) {
    if (!text) return 0u;
    gsize len = strlen(text);
    if (byte_offset > len) byte_offset = len;
    while (byte_offset > 0u && text[byte_offset - 1u] != '\n') byte_offset--;
    return byte_offset;
}

/**
 * @brief Resolve the target range for code formatting.
 * @details Selection wins exactly. Without a selection, the formatter receives
 *          only the surrounding brace block so Ctrl+J never reformats a whole
 *          file by accident.
 * @param tab The editor tab being formatted.
 * @param start_out Output start iterator.
 * @param end_out Output end iterator.
 * @return TRUE when a range was resolved.
 */
static gboolean formatter_target_range(EditorTab *tab,
                                       GtkTextIter *start_out,
                                       GtkTextIter *end_out) {
    if (!tab || !tab->buffer || !start_out || !end_out) return FALSE;
    if (gtk_text_buffer_get_selection_bounds(tab->buffer, start_out, end_out)) {
        return TRUE;
    }
    if (!tab->active_syntax || !tab->active_syntax->formatting ||
        g_strcmp0(tab->active_syntax->formatting->scope, "selection_or_block") != 0) {
        return FALSE;
    }

    GtkTextIter full_start;
    GtkTextIter full_end;
    GtkTextIter cursor;
    GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_bounds(tab->buffer, &full_start, &full_end);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, insert);
    char *text = gtk_text_buffer_get_text(tab->buffer, &full_start, &full_end, FALSE);
    if (!text) return FALSE;

    gint cursor_chars = gtk_text_iter_get_offset(&cursor);
    const char *cursor_ptr = g_utf8_offset_to_pointer(text, cursor_chars);
    gsize cursor_byte = cursor_ptr ? (gsize)(cursor_ptr - text) : 0u;
    gsize start_byte = 0u;
    gsize end_byte = 0u;
    gboolean found = graptos_format_find_surrounding_block(tab->active_syntax,
                                                           text,
                                                           cursor_byte,
                                                           &start_byte,
                                                           &end_byte);
    if (found) {
        start_byte = byte_line_start(text, start_byte);
        gtk_text_buffer_get_iter_at_offset(tab->buffer,
                                           start_out,
                                           char_offset_for_byte_offset(text, start_byte));
        gtk_text_buffer_get_iter_at_offset(tab->buffer,
                                           end_out,
                                           char_offset_for_byte_offset(text, end_byte));
    }
    g_free(text);
    return found;
}

/**
 * @brief Format selected code or the surrounding syntax block.
 * @details The editor owns range selection and undo grouping; the formatter only
 *          receives plain text plus syntax and indentation preferences.
 * @param tab The editor tab whose buffer should be formatted.
 */
void editor_tab_format_code(EditorTab *tab) {
    if (!tab || tab->locked || !tab->buffer || !tab->active_syntax ||
        !tab->active_syntax->formatting || !tab->active_syntax->formatting->enabled) {
        return;
    }

    GtkTextIter start;
    GtkTextIter end;
    if (!formatter_target_range(tab, &start, &end)) {
        if (tab && tab->win) app_window_set_status(tab->win, "No surrounding block to format");
        return;
    }

    gint original_start = gtk_text_iter_get_offset(&start);
    gint original_cursor = 0;
    GtkTextIter cursor;
    GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, insert);
    original_cursor = gtk_text_iter_get_offset(&cursor);

    char *text = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
    if (!text) return;
    GraptosFormatterPreferences preferences = {
        .tab_width = tab->tab_width,
        .insert_spaces = tab->insert_spaces,
    };
    g_autoptr(GError) error = NULL;
    char *formatted = graptos_format_text(tab->active_syntax, &preferences, text, &error);
    if (!formatted) {
        if (tab->win && error) app_window_set_status(tab->win, error->message);
        g_free(text);
        return;
    }
    if (strcmp(text, formatted) != 0) {
        gtk_text_buffer_begin_user_action(tab->buffer);
        gtk_text_buffer_delete(tab->buffer, &start, &end);
        gtk_text_buffer_insert(tab->buffer, &start, formatted, -1);
        gtk_text_buffer_end_user_action(tab->buffer);

        gint delta = original_cursor - original_start;
        if (delta < 0) delta = 0;
        gint formatted_chars = (gint)g_utf8_strlen(formatted, -1);
        if (delta > formatted_chars) delta = formatted_chars;
        GtkTextIter restored;
        gtk_text_buffer_get_iter_at_offset(tab->buffer,
                                           &restored,
                                           original_start + delta);
        gtk_text_buffer_place_cursor(tab->buffer, &restored);
    }
    if (tab->win) app_window_set_status(tab->win, "Formatted code");
    g_free(formatted);
    g_free(text);
}


/**
 * @brief Editor tab justify paragraph.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_justify_paragraph(EditorTab *tab) {
    if (!tab || tab->locked) return;
    GtkTextIter insert;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &insert, mark);

    GtkTextIter start = insert;
    GtkTextIter end = insert;
    while (gtk_text_iter_backward_line(&start)) {
        GtkTextIter line_end = start;
        gtk_text_iter_forward_to_line_end(&line_end);
        char *line = gtk_text_buffer_get_text(tab->buffer, &start, &line_end, FALSE);
        gboolean blank = line == NULL || g_strstrip(line)[0] == '\0';
        g_free(line);
        if (blank) {
            gtk_text_iter_forward_line(&start);
            break;
        }
    }
    while (!gtk_text_iter_is_end(&end)) {
        GtkTextIter line_start = end;
        GtkTextIter line_end = end;
        gtk_text_iter_forward_to_line_end(&line_end);
        char *line = gtk_text_buffer_get_text(tab->buffer, &line_start, &line_end, FALSE);
        gboolean blank = line == NULL || g_strstrip(line)[0] == '\0';
        g_free(line);
        if (blank) break;
        if (!gtk_text_iter_forward_line(&end)) break;
    }

    char *text = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
    if (!text) return;
    char **words = g_strsplit_set(text, " \t\r\n", -1);
    GString *out = g_string_new(NULL);
    int col = 0;
    for (guint i = 0; words && words[i]; i++) {
        if (words[i][0] == '\0') continue;
        int wlen = (int)g_utf8_strlen(words[i], -1);
        if (col > 0 && col + 1 + wlen > 78) {
            g_string_append_c(out, '\n');
            col = 0;
        } else if (col > 0) {
            g_string_append_c(out, ' ');
            col++;
        }
        g_string_append(out, words[i]);
        col += wlen;
    }
    g_string_append_c(out, '\n');
    gtk_text_buffer_begin_user_action(tab->buffer);
    gtk_text_buffer_delete(tab->buffer, &start, &end);
    gtk_text_buffer_insert(tab->buffer, &start, out->str, -1);
    gtk_text_buffer_end_user_action(tab->buffer);
    g_string_free(out, TRUE);
    g_strfreev(words);
    g_free(text);
}

