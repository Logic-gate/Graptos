static void collect_include_paths_from_text(GPtrArray *paths, GHashTable *seen, EditorTab *tab, const char *text, guint depth);

static void add_include_candidate(GPtrArray *paths, GHashTable *seen, EditorTab *tab, const char *name, gboolean system_include) {
    if (!paths || !seen || !tab || !name || name[0] == '\0') return;
    if (!system_include && tab->file_path) {
        char *dir = g_path_get_dirname(tab->file_path);
        char *candidate = g_build_filename(dir, name, NULL);
        if (g_file_test(candidate, G_FILE_TEST_EXISTS)) add_file_path(paths, seen, candidate);
        g_free(candidate);
        g_free(dir);
    }
    if (!system_include && tab->win && project_has_roots(tab->win)) {
        char *found = find_in_projects(tab->win, name);
        if (found) {
            add_file_path(paths, seen, found);
            g_free(found);
        }
    }
    if (system_include) {
        static const char *roots[] = {
            "/usr/include", "/usr/local/include", "/usr/include/x86_64-linux-gnu", NULL
        };
        for (guint i = 0u; roots[i]; i++) {
            char *candidate = g_build_filename(roots[i], name, NULL);
            if (g_file_test(candidate, G_FILE_TEST_EXISTS)) add_file_path(paths, seen, candidate);
            g_free(candidate);
        }
    }
}

static void collect_include_paths_from_text(GPtrArray *paths, GHashTable *seen, EditorTab *tab, const char *text, guint depth) {
    if (!paths || !seen || !tab || !text || depth > CLEAF_INDEX_MAX_INCLUDE_DEPTH) return;
    const char *p = text;
    while (*p) {
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);
        char *line = g_strndup(p, (gsize)(line_end - p));
        char *trim = g_strstrip(line);
        if (g_str_has_prefix(trim, "#include")) {
            char *open_quote = strchr(trim, '"');
            char *open_angle = strchr(trim, '<');
            if (open_quote) {
                char *close_quote = strchr(open_quote + 1, '"');
                if (close_quote && close_quote > open_quote + 1) {
                    char *name = g_strndup(open_quote + 1, (gsize)(close_quote - open_quote - 1));
                    add_include_candidate(paths, seen, tab, name, FALSE);
                    g_free(name);
                }
            } else if (open_angle) {
                char *close_angle = strchr(open_angle + 1, '>');
                if (close_angle && close_angle > open_angle + 1) {
                    char *name = g_strndup(open_angle + 1, (gsize)(close_angle - open_angle - 1));
                    add_include_candidate(paths, seen, tab, name, TRUE);
                    g_free(name);
                }
            }
        }
        g_free(line);
        p = *line_end == '\n' ? line_end + 1 : line_end;
    }

    guint initial = paths->len;
    for (guint i = 0u; i < initial; i++) {
        char *child_text = NULL;
        const char *path = g_ptr_array_index(paths, i);
        if (read_small_file(path, &child_text)) {
            collect_include_paths_from_text(paths, seen, tab, child_text, depth + 1u);
            g_free(child_text);
        }
    }
}

static gint sort_strings(gconstpointer a, gconstpointer b) {
    const char *sa = *(char * const *)a;
    const char *sb = *(char * const *)b;
    return g_ascii_strcasecmp(sa ? sa : "", sb ? sb : "");
}

GPtrArray *index_candidates_for_tab(EditorTab *tab, const char *prefix, guint max_results) {
    if (max_results == 0u) max_results = 64u;
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    if (!tab || !prefix || strlen(prefix) < 2u) return out;
    GHashTable *seen_words = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    char *text = tab_text(tab);
    collect_c_declarations(out, seen_words, text, prefix, max_results);

    GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
    GHashTable *seen_paths = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    collect_include_paths_from_text(paths, seen_paths, tab, text, 0u);

    if (tab->win) collect_project_files_for_window(paths, seen_paths, tab->win);

    for (guint i = 0u; i < paths->len && out->len < max_results; i++) {
        char *file_text = NULL;
        if (!read_small_file(g_ptr_array_index(paths, i), &file_text)) continue;
        collect_c_declarations(out, seen_words, file_text, prefix, max_results);
        if (out->len < max_results) collect_identifiers(out, seen_words, file_text, prefix, max_results);
        g_free(file_text);
    }

    g_ptr_array_sort(out, sort_strings);
    g_ptr_array_free(paths, TRUE);
    g_hash_table_destroy(seen_paths);
    g_hash_table_destroy(seen_words);
    g_free(text);
    return out;
}

