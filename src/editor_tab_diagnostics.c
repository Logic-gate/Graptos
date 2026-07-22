/**
 * @file src/editor_tab_diagnostics.c
 * @brief Graptoς editor tab diagnostics module.
 * @details Diagnostics belong to the visible buffer. YAML checks and LSP warnings arrive
 *          through different paths, but the editor should render them the same way and
 *          clear them with the same rules.
 */

#include "editor_tab_private.h"

/**
 * @brief Editor tab diagnostics type definition.
 */
typedef struct {
    gunichar open_char; /**< Open char. */
    gunichar close_char; /**< Close char. */
    gint offset; /**< Offset. */
} DiagnosticFrame;
/**
 * @brief Diagnostic free.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param data The callback context passed by the caller.
 */

void editor_diagnostic_free(gpointer data) {
    EditorDiagnostic *diagnostic = data;
    if (!diagnostic) return;
    g_free(diagnostic->message);
    g_free(diagnostic);
}

/**
 * @brief Ptr array has entries.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param array The array supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean ptr_array_has_entries(GPtrArray *array) {
    return array && array->len > 0u;
}

/**
 * @brief Str ends with.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @param suffix The suffix supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean str_ends_with(const char *text, const char *suffix) {
    if (!text || !suffix) return FALSE;
    gsize text_len = strlen(text);
    gsize suffix_len = strlen(suffix);
    return suffix_len <= text_len && strcmp(text + text_len - suffix_len, suffix) == 0;
}

/**
 * @brief Str starts with.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param text The text fragment supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean str_starts_with(const char *text, const char *prefix) {
    return text && prefix && g_str_has_prefix(text, prefix);
}

/**
 * @brief String list matches prefix.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param items The items supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean string_list_matches_prefix(GPtrArray *items, const char *text) {
    if (!items || !text) return FALSE;
    for (guint i = 0u; i < items->len; i++) {
        const char *item = g_ptr_array_index(items, i);
        if (item && item[0] != '\0' && str_starts_with(text, item)) return TRUE;
    }
    return FALSE;
}

/**
 * @brief String list matches suffix.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param items The items supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean string_list_matches_suffix(GPtrArray *items, const char *text) {
    if (!items || !text) return FALSE;
    for (guint i = 0u; i < items->len; i++) {
        const char *item = g_ptr_array_index(items, i);
        if (item && item[0] != '\0' && str_ends_with(text, item)) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Strip inline line comment.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param comment The comment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *strip_inline_line_comment(const char *line, const char *comment) {
    if (!line) return g_strdup("");
    if (!comment || comment[0] == '\0') return g_strdup(line);

    const char *hit = strstr(line, comment);
    if (!hit) return g_strdup(line);
    return g_strndup(line, (gsize)(hit - line));
}

/**
 * @brief Line needs statement ender.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param syntax The syntax definition used by the editor path.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean line_needs_statement_ender(SyntaxDef *syntax, const char *line) {
    if (!syntax || !ptr_array_has_entries(syntax->statement_required_enders) || !line) return FALSE;

    char *without_comment = strip_inline_line_comment(line, syntax->line_comment);
    char *trimmed = without_comment ? g_strstrip(without_comment) : NULL;
    if (!trimmed || trimmed[0] == '\0') {
        g_free(without_comment);
        return FALSE;
    }

    if (string_list_matches_prefix(syntax->statement_exempt_prefixes, trimmed) ||
        string_list_matches_suffix(syntax->statement_exempt_suffixes, trimmed) ||
        string_list_matches_suffix(syntax->statement_required_enders, trimmed)) {
        g_free(without_comment);
        return FALSE;
    }

    g_free(without_comment);
    return TRUE;
}

/**
 * @brief Line missing required closer.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param syntax The syntax definition used by the editor path.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean line_missing_required_closer(SyntaxDef *syntax, const char *line) {
    if (!syntax || !ptr_array_has_entries(syntax->line_close_pairs) || !line) return FALSE;

    for (guint i = 0u; i < syntax->line_close_pairs->len; i++) {
        SyntaxPair *pair = g_ptr_array_index(syntax->line_close_pairs, i);
        if (!pair || !pair->open || !pair->close) continue;
        const char *open = strstr(line, pair->open);
        if (!open) continue;
        const char *after_open = open + strlen(pair->open);
        if (!strstr(after_open, pair->close)) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Apply diagnostic line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param line_no The line no supplied by the caller.
 * @param count The count supplied by the caller.
 */
static void apply_diagnostic_line(EditorTab *tab, gint line_no, guint *count) {
    if (!tab || !tab->buffer || line_no < 0) return;
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_iter_at_line(tab->buffer, &start, line_no);
    end = start;
    if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
    gtk_text_buffer_apply_tag_by_name(tab->buffer, "graptos-diagnostic-warning", &start, &end);
    if (count) (*count)++;
}

/**
 * @brief Apply diagnostic offset.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param offset The offset supplied by the caller.
 * @param count The count supplied by the caller.
 */
static void apply_diagnostic_offset(EditorTab *tab, gint offset, guint *count) {
    if (!tab || !tab->buffer || offset < 0) return;
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_offset(tab->buffer, &iter, offset);
    apply_diagnostic_line(tab, gtk_text_iter_get_line(&iter), count);
}

/**
 * @brief Syntax pair is single char.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param pair The pair supplied by the caller.
 * @param open_char The open char supplied by the caller.
 * @param close_char The close char supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean syntax_pair_is_single_char(SyntaxPair *pair,
                                           gunichar *open_char,
                                           gunichar *close_char) {
    if (!pair || !pair->open || !pair->close) return FALSE;
    if (!g_utf8_validate(pair->open, -1, NULL) || !g_utf8_validate(pair->close, -1, NULL)) return FALSE;

    const char *open_next = g_utf8_next_char(pair->open);
    const char *close_next = g_utf8_next_char(pair->close);
    if (open_next[0] != '\0' || close_next[0] != '\0') return FALSE;

    if (open_char) *open_char = g_utf8_get_char(pair->open);
    if (close_char) *close_char = g_utf8_get_char(pair->close);
    return TRUE;
}

/**
 * @brief Pair index for open.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param syntax The syntax definition used by the editor path.
 * @param ch The ch supplied by the caller.
 * @param close_char The close char supplied by the caller.
 * @return The computed value requested by the caller.
 */
static gint pair_index_for_open(SyntaxDef *syntax, gunichar ch, gunichar *close_char) {
    if (!syntax || !syntax->close_pairs) return -1;
    for (guint i = 0u; i < syntax->close_pairs->len; i++) {
        SyntaxPair *pair = g_ptr_array_index(syntax->close_pairs, i);
        gunichar open_ch = 0;
        gunichar close_ch = 0;
        if (!syntax_pair_is_single_char(pair, &open_ch, &close_ch)) continue;
        if (open_ch == ch) {
            if (close_char) *close_char = close_ch;
            return (gint)i;
        }
    }
    return -1;
}

/**
 * @brief Is any close char.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param syntax The syntax definition used by the editor path.
 * @param ch The ch supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean is_any_close_char(SyntaxDef *syntax, gunichar ch) {
    if (!syntax || !syntax->close_pairs) return FALSE;
    for (guint i = 0u; i < syntax->close_pairs->len; i++) {
        SyntaxPair *pair = g_ptr_array_index(syntax->close_pairs, i);
        gunichar open_ch = 0;
        gunichar close_ch = 0;
        if (!syntax_pair_is_single_char(pair, &open_ch, &close_ch)) continue;
        if (close_ch == ch) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Frame array push.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param stack The stack supplied by the caller.
 * @param open_char The open char supplied by the caller.
 * @param close_char The close char supplied by the caller.
 * @param offset The offset supplied by the caller.
 */
static void frame_array_push(GArray *stack, gunichar open_char,
                             gunichar close_char, gint offset) {
    DiagnosticFrame frame;
    frame.open_char = open_char;
    frame.close_char = close_char;
    frame.offset = offset;
    g_array_append_val(stack, frame);
}

/**
 * @brief Frame array pop if matches.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param stack The stack supplied by the caller.
 * @param close_char The close char supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean frame_array_pop_if_matches(GArray *stack, gunichar close_char) {
    if (!stack || stack->len == 0u) return FALSE;
    DiagnosticFrame frame = g_array_index(stack, DiagnosticFrame, stack->len - 1u);
    if (frame.close_char != close_char) return FALSE;
    g_array_remove_index(stack, stack->len - 1u);
    return TRUE;
}

/**
 * @brief Apply unbalanced pair diagnostics.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param text The text fragment supplied by the caller.
 * @param count The count supplied by the caller.
 */
static void apply_unbalanced_pair_diagnostics(EditorTab *tab,
                                              const char *text,
                                              guint *count) {
    SyntaxDef *syntax = tab ? tab->active_syntax : NULL;
    if (!syntax || !ptr_array_has_entries(syntax->close_pairs) || !text) return;

    GArray *stack = g_array_new(FALSE, FALSE, sizeof(DiagnosticFrame));
    if (!stack) return;

    gboolean in_line_comment = FALSE;
    gboolean in_string = FALSE;
    gunichar string_quote = 0;
    gint offset = 0;
    for (const char *p = text; p && *p; p = g_utf8_next_char(p), offset++) {
        gunichar ch = g_utf8_get_char(p);
        gunichar next = 0;
        const char *next_p = g_utf8_next_char(p);
        if (next_p && *next_p) next = g_utf8_get_char(next_p);

        if (in_line_comment) {
            if (ch == '\n') in_line_comment = FALSE;
            continue;
        }

        if (!in_string && syntax->line_comment && syntax->line_comment[0] != '\0' &&
            strncmp(p, syntax->line_comment, strlen(syntax->line_comment)) == 0) {
            in_line_comment = TRUE;
            continue;
        }

        if (in_string) {
            if (ch == '\\' && next != 0) {
                p = next_p;
                offset++;
                continue;
            }
            if (ch == string_quote) {
                in_string = FALSE;
                string_quote = 0;
            }
            continue;
        }

        if (ch == '"' || ch == '\'') {
            in_string = TRUE;
            string_quote = ch;
            continue;
        }

        gunichar expected_close = 0;
        if (pair_index_for_open(syntax, ch, &expected_close) >= 0) {
            frame_array_push(stack, ch, expected_close, offset);
        } else if (is_any_close_char(syntax, ch)) {
            if (!frame_array_pop_if_matches(stack, ch)) apply_diagnostic_offset(tab, offset, count);
        }
    }

    for (guint i = 0u; i < stack->len; i++) {
        DiagnosticFrame frame = g_array_index(stack, DiagnosticFrame, i);
        apply_diagnostic_offset(tab, frame.offset, count);
    }

    g_array_free(stack, TRUE);
}

/**
 * @brief Apply line diagnostics.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param count The count supplied by the caller.
 */
static void apply_line_diagnostics(EditorTab *tab, guint *count) {
/**
 * @brief Tab apply external diagnostic.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param start_line The start line supplied by the caller.
 * @param start_character The start character supplied by the caller.
 * @param end_line The end line supplied by the caller.
 * @param end_character The end character supplied by the caller.
 * @param message The message supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
    if (!tab || !tab->buffer || !tab->active_syntax) return;
    SyntaxDef *syntax = tab->active_syntax;
    gint line_count = gtk_text_buffer_get_line_count(tab->buffer);

    for (gint line_no = 0; line_no < line_count; line_no++) {
        GtkTextIter start;
        GtkTextIter end;
        gtk_text_buffer_get_iter_at_line(tab->buffer, &start, line_no);
        end = start;
        if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
        char *line = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
        if (!line) continue;

        if (line_missing_required_closer(syntax, line) ||
            line_needs_statement_ender(syntax, line)) {
            apply_diagnostic_line(tab, line_no, count);
        }
        g_free(line);
    }
}

/**
 * @brief Ensure diagnostic tag.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void ensure_diagnostic_tag(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    const char *bg = tab->win && tab->win->diagnostic_warning_bg_color ?
        tab->win->diagnostic_warning_bg_color : "#5f4b24";
    const char *fg = tab->win && tab->win->diagnostic_warning_fg_color ?
        tab->win->diagnostic_warning_fg_color : "#ffd166";
    GdkRGBA underline = { 1.0f, 0.70f, 0.15f, 1.0f };
    if (fg && fg[0] == '#') (void)gdk_rgba_parse(&underline, fg);
    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(tab->buffer);
    GtkTextTag *tag = gtk_text_tag_table_lookup(table, "graptos-diagnostic-warning");
    if (tag) {
        g_object_set(tag,
                     "foreground", fg,
                     "underline", PANGO_UNDERLINE_ERROR,
                     "underline-rgba", &underline,
                     "background", bg,
                     NULL);
        return;
    }
    gtk_text_buffer_create_tag(tab->buffer, "graptos-diagnostic-warning",
                               "foreground", fg,
                               "underline", PANGO_UNDERLINE_ERROR,
                               "underline-rgba", &underline,
                               "background", bg,
                               NULL);
}

/**
 * @brief Clear syntax diagnostics.
 * @details Diagnostic tags are short-lived editor state. Clearing them in one
 *          pass keeps old syntax and LSP messages from surviving after the
 *          buffer, language, or active file has moved on.
 * @param tab The editor tab whose diagnostic tags should be removed.
 */
void clear_syntax_diagnostics(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    if (tab->diagnostics_active) {
        GtkTextTagTable *table = gtk_text_buffer_get_tag_table(tab->buffer);
        if (gtk_text_tag_table_lookup(table, "graptos-diagnostic-warning")) {
            GtkTextIter start;
            GtkTextIter end;
            gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
            gtk_text_buffer_remove_tag_by_name(tab->buffer, "graptos-diagnostic-warning", &start, &end);
        }
    }
    if (tab->diagnostics) g_ptr_array_set_size(tab->diagnostics, 0u);
    tab->diagnostics_active = FALSE;
    tab->diagnostic_warnings = 0u;
}

/**
 * @brief Apply a diagnostic range supplied by an external source.
 * @details LSP diagnostics use line and character positions while the editor
 *          stores tags on GtkTextIters. The conversion is clamped to the live
 *          buffer so stale server positions cannot crash or mark unrelated
 *          memory.
 * @param tab The editor tab receiving the diagnostic.
 * @param start_line The starting diagnostic line.
 * @param start_character The starting character within the line.
 * @param end_line The ending diagnostic line.
 * @param end_character The ending character within the line.
 * @param message The message shown when the diagnostic is hovered.
 * @return TRUE when a diagnostic was applied; otherwise FALSE.
 */
gboolean editor_tab_apply_external_diagnostic(EditorTab *tab,
                                              gint start_line,
                                              gint start_character,
                                              gint end_line,
                                              gint end_character,
                                              const char *message) {
    if (!tab || !tab->buffer || start_line < 0 || start_character < 0) return FALSE;
    ensure_diagnostic_tag(tab);

    GtkTextIter start;
    GtkTextIter end;
    gint line_count = gtk_text_buffer_get_line_count(tab->buffer);
    if (start_line >= line_count) start_line = line_count > 0 ? line_count - 1 : 0;
    if (end_line < start_line) end_line = start_line;
    if (end_line >= line_count) end_line = line_count > 0 ? line_count - 1 : 0;

    gtk_text_buffer_get_iter_at_line(tab->buffer, &start, start_line);
    for (gint i = 0; i < start_character &&
         !gtk_text_iter_ends_line(&start) &&
         !gtk_text_iter_is_end(&start); i++) {
        if (!gtk_text_iter_forward_char(&start)) break;
    }
    gtk_text_buffer_get_iter_at_line(tab->buffer, &end, end_line);
    for (gint i = 0; i < end_character &&
         !gtk_text_iter_ends_line(&end) &&
         !gtk_text_iter_is_end(&end); i++) {
        if (!gtk_text_iter_forward_char(&end)) break;
    }
    if (gtk_text_iter_compare(&end, &start) <= 0) {
        end = start;
        if (!gtk_text_iter_forward_char(&end)) {
            if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
        }
    }

    gtk_text_buffer_apply_tag_by_name(tab->buffer,
                                      "graptos-diagnostic-warning",
                                      &start,
                                      &end);

    if (tab->diagnostics && message && message[0] != '\0') {
        EditorDiagnostic *diagnostic = g_new0(EditorDiagnostic, 1);
        if (diagnostic) {
            diagnostic->start_offset = gtk_text_iter_get_offset(&start);
            diagnostic->end_offset = gtk_text_iter_get_offset(&end);
            diagnostic->message = g_strdup(message);
            g_ptr_array_add(tab->diagnostics, diagnostic);
        }
    }

    tab->diagnostic_warnings++;
    tab->diagnostics_active = TRUE;
    return TRUE;
}

/**
 * @brief Find the diagnostic covering an iterator.
 * @details Hover code asks this on every pointer move. The lookup stays as a
 *          simple offset scan because diagnostics are already bounded and it
 *          avoids maintaining a second interval structure for small lists.
 * @param tab The editor tab that owns the diagnostics.
 * @param iter The iterator under the pointer or cursor.
 * @return The matching diagnostic, or NULL when the position is clean.
 */
EditorDiagnostic *editor_tab_diagnostic_at_iter(EditorTab *tab, GtkTextIter *iter) {
    if (!tab || !tab->diagnostics || !iter) return NULL;
    gint offset = gtk_text_iter_get_offset(iter);
    for (guint i = 0u; i < tab->diagnostics->len; i++) {
        EditorDiagnostic *diagnostic = g_ptr_array_index(tab->diagnostics, i);
        if (!diagnostic) continue;
        if (offset >= diagnostic->start_offset && offset < diagnostic->end_offset) {
            return diagnostic;
        }
    }
    return NULL;
}

/**
 * @brief Editor tab apply syntax diagnostics.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_apply_syntax_diagnostics(EditorTab *tab) {
    if (!tab || !tab->buffer) return;
    if (tab->win && !tab->win->diagnostics_enabled) {
        clear_syntax_diagnostics(tab);
        editor_tab_schedule_lightweight_ui_refresh(tab);
        return;
    }
    if (tab->active_syntax &&
        tab->active_syntax->lsp_command &&
        tab->active_syntax->lsp_command[0] != '\0') {
        editor_tab_schedule_lightweight_ui_refresh(tab);
        return;
    }
    if (!editor_tab_live_features_allowed(tab)) {
        tab->diagnostic_warnings = 0u;
        tab->diagnostics_active = FALSE;
        editor_tab_schedule_lightweight_ui_refresh(tab);
        return;
    }

    clear_syntax_diagnostics(tab);

    if (!tab->active_syntax ||
        (guint)gtk_text_buffer_get_char_count(tab->buffer) > GRAPTOS_DIAGNOSTICS_MAX_CHARS) {
        editor_tab_schedule_lightweight_ui_refresh(tab);
        return;
    }

    ensure_diagnostic_tag(tab);

    guint warnings = 0u;
    apply_line_diagnostics(tab, &warnings);

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
    if (text) {
        apply_unbalanced_pair_diagnostics(tab, text, &warnings);
        g_free(text);
    }

    tab->diagnostic_warnings = warnings;
    tab->diagnostics_active = warnings > 0u;
    editor_tab_schedule_lightweight_ui_refresh(tab);
}

/**
 * @brief Diagnostics timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean diagnostics_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->diagnostics_timeout = 0u;
    editor_tab_apply_syntax_diagnostics(tab);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Editor tab schedule syntax diagnostics.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_syntax_diagnostics(EditorTab *tab) {
    if (!tab) return;
    graptos_source_cancel(&tab->diagnostics_timeout);
    if (tab->win && !tab->win->diagnostics_enabled) {
        clear_syntax_diagnostics(tab);
        editor_tab_schedule_lightweight_ui_refresh(tab);
        tab->diagnostics_timeout = 0u;
        return;
    }
    if (!editor_tab_live_features_allowed(tab)) {
        tab->diagnostics_timeout = 0u;
        return;
    }
    tab->diagnostics_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                                 GRAPTOS_DIAGNOSTICS_DELAY_MS,
                                                 diagnostics_timeout_cb,
                                                 tab,
                                                 NULL);
}
