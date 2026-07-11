#include "editor_tab_private.h"

static void completion_add_row_with_detail(EditorTab *tab, const char *word, const char *detail);

static gboolean completion_contains(GPtrArray *items, const char *word) {
    if (!items || !word) return FALSE;
    for (guint i = 0u; i < items->len; i++) {
        const char *existing = g_ptr_array_index(items, i);
        if (existing && strcmp(existing, word) == 0) return TRUE;
    }
    return FALSE;
}

static void completion_copy_candidates(GPtrArray *dst, GPtrArray *src) {
    if (!dst || !src) return;
    for (guint i = 0u; i < src->len; i++) {
        const char *candidate = g_ptr_array_index(src, i);
        if (candidate && candidate[0] != '\0') {
            g_ptr_array_add(dst, g_strdup(candidate));
        }
    }
}

static void completion_merge_imports(GPtrArray *dst, GPtrArray *imports) {
    if (!dst || !imports) return;
    for (guint i = 0u; i < imports->len; i++) {
        const char *candidate = g_ptr_array_index(imports, i);
        if (!candidate || candidate[0] == '\0') continue;
        if (!completion_contains(dst, candidate)) {
            g_ptr_array_add(dst, g_strdup(candidate));
        }
    }
}

static void completion_merge_indexed(EditorTab *tab, GPtrArray *dst,
                                     const char *prefix) {
    if (!tab || !dst || !prefix) return;
    GPtrArray *indexed = index_candidates_for_tab(
        tab, prefix, CLEAF_COMPLETION_DEFAULT_MAX_RESULTS);
    if (!indexed) return;

    for (guint i = 0u; i < indexed->len; i++) {
        completion_candidates_add(dst, g_ptr_array_index(indexed, i),
                                  prefix,
                                  CLEAF_COMPLETION_DEFAULT_MAX_RESULTS);
    }
    g_ptr_array_free(indexed, TRUE);
}

static GPtrArray *completion_build_candidates(EditorTab *tab,
                                              const char *prefix,
                                              gboolean import_context,
                                              GPtrArray *imports,
                                              gboolean manual) {
    if (import_context) {
        GPtrArray *items = g_ptr_array_new_with_free_func(g_free);
        completion_copy_candidates(items, imports);
        return items;
    }

    GPtrArray *items = completion_candidates_new(
        tab->buffer, tab->active_syntax, prefix,
        CLEAF_COMPLETION_DEFAULT_MAX_RESULTS);
    if (!items) return NULL;

    completion_merge_imports(items, imports);
    if (manual) completion_merge_indexed(tab, items, prefix);
    return items;
}

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

static void completion_populate_rows(EditorTab *tab, GPtrArray *candidates) {
    completion_clear_rows(tab);
    if (!candidates) {
        completion_update_popup_size(tab, 0u, FALSE);
        return;
    }
    gboolean has_details = FALSE;
    gboolean allow_details = tab && tab->buffer && editor_tab_live_features_allowed(tab);
    for (guint i = 0u; i < candidates->len; i++) {
        const char *word = g_ptr_array_index(candidates, i);
        char *detail = allow_details && i < 10u
            ? completion_detail_from_current_buffer(tab, word)
            : NULL;
        if (detail && detail[0] != '\0') has_details = TRUE;
        completion_add_row_with_detail(tab, word, detail);
        g_free(detail);
    }
    completion_update_popup_size(tab, candidates ? candidates->len : 0u,
                                 has_details);
}

static void completion_place_popover(EditorTab *tab, GtkTextIter *cursor,
                                     gboolean manual) {
    if (!tab || !tab->text_view || !tab->popover_parent ||
        !gtk_widget_get_mapped(tab->text_view) ||
        !gtk_widget_get_mapped(tab->popover_parent) ||
        gtk_widget_get_allocated_width(tab->text_view) <= 0 ||
        gtk_widget_get_allocated_height(tab->text_view) <= 0 ||
        gtk_widget_get_allocated_width(tab->popover_parent) <= 0 ||
        gtk_widget_get_allocated_height(tab->popover_parent) <= 0) {
        return;
    }

    GdkRectangle location;
    gtk_text_view_get_iter_location(GTK_TEXT_VIEW(tab->text_view), cursor,
                                    &location);
    location.y += location.height;
    location.width = 1;
    if (location.height < 1) location.height = 1;

    if (!editor_tab_text_rect_to_popover_parent(tab, &location)) return;

    gtk_popover_set_pointing_to(GTK_POPOVER(tab->completion_popover),
                                &location);
    cleaf_popover_show(tab->completion_popover);
    gtk_widget_grab_focus(tab->text_view);

    GtkListBoxRow *first = gtk_list_box_get_row_at_index(
        GTK_LIST_BOX(tab->completion_list), 0);
    if (manual && first) gtk_list_box_select_row(
        GTK_LIST_BOX(tab->completion_list), first);
}

gboolean completion_is_visible(EditorTab *tab) {
    return tab && tab->completion_popover &&
           gtk_widget_get_visible(tab->completion_popover);
}

void editor_tab_hide_completion(EditorTab *tab) {
    if (!tab) return;
    if (tab->completion_timeout != 0u) {
        cleaf_source_cancel(&tab->completion_timeout);
    }
    if (tab->completion_popover) cleaf_popover_hide(tab->completion_popover);
    tab->completion_manual = FALSE;
    g_clear_pointer(&tab->completion_prefix, g_free);
}

void completion_clear_rows(EditorTab *tab) {
    if (!tab || !tab->completion_list) return;
    cleaf_list_box_clear(tab->completion_list);
}

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

void completion_row_activated(GtkListBox *box, GtkListBoxRow *row,
                              gpointer user_data) {
    (void)box;
    EditorTab *tab = user_data;
    const char *word = row ? g_object_get_data(
        G_OBJECT(row), "cleaf-completion-word") : NULL;
    completion_insert_word(tab, word);
}

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
    gtk_widget_add_css_class(label, "cleaf-completion-title");
    gtk_box_append(GTK_BOX(box), label);

    if (detail && detail[0] != '\0') {
        GtkWidget *detail_label = gtk_label_new(detail);
        gtk_widget_set_can_focus(detail_label, FALSE);
        gtk_label_set_xalign(GTK_LABEL(detail_label), 0.0f);
        gtk_label_set_ellipsize(GTK_LABEL(detail_label), PANGO_ELLIPSIZE_END);
        gtk_widget_add_css_class(detail_label, "cleaf-completion-detail");
        gtk_box_append(GTK_BOX(box), detail_label);
    }

    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data_full(G_OBJECT(row), "cleaf-completion-word",
                           g_strdup(word), g_free);
    gtk_list_box_insert(GTK_LIST_BOX(tab->completion_list), row, -1);
}

void completion_add_row(EditorTab *tab, const char *word) {
    completion_add_row_with_detail(tab, word, NULL);
}

void editor_tab_show_completion(EditorTab *tab, gboolean manual) {
    if (!tab || !tab->autocomplete_enabled) return;
    if (!manual && !editor_tab_live_features_allowed(tab)) return;

    GtkTextIter prefix_start;
    GtkTextIter cursor;
    char *prefix = completion_prefix_at_cursor(tab->buffer, &prefix_start,
                                               &cursor);
    if (!prefix) return;

    gboolean is_import = import_completion_tab_is_import_context(tab);
    if (!manual && !is_import && g_utf8_strlen(prefix, -1) < 2) {
        g_free(prefix);
        editor_tab_hide_completion(tab);
        return;
    }

    GPtrArray *imports = (manual || is_import)
        ? import_completion_candidates_for_tab(tab, prefix,
                                               CLEAF_COMPLETION_DEFAULT_MAX_RESULTS)
        : NULL;
    GPtrArray *candidates = completion_build_candidates(tab, prefix,
                                                        is_import,
                                                        imports,
                                                        manual);
    g_clear_pointer(&imports, g_ptr_array_unref);
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

gboolean completion_timeout_cb(gpointer user_data) {
    EditorTab *tab = user_data;
    if (!tab) return G_SOURCE_REMOVE;
    tab->completion_timeout = 0u;
    editor_tab_show_completion(tab, FALSE);
    return G_SOURCE_REMOVE;
}

void editor_tab_schedule_completion(EditorTab *tab) {
    if (!tab || !tab->autocomplete_enabled || !tab->buffer) return;
    cleaf_source_cancel(&tab->completion_timeout);

    /*
     * Automatic completion must never block text input.  It scans text and may
     * touch import-context logic, so keep it for genuinely small buffers only.
     * Manual completion is still available in larger files.
     */
    if (!editor_tab_live_features_allowed(tab) ||
        (guint)gtk_text_buffer_get_char_count(tab->buffer) > CLEAF_AUTO_COMPLETION_MAX_CHARS) {
        if (tab->completion_popover) cleaf_popover_hide(tab->completion_popover);
        return;
    }

    tab->completion_timeout = g_timeout_add_full(G_PRIORITY_LOW,
                                               CLEAF_COMPLETION_DELAY_MS,
                                               completion_timeout_cb,
                                               tab,
                                               NULL);
}

void completion_accept_selected(EditorTab *tab) {
    if (!tab || !tab->completion_list) return;
    GtkListBox *list = GTK_LIST_BOX(tab->completion_list);
    GtkListBoxRow *row = gtk_list_box_get_selected_row(list);
    if (!row) row = gtk_list_box_get_row_at_index(list, 0);
    const char *word = row ? g_object_get_data(
        G_OBJECT(row), "cleaf-completion-word") : NULL;
    completion_insert_word(tab, word);
}

void completion_select_delta(EditorTab *tab, int delta) {
    if (!tab || !tab->completion_list) return;
    GtkListBox *list = GTK_LIST_BOX(tab->completion_list);
    GtkListBoxRow *row = gtk_list_box_get_selected_row(list);
    int index = row ? gtk_list_box_row_get_index(row) : 0;
    int next = index + delta;
    if (next < 0) next = 0;
    GtkListBoxRow *next_row = gtk_list_box_get_row_at_index(list, next);
    if (next_row) gtk_list_box_select_row(list, next_row);
}
