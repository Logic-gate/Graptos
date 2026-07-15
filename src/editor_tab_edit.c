/**
 * @file src/editor_tab_edit.c
 * @brief Cleaf editor tab edit module.
 */

#include "editor_tab_private.h"

/**
 * @brief Editor tab cut clipboard.
 */
void editor_tab_cut_clipboard(EditorTab *tab) {
    if (!tab || tab->locked) return;
    GdkClipboard *clipboard = gtk_widget_get_clipboard(tab->text_view);
    gtk_text_buffer_cut_clipboard(tab->buffer, clipboard, TRUE);
}


/**
 * @brief Editor tab copy clipboard.
 */
void editor_tab_copy_clipboard(EditorTab *tab) {
    if (!tab) return;
    GdkClipboard *clipboard = gtk_widget_get_clipboard(tab->text_view);
    gtk_text_buffer_copy_clipboard(tab->buffer, clipboard);
}


/**
 * @brief Editor tab paste clipboard.
 */
void editor_tab_paste_clipboard(EditorTab *tab) {
    if (!tab || tab->locked) return;
    GdkClipboard *clipboard = gtk_widget_get_clipboard(tab->text_view);
    gtk_text_buffer_paste_clipboard(tab->buffer, clipboard, NULL, TRUE);
}


/**
 * @brief Editor tab select all.
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
 */
gboolean select_match(EditorTab *tab, GtkTextIter *start, GtkTextIter *end) {
    if (!tab || !start || !end) return FALSE;
    gtk_text_buffer_select_range(tab->buffer, start, end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(tab->text_view), start, 0.18, FALSE, 0.0, 0.0);
    return TRUE;
}


/**
 * @brief Editor tab find.
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
 * @brief Editor tab justify paragraph.
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


