/**
 * @file src/index/index_references.inc.c
 * @brief Cleaf index references module.
 */

static char *definition_in_text(const char *path_label, const char *text, const char *word) {
    if (!text || !word || word[0] == '\0') return NULL;
    const char *p = text;
    guint line_no = 1u;
    while (*p) {
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);
        gsize len = (gsize)(line_end - p);
        if (len > CLEAF_INDEX_MAX_LINE) len = CLEAF_INDEX_MAX_LINE;
        char *line = g_strndup(p, len);
        char *trim = g_strstrip(line);
        gboolean match = FALSE;
        if (g_str_has_prefix(trim, "#define")) {
            const char *q = trim + 7;
            while (*q && g_ascii_isspace(*q)) q++;
            match = g_str_has_prefix(q, word) && !ascii_word_char(q[strlen(word)]);
        }
        if (!match && has_word_boundary(trim, word)) {
            if (strstr(trim, "typedef ") || strstr(trim, "struct ") || strstr(trim, "enum ") || strstr(trim, "union ") || strchr(trim, '(') || strchr(trim, '=')) match = TRUE;
        }
        if (match) {
            char *out = g_strdup_printf("%s:%u\n%s", path_label ? path_label : "current buffer", line_no, trim);
            g_free(line);
            return out;
        }
        g_free(line);
        p = *line_end == '\n' ? line_end + 1 : line_end;
        line_no++;
    }
    return NULL;
}

/**
 * @brief Index definition for word.
 */
char *index_definition_for_word(EditorTab *tab, const char *word) {
    if (!tab || !word || strlen(word) < 2u) return NULL;
    char *text = tab_text(tab);
    char *def = definition_in_text("current buffer", text, word);
    if (def) {
        g_free(text);
        return def;
    }

    GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
    GHashTable *seen_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    collect_include_paths_from_text(paths, seen_paths, tab, text, 0u);
    if (tab->win) collect_project_files_for_window(paths, seen_paths, tab->win);
    for (guint i = 0u; i < paths->len; i++) {
        const char *path = g_ptr_array_index(paths, i);
        char *file_text = NULL;
        if (!read_small_file(path, &file_text)) continue;
        def = definition_in_text(path, file_text, word);
        g_free(file_text);
        if (def) break;
    }
    g_ptr_array_free(paths, TRUE);
    g_hash_table_destroy(seen_paths);
    g_free(text);
    return def;
}

/**
 * @brief Index reference free.
 */
void index_reference_free(gpointer data) {
    IndexReference *ref = data;
    if (!ref) return;
    g_free(ref->path);
    g_free(ref->display);
    g_free(ref->snippet);
    g_free(ref->kind);
    g_free(ref);
}

/**
 * @brief Relative display path.
 */
static char *relative_display_path(EditorTab *tab, const char *path) {
    if (!path) return g_strdup("current buffer");
    const char *root = tab && tab->win ? project_root_for_path(tab->win, path) : NULL;
    if (root) {
        const char *rel = path + strlen(root);
        if (*rel == G_DIR_SEPARATOR) rel++;
        return g_strdup(rel && *rel ? rel : path);
    }
    return g_filename_display_name(path);
}

/**
 * @brief Language for path.
 */
static const char *language_for_path(EditorTab *tab, const char *path) {
    if (!path) return "Buffer";
    return syntax_language_for_path(tab && tab->win ? tab->win->syntaxes : NULL, path);
}

/**
 * @brief Line looks definition.
 */
static gboolean line_looks_definition(const char *trim, const char *word) {
    if (!trim || !word || word[0] == '\0') return FALSE;
    gsize wlen = strlen(word);
    if (g_str_has_prefix(trim, "#define")) {
        const char *q = trim + 7;
        while (*q && g_ascii_isspace(*q)) q++;
        return g_str_has_prefix(q, word) && !ascii_word_char(q[wlen]);
    }
    if (!has_word_boundary(trim, word)) return FALSE;
    if (strstr(trim, "function ")) return TRUE;
    if (strstr(trim, "const ") || strstr(trim, "let ") || strstr(trim, "var ")) return TRUE;
    if (strstr(trim, "typedef ") || strstr(trim, "struct ") || strstr(trim, "enum ") || strstr(trim, "union ")) return TRUE;
    if (strstr(trim, "def ") || strstr(trim, "class ")) return TRUE;
    if (strchr(trim, '(') && strchr(trim, ')') && strchr(trim, '{')) return TRUE;
    if (strchr(trim, '=') && !strstr(trim, "==") && !strstr(trim, "=>")) return TRUE;
    return FALSE;
}

/**
 * @brief Seen reference.
 */
static gboolean seen_reference(GHashTable *seen, const char *path, guint line) {
    char *key = g_strdup_printf("%s:%u", path ? path : "<buffer>", line);
    gboolean found = g_hash_table_contains(seen, key);
    if (!found) g_hash_table_add(seen, key);
    else g_free(key);
    return found;
}

/**
 * @brief Add reference.
 */
static void add_reference(GPtrArray *out, GHashTable *seen, EditorTab *tab, const char *path, guint line, const char *snippet, const char *kind, guint max_results) {
    if (!out || !seen || !snippet || out->len >= max_results) return;
    if (seen_reference(seen, path, line)) return;
    IndexReference *ref = g_new0(IndexReference, 1);
    if (!ref) return;
    ref->path = path ? g_strdup(path) : NULL;
    ref->display = relative_display_path(tab, path);
    ref->line = line;
    ref->snippet = g_strdup(snippet);
    ref->kind = g_strdup(kind ? kind : language_for_path(tab, path));
    g_ptr_array_add(out, ref);
}

/**
 * @brief Collect references from text.
 */
static void collect_references_from_text(GPtrArray *out, GHashTable *seen, EditorTab *tab, const char *path, const char *text, const char *word, guint max_results, gboolean definitions_first) {
    if (!out || !text || !word) return;
    const char *p = text;
    guint line_no = 1u;
    while (*p && out->len < max_results) {
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);
        gsize len = (gsize)(line_end - p);
        if (len > CLEAF_INDEX_MAX_LINE) len = CLEAF_INDEX_MAX_LINE;
        char *line = g_strndup(p, len);
        char *trim = g_strstrip(line);
        gboolean boundary = has_word_boundary(trim, word);
        gboolean is_def = boundary && line_looks_definition(trim, word);
        if ((definitions_first && is_def) || (!definitions_first && boundary && !is_def)) {
            add_reference(out, seen, tab, path, line_no, trim, is_def ? "definition" : language_for_path(tab, path), max_results);
        }
        g_free(line);
        p = *line_end == '\n' ? line_end + 1 : line_end;
        line_no++;
    }
}

/**
 * @brief Index references for word.
 */
GPtrArray *index_references_for_word(EditorTab *tab, const char *word, guint max_results) {
    if (max_results == 0u) max_results = 20u;
    GPtrArray *out = g_ptr_array_new_with_free_func(index_reference_free);
    if (!tab || !word || strlen(word) < 2u) return out;

    GHashTable *seen_refs = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    char *text = tab_text(tab);
    collect_references_from_text(out, seen_refs, tab, tab->file_path, text, word, max_results, TRUE);

    GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
    GHashTable *seen_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    collect_include_paths_from_text(paths, seen_paths, tab, text, 0u);
    if (tab->win) collect_project_files_for_window(paths, seen_paths, tab->win);

    for (guint i = 0u; i < paths->len && out->len < max_results; i++) {
        const char *path = g_ptr_array_index(paths, i);
        char *file_text = NULL;
        if (!read_small_file(path, &file_text)) continue;
        collect_references_from_text(out, seen_refs, tab, path, file_text, word, max_results, TRUE);
        g_free(file_text);
    }

    collect_references_from_text(out, seen_refs, tab, tab->file_path, text, word, max_results, FALSE);
    for (guint i = 0u; i < paths->len && out->len < max_results; i++) {
        const char *path = g_ptr_array_index(paths, i);
        char *file_text = NULL;
        if (!read_small_file(path, &file_text)) continue;
        collect_references_from_text(out, seen_refs, tab, path, file_text, word, max_results, FALSE);
        g_free(file_text);
    }

    g_ptr_array_free(paths, TRUE);
    g_hash_table_destroy(seen_paths);
    g_hash_table_destroy(seen_refs);
    g_free(text);
    return out;
}
