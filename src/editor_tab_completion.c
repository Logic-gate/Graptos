/**
 * @file src/editor_tab_completion.c
 * @brief Graptoς editor tab completion module.
 * @details Completion has to feel instant even when LSP is slow or missing. We keep the
 *          local fallback and UI merge logic close to the tab, then let smarter providers
 *          feed into it when they have useful answers.
 */

#include "editor_tab_private.h"

/**
 * @brief Completion add row with detail.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 * @param detail The detail supplied by the caller.
 */
static void completion_add_row_with_detail(EditorTab *tab, const char *word, const char *detail);
/**
 * @brief Completion add source header.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param label_text The label text supplied by the caller.
 */
static void completion_add_source_header(EditorTab *tab, const char *label_text);

/**
 * @brief Completion result source.
 */
typedef enum {
    COMPLETION_SOURCE_GRAPTOS, /**< Graptoς YAML/index/import source. */
    COMPLETION_SOURCE_LSP /**< Language Server Protocol source. */
} CompletionSource;

/**
 * @brief Completion candidate with source metadata.
 */
typedef struct {
    char *word; /**< Insertable completion text. */
    CompletionSource source; /**< Candidate source. */
} CompletionCandidate;

/**
 * @brief Free a completion candidate.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param data The callback context passed by the caller.
 */
static void completion_candidate_free(gpointer data) {
    CompletionCandidate *candidate = data;
    if (!candidate) return;
    g_free(candidate->word);
    g_free(candidate);
}

/**
 * @brief Member access completion context.
 */
typedef struct {
    gboolean active; /**< Whether member completion is active. */
    char *base; /**< Visible base before the dot. */
    char *prefix; /**< Member prefix after the dot. */
} MemberCompletionContext;

/**
 * @brief Completion contains.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param items The items supplied by the caller.
 * @param word The symbol text being matched.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean completion_contains(GPtrArray *items, const char *word) {
    if (!items || !word) return FALSE;
    for (guint i = 0u; i < items->len; i++) {
        CompletionCandidate *existing = g_ptr_array_index(items, i);
        if (existing && existing->word && strcmp(existing->word, word) == 0) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Add a sourced completion candidate.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param items The items supplied by the caller.
 * @param word The symbol text being matched.
 * @param source The source supplied by the caller.
 */
static void completion_add_sourced(GPtrArray *items,
                                   const char *word,
                                   CompletionSource source) {
    if (!items || !word || word[0] == '\0') return;
    if (completion_contains(items, word)) return;
    CompletionCandidate *candidate = g_new0(CompletionCandidate, 1);
    if (!candidate) return;
    candidate->word = g_strdup(word);
    candidate->source = source;
    g_ptr_array_add(items, candidate);
}

/**
 * @brief Completion copy candidates.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param dst The dst supplied by the caller.
 * @param src The src supplied by the caller.
 * @param source The source supplied by the caller.
 */
static void completion_copy_candidates(GPtrArray *dst,
                                       GPtrArray *src,
                                       CompletionSource source) {
    if (!dst || !src) return;
    for (guint i = 0u; i < src->len; i++) {
        const char *candidate = g_ptr_array_index(src, i);
        completion_add_sourced(dst, candidate, source);
    }
}

/**
 * @brief Completion merge imports.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param dst The dst supplied by the caller.
 * @param imports The imports supplied by the caller.
 * @param source The source supplied by the caller.
 */
static void completion_merge_imports(GPtrArray *dst,
                                     GPtrArray *imports,
                                     CompletionSource source) {
    if (!dst || !imports) return;
    for (guint i = 0u; i < imports->len; i++) {
        const char *candidate = g_ptr_array_index(imports, i);
        completion_add_sourced(dst, candidate, source);
    }
}

/**
 * @brief Clear a member completion context.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param ctx The ctx supplied by the caller.
 */
static void member_completion_context_clear(MemberCompletionContext *ctx) {
    if (!ctx) return;
    g_clear_pointer(&ctx->base, g_free);
    g_clear_pointer(&ctx->prefix, g_free);
    ctx->active = FALSE;
}

/**
 * @brief Read the identifier immediately before an iterator.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param iter The text iterator that anchors the lookup.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *identifier_before_iter(GtkTextIter *iter) {
    if (!iter) return NULL;
    GtkTextIter start = *iter;
    while (!gtk_text_iter_starts_line(&start)) {
        GtkTextIter prev = start;
        if (!gtk_text_iter_backward_char(&prev)) break;
        gunichar ch = gtk_text_iter_get_char(&prev);
        if (!completion_is_word_char(ch)) break;
        start = prev;
    }
    if (gtk_text_iter_equal(&start, iter)) return NULL;
    return gtk_text_buffer_get_text(gtk_text_iter_get_buffer(iter),
                                    &start,
                                    iter,
                                    FALSE);
}

/**
 * @brief Detect member access completion before the cursor.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param buffer The text buffer used for the operation.
 * @param prefix_start The prefix start supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @param ctx The ctx supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean member_completion_context_at_cursor(GtkTextBuffer *buffer,
                                                    GtkTextIter *prefix_start,
                                                    const char *prefix,
                                                    MemberCompletionContext *ctx) {
    if (!buffer || !prefix_start || !ctx) return FALSE;

    GtkTextIter dot = *prefix_start;
    if (!gtk_text_iter_backward_char(&dot)) return FALSE;
    if (gtk_text_iter_get_char(&dot) != (gunichar)'.') return FALSE;

    GtkTextIter base_end = dot;
    char *base = identifier_before_iter(&base_end);
    if (!base || base[0] == '\0') {
        g_free(base);
        return FALSE;
    }

    ctx->active = TRUE;
    ctx->base = base;
    ctx->prefix = g_strdup(prefix ? prefix : "");
    return TRUE;
}

/**
 * @brief Completion merge indexed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param dst The dst supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 */
static void completion_merge_indexed(EditorTab *tab, GPtrArray *dst,
                                     const char *prefix) {
    if (!tab || !dst || !prefix) return;
    GPtrArray *indexed = index_candidates_for_tab(
        tab, prefix, GRAPTOS_COMPLETION_DEFAULT_MAX_RESULTS);
    if (!indexed) return;

    for (guint i = 0u; i < indexed->len; i++) {
            completion_add_sourced(dst,
                                   g_ptr_array_index(indexed, i),
                                   COMPLETION_SOURCE_GRAPTOS);
    }
    g_ptr_array_free(indexed, TRUE);
}

/**
 * @brief Completion build candidates.
 * @details LSP gets first chance because it has real language knowledge. Local
 *          Graptoς candidates stay in the same list as a fallback, and source
 *          headers are preserved so the user can see which engine answered.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param prefix The prefix supplied by the caller.
 * @param import_context The import context supplied by the caller.
 * @param member_context The member context supplied by the caller.
 * @param imports The imports supplied by the caller.
 * @param manual The manual supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GPtrArray *completion_build_candidates(EditorTab *tab,
                                              const char *prefix,
                                              gboolean import_context,
                                              gboolean member_context,
                                              GPtrArray *imports,
                                              gboolean manual) {
    GPtrArray *lsp_items = NULL;
    if (tab->win && tab->win->lsp_client && tab->file_path) {
        lsp_items = lsp_client_completion_candidates(
            tab->win->lsp_client,
            tab,
            prefix,
            member_context,
            GRAPTOS_COMPLETION_DEFAULT_MAX_RESULTS);
    }
    guint lsp_item_count = lsp_items ? lsp_items->len : 0u;

    if (import_context || member_context) {
        GPtrArray *items = g_ptr_array_new_with_free_func(completion_candidate_free);
        completion_copy_candidates(items, lsp_items, COMPLETION_SOURCE_LSP);
        completion_copy_candidates(items, imports, COMPLETION_SOURCE_GRAPTOS);
        if (lsp_items) g_ptr_array_free(lsp_items, TRUE);
        if (tab->win && tab->win->debug_mode) {
            g_message("LSP completion UI: prefix=%s lsp_first_items=%u final_items=%u manual=%d import=%d member=%d",
                      prefix ? prefix : "",
                      lsp_item_count,
                      items ? items->len : 0u,
                      manual ? 1 : 0,
                      import_context ? 1 : 0,
                      member_context ? 1 : 0);
        }
        return items;
    }

    GPtrArray *items = g_ptr_array_new_with_free_func(completion_candidate_free);
    if (!items) return NULL;

    completion_copy_candidates(items, lsp_items, COMPLETION_SOURCE_LSP);
    if (lsp_items) g_ptr_array_free(lsp_items, TRUE);

    if (items->len == 0u) {
        GPtrArray *yaml_items = completion_candidates_new(
            tab->buffer, tab->active_syntax, prefix,
            GRAPTOS_COMPLETION_DEFAULT_MAX_RESULTS);
        completion_merge_imports(items, yaml_items, COMPLETION_SOURCE_GRAPTOS);
        if (yaml_items) g_ptr_array_free(yaml_items, TRUE);
    }
    completion_merge_imports(items, imports, COMPLETION_SOURCE_GRAPTOS);
    if (manual) completion_merge_indexed(tab, items, prefix);
    if (tab->win && tab->win->debug_mode) {
        g_message("LSP completion UI: prefix=%s lsp_first_items=%u final_items=%u manual=%d import=%d member=%d",
                  prefix ? prefix : "",
                  lsp_item_count,
                  items ? items->len : 0u,
                  manual ? 1 : 0,
                  import_context ? 1 : 0,
                  member_context ? 1 : 0);
    }
    return items;
}

/**
 * @brief Completion line has word boundary.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param word The symbol text being matched.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean completion_line_has_word_boundary(const char *line,
                                                 const char *word) {
    if (!line || !word || word[0] == '\0') return FALSE;
    gsize len = strlen(word);
    const char *p = line;
    while ((p = strstr(p, word)) != NULL) {
        char before = p == line ? '\0' : p[-1];
        char after = p[len];
        if (!g_ascii_isalnum(before) && before != '_' &&
            !g_ascii_isalnum(after) && after != '_') {
            return TRUE;
        }
        p += len;
    }
    return FALSE;
}

/**
 * @brief Completion line looks like definition.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param trim The trim supplied by the caller.
 * @param word The symbol text being matched.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean completion_line_looks_like_definition(const char *trim,
                                                     const char *word) {
    if (!trim || !word || !completion_line_has_word_boundary(trim, word)) {
        return FALSE;
    }
    if (g_str_has_prefix(trim, "#define")) return TRUE;
    if (strstr(trim, "typedef ") || strstr(trim, "struct ") ||
        strstr(trim, "enum ") || strstr(trim, "union ")) return TRUE;
    if (strstr(trim, "def ") || strstr(trim, "class ") ||
        strstr(trim, "function ")) return TRUE;
    if (strstr(trim, "const ") || strstr(trim, "let ") ||
        strstr(trim, "var ") || strstr(trim, "static ")) return TRUE;
    if (strchr(trim, '(') && strchr(trim, ')')) return TRUE;
    if (strchr(trim, '=') && !strstr(trim, "==")) return TRUE;
    return FALSE;
}

/**
 * @brief Completion clean comment line.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *completion_clean_comment_line(const char *line) {
    if (!line) return NULL;
    char *copy = g_strdup(line);
    char *t = g_strstrip(copy);

    if (g_str_has_prefix(t, "/**")) t += 3;
    else if (g_str_has_prefix(t, "/*")) t += 2;
    else if (g_str_has_prefix(t, "///")) t += 3;
    else if (g_str_has_prefix(t, "//")) t += 2;
    else if (g_str_has_prefix(t, "#")) t += 1;
    else if (g_str_has_prefix(t, "*")) t += 1;
    else {
        g_free(copy);
        return NULL;
    }

    t = g_strstrip(t);
    gsize len = strlen(t);
    while (len > 0u && (t[len - 1u] == '/' || t[len - 1u] == '*')) {
        t[len - 1u] = '\0';
        len--;
    }
    char *out = g_strdup(g_strstrip(t));
    g_free(copy);
    return out;
}

/**
 * @brief Completion detail from current buffer.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *completion_detail_from_current_buffer(EditorTab *tab,
                                                   const char *word) {
    if (!tab || !word || word[0] == '\0') return NULL;
    char *text = buffer_text(tab);
    if (!text) return NULL;

    char **lines = g_strsplit(text, "\n", -1);
    char *detail = NULL;
    for (guint i = 0u; lines && lines[i]; i++) {
        char *line_copy = g_strdup(lines[i]);
        char *trim = g_strstrip(line_copy);
        if (completion_line_looks_like_definition(trim, word)) {
            GString *out = g_string_new(NULL);
            if (i > 0u) {
                guint start = i > 2u ? i - 2u : 0u;
                for (guint c = start; c < i; c++) {
                    char *comment = completion_clean_comment_line(lines[c]);
                    if (comment && comment[0] != '\0') {
                        if (out->len > 0u) g_string_append(out, " ");
                        g_string_append(out, comment);
                    }
                    g_free(comment);
                }
            }
            if (out->len > 0u) g_string_append(out, " — ");
            g_string_append(out, trim);
            detail = g_string_free(out, FALSE);
            g_free(line_copy);
            break;
        }
        g_free(line_copy);
    }

    g_strfreev(lines);
    g_free(text);

    if (detail && strlen(detail) > 220u) {
        detail[217] = '.';
        detail[218] = '.';
        detail[219] = '.';
        detail[220] = '\0';
    }
    return detail;
}

/**
 * @brief Completion update popup size.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param row_count The row count supplied by the caller.
 * @param has_details The has details supplied by the caller.
 */
static void completion_update_popup_size(EditorTab *tab,
                                         guint row_count,
                                         gboolean has_details) {
    if (!tab || !tab->completion_scrolled) return;
    guint visible = row_count > 10u ? 10u : row_count;
    gint row_height = has_details ? 54 : 30;
    gint height = visible > 0u ? (gint)visible * row_height + 2 : 30;
    if (height < 30) height = 30;
    gtk_widget_set_size_request(tab->completion_scrolled, 460, height);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tab->completion_scrolled),
                                   GTK_POLICY_NEVER,
                                   row_count > 10u ? GTK_POLICY_AUTOMATIC
                                                   : GTK_POLICY_NEVER);
}

/**
 * @brief Completion populate rows.
 * @details Detail rows are useful, but scanning the current buffer for every
 *          candidate gets expensive quickly. We add details only for the first
 *          few visible items so the popover stays fast while still feeling rich.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param candidates The candidates supplied by the caller.
 */
static void completion_populate_rows(EditorTab *tab, GPtrArray *candidates) {
    completion_clear_rows(tab);
    if (!candidates) {
        completion_update_popup_size(tab, 0u, FALSE);
        return;
    }
    gboolean has_details = FALSE;
    gboolean allow_details = tab && tab->buffer && editor_tab_live_features_allowed(tab);
    CompletionSource last_source = (CompletionSource)-1;
    guint data_rows = 0u;
    for (guint i = 0u; i < candidates->len; i++) {
        CompletionCandidate *candidate = g_ptr_array_index(candidates, i);
        if (!candidate || !candidate->word) continue;
        if (candidate->source != last_source) {
            completion_add_source_header(tab,
                                         candidate->source == COMPLETION_SOURCE_LSP
                                             ? "LSP"
                                             : "Graptoς");
            last_source = candidate->source;
        }
        const char *word = candidate->word;
        char *detail = allow_details && i < 10u
            ? completion_detail_from_current_buffer(tab, word)
            : NULL;
        if (detail && detail[0] != '\0') has_details = TRUE;
        completion_add_row_with_detail(tab, word, detail);
        data_rows++;
        g_free(detail);
    }
    completion_update_popup_size(tab, data_rows,
                                 has_details);
}

/**
 * @brief Completion place popover.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param cursor The cursor supplied by the caller.
 * @param manual The manual supplied by the caller.
 */
static void completion_place_popover(EditorTab *tab, GtkTextIter *cursor,
                                     gboolean manual) {
    (void)cursor;
    if (!tab || !tab->text_view || !tab->popover_parent ||
        !gtk_widget_get_mapped(tab->text_view) ||
        !gtk_widget_get_mapped(tab->popover_parent) ||
        gtk_widget_get_width(tab->text_view) <= 0 ||
        gtk_widget_get_height(tab->text_view) <= 0 ||
        gtk_widget_get_width(tab->popover_parent) <= 0 ||
        gtk_widget_get_height(tab->popover_parent) <= 0) {
        return;
    }

    if (!editor_tab_place_popover_at_cursor(tab, tab->completion_popover)) {
        return;
    }
    graptos_popover_show(tab->completion_popover);
    gtk_widget_grab_focus(tab->text_view);

    GtkListBoxRow *first = gtk_list_box_get_row_at_index(
        GTK_LIST_BOX(tab->completion_list), 0);
    while (first &&
           !g_object_get_data(G_OBJECT(first), "graptos-completion-word")) {
/**
 * @brief Add source header.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param label_text The label text supplied by the caller.
 */
        first = gtk_list_box_get_row_at_index(
            GTK_LIST_BOX(tab->completion_list),
            gtk_list_box_row_get_index(first) + 1);
    }
    if (manual && first) gtk_list_box_select_row(
        GTK_LIST_BOX(tab->completion_list), first);
}

/**
 * @brief Completion is visible.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean completion_is_visible(EditorTab *tab) {
    return tab && tab->completion_popover &&
           gtk_widget_get_visible(tab->completion_popover);
}

/**
 * @brief Editor tab hide completion.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_hide_completion(EditorTab *tab) {
    if (!tab) return;
    if (tab->completion_timeout != 0u) {
        graptos_source_cancel(&tab->completion_timeout);
    }
    if (tab->completion_popover) graptos_popover_hide(tab->completion_popover);
    tab->completion_manual = FALSE;
    g_clear_pointer(&tab->completion_prefix, g_free);
}

/**
 * @brief Completion clear rows.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void completion_clear_rows(EditorTab *tab) {
    if (!tab || !tab->completion_list) return;
    graptos_list_box_clear(tab->completion_list);
}

/**
 * @brief Completion insert word.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 */
void completion_insert_word(EditorTab *tab, const char *word) {
    if (!tab || tab->locked || !word || word[0] == '\0') return;

    GtkTextIter start;
    GtkTextIter cursor;
    char *prefix = completion_prefix_at_cursor(tab->buffer, &start, &cursor);
    if (!prefix) return;

    gint start_offset = gtk_text_iter_get_offset(&start);
    gtk_text_buffer_begin_user_action(tab->buffer);
    if (!gtk_text_iter_equal(&start, &cursor)) {
        gtk_text_buffer_delete(tab->buffer, &start, &cursor);
        gtk_text_buffer_get_iter_at_offset(tab->buffer, &start, start_offset);
    }
    gtk_text_buffer_insert(tab->buffer, &start, word, -1);
    gtk_text_buffer_end_user_action(tab->buffer);

    g_free(prefix);
    editor_tab_hide_completion(tab);
}

/**
 * @brief Completion row activated.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param box The box supplied by the caller.
 * @param row The row supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void completion_row_activated(GtkListBox *box, GtkListBoxRow *row,
                              gpointer user_data) {
    (void)box;
    EditorTab *tab = user_data;
    const char *word = row ? g_object_get_data(
        G_OBJECT(row), "graptos-completion-word") : NULL;
    completion_insert_word(tab, word);
}

/**
 * @brief Add a non-selectable completion source header.
 * @details Graptoς and LSP entries are shown together, so the list needs a
 *          visible divider that does not behave like a completion candidate.
 *          Keeping headers as inert rows avoids special cases in navigation.
 * @param tab The editor tab that owns the completion list.
 * @param label_text The heading text shown above that source group.
 */
static void completion_add_source_header(EditorTab *tab, const char *label_text) {
    if (!tab || !tab->completion_list || !label_text) return;
    GtkWidget *row = gtk_list_box_row_new();
    gtk_widget_set_sensitive(row, FALSE);
    gtk_widget_set_can_focus(row, FALSE);

    GtkWidget *label = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_start(label, 8);
    gtk_widget_set_margin_end(label, 8);
    gtk_widget_set_margin_top(label, 6);
    gtk_widget_set_margin_bottom(label, 3);
    gtk_widget_add_css_class(label, "graptos-ref-heading");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
    gtk_list_box_insert(GTK_LIST_BOX(tab->completion_list), row, -1);
}

/**
 * @brief Completion add row with detail.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 * @param detail The detail supplied by the caller.
 */
static void completion_add_row_with_detail(EditorTab *tab,
                                           const char *word,
                                           const char *detail) {
    if (!tab || !tab->completion_list || !word) return;

    GtkWidget *row = gtk_list_box_row_new();
    gtk_widget_set_can_focus(row, FALSE);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_top(box, detail && detail[0] ? 4 : 3);
    gtk_widget_set_margin_bottom(box, detail && detail[0] ? 5 : 3);

    GtkWidget *label = gtk_label_new(word);
    gtk_widget_set_can_focus(label, FALSE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(label, "graptos-completion-title");
    gtk_box_append(GTK_BOX(box), label);

    if (detail && detail[0] != '\0') {
        GtkWidget *detail_label = gtk_label_new(detail);
        gtk_widget_set_can_focus(detail_label, FALSE);
        gtk_label_set_xalign(GTK_LABEL(detail_label), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(detail_label), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(detail_label, "graptos-completion-detail");
        gtk_box_append(GTK_BOX(box), detail_label);
    }

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data_full(G_OBJECT(row), "graptos-completion-word",
                           g_strdup(word), g_free);
    gtk_list_box_insert(GTK_LIST_BOX(tab->completion_list), row, -1);
}

/**
 * @brief Completion add row.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param word The symbol text being matched.
 */
void completion_add_row(EditorTab *tab, const char *word) {
    completion_add_row_with_detail(tab, word, NULL);
}

/**
 * @brief Editor tab show completion.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param manual The manual supplied by the caller.
 */
void editor_tab_show_completion(EditorTab *tab, gboolean manual) {
    if (!tab || !tab->autocomplete_enabled) return;
    if (!manual && !editor_tab_live_features_allowed(tab)) return;

    GtkTextIter prefix_start;
    GtkTextIter cursor;
    char *prefix = completion_prefix_at_cursor(tab->buffer, &prefix_start,
                                               &cursor);
    if (!prefix) return;

    gboolean is_import = import_completion_tab_is_import_context(tab);
    MemberCompletionContext member_ctx = {0};
    gboolean is_member = member_completion_context_at_cursor(tab->buffer,
                                                             &prefix_start,
                                                             prefix,
                                                             &member_ctx);
    if (!manual && !is_import && !is_member && g_utf8_strlen(prefix, -1) < 2) {
        g_free(prefix);
        editor_tab_hide_completion(tab);
        return;
    }

    GPtrArray *imports = is_member
        ? import_completion_member_candidates_for_tab(tab,
                                                      member_ctx.base,
                                                      member_ctx.prefix,
                                                      GRAPTOS_COMPLETION_DEFAULT_MAX_RESULTS)
        : (manual || is_import)
        ? import_completion_candidates_for_tab(tab, prefix,
                                               GRAPTOS_COMPLETION_DEFAULT_MAX_RESULTS)
        : NULL;
    GPtrArray *candidates = completion_build_candidates(tab, prefix,
                                                        is_import,
                                                        is_member,
                                                        imports,
                                                        manual);
    g_clear_pointer(&imports, g_ptr_array_unref);
    member_completion_context_clear(&member_ctx);
    if (!candidates || candidates->len == 0u) {
        g_clear_pointer(&candidates, g_ptr_array_unref);
        g_free(prefix);
        editor_tab_hide_completion(tab);
        return;
    }

    completion_populate_rows(tab, candidates);
    g_ptr_array_free(candidates, TRUE);
    g_free(tab->completion_prefix);
    tab->completion_prefix = prefix;
    tab->completion_manual = manual;
    completion_place_popover(tab, &cursor, manual);
}

/**
 * @brief Completion timeout cb.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean completion_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->completion_timeout = 0u;
    editor_tab_show_completion(tab, FALSE);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Editor tab schedule completion.
 * @details Automatic completion is intentionally delayed and bounded. It should
 *          feel like it appears while typing, not like it is part of the key
 *          press path, especially on files where local scanning would be noisy.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void editor_tab_schedule_completion(EditorTab *tab) {
    if (!tab || !tab->autocomplete_enabled || !tab->buffer) return;
    graptos_source_cancel(&tab->completion_timeout);

    /*
     * Automatic completion must never block text input.  It scans text and may
     * touch import-context logic, so keep it for genuinely small buffers only.
     * Manual completion is still available in larger files.
     */
    if (!editor_tab_live_features_allowed(tab) ||
        (guint)gtk_text_buffer_get_char_count(tab->buffer) > GRAPTOS_AUTO_COMPLETION_MAX_CHARS) {
        if (tab->completion_popover) graptos_popover_hide(tab->completion_popover);
        return;
    }

    tab->completion_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                               GRAPTOS_COMPLETION_DELAY_MS,
                                               completion_timeout_cb,
                                               tab,
                                               NULL);
}

/**
 * @brief Completion accept selected.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void completion_accept_selected(EditorTab *tab) {
    if (!tab || !tab->completion_list) return;
    GtkListBox *list = GTK_LIST_BOX(tab->completion_list);
    GtkListBoxRow *row = gtk_list_box_get_selected_row(list);
    if (!row) row = gtk_list_box_get_row_at_index(list, 0);
    const char *word = row ? g_object_get_data(
        G_OBJECT(row), "graptos-completion-word") : NULL;
    while (!word && row) {
        row = gtk_list_box_get_row_at_index(list,
                                            gtk_list_box_row_get_index(row) + 1);
        word = row ? g_object_get_data(G_OBJECT(row), "graptos-completion-word") : NULL;
    }
    completion_insert_word(tab, word);
}

/**
 * @brief Completion select delta.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param delta The delta supplied by the caller.
 */
void completion_select_delta(EditorTab *tab, int delta) {
    if (!tab || !tab->completion_list) return;
    GtkListBox *list = GTK_LIST_BOX(tab->completion_list);
    GtkListBoxRow *row = gtk_list_box_get_selected_row(list);
    int index = row ? gtk_list_box_row_get_index(row) : 0;
    int next = index + delta;
    if (next < 0) next = 0;
    GtkListBoxRow *next_row = gtk_list_box_get_row_at_index(list, next);
    while (next_row &&
           !g_object_get_data(G_OBJECT(next_row), "graptos-completion-word")) {
        next += delta >= 0 ? 1 : -1;
        if (next < 0) return;
        next_row = gtk_list_box_get_row_at_index(list, next);
    }
    if (next_row) gtk_list_box_select_row(list, next_row);
}
