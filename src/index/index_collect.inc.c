
static gboolean ascii_word_start(char ch) {
    return g_ascii_isalpha(ch) || ch == '_';
}

static gboolean ascii_word_char(char ch) {
    return g_ascii_isalnum(ch) || ch == '_';
}

static char *tab_text(EditorTab *tab) {
    if (!tab || !tab->buffer) return NULL;
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(tab->buffer, &start, &end);
    return gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
}

static gboolean has_word_boundary(const char *line, const char *word) {
    if (!line || !word || word[0] == '\0') return FALSE;
    const char *p = line;
    gsize len = strlen(word);
    while ((p = strstr(p, word)) != NULL) {
        char before = p == line ? '\0' : p[-1];
        char after = p[len];
        if (!ascii_word_char(before) && !ascii_word_char(after)) return TRUE;
        p += len;
    }
    return FALSE;
}

/*
 * Index only small UTF-8 source-like files.  Skipping binaries and very large
 * files is intentional: reference popovers should answer from a bounded cache,
 * not stall while reading arbitrary project data.
 */
static gboolean read_small_file(const char *path, char **out_text) {
    if (out_text) *out_text = NULL;
    if (!path || !out_text) return FALSE;
    struct stat st;
    if (stat(path, &st) != 0) return FALSE;
    if (!S_ISREG(st.st_mode)) return FALSE;
    if (st.st_size < 0 || (guint64)st.st_size > (guint64)CLEAF_INDEX_MAX_FILE_BYTES) return FALSE;
    char *text = NULL;
    gsize len = 0u;
    GError *error = NULL;
    if (!g_file_get_contents(path, &text, &len, &error)) {
        g_clear_error(&error);
        return FALSE;
    }
    if (!g_utf8_validate(text, (gssize)len, NULL)) {
        g_free(text);
        return FALSE;
    }
    *out_text = text;
    return TRUE;
}

static void add_unique(GPtrArray *out, GHashTable *seen, const char *word, const char *prefix, guint max_results) {
    if (!out || !seen || !word || !prefix) return;
    if (out->len >= max_results) return;
    gsize word_len = strlen(word);
    gsize prefix_len = strlen(prefix);
    if (prefix_len < 2u || word_len <= prefix_len || word_len > 96u) return;
    if (strncmp(word, prefix, prefix_len) != 0) return;
    if (g_hash_table_contains(seen, word)) return;
    g_hash_table_add(seen, g_strdup(word));
    g_ptr_array_add(out, g_strdup(word));
}

static void collect_identifiers(GPtrArray *out, GHashTable *seen, const char *text, const char *prefix, guint max_results) {
    if (!out || !seen || !text || !prefix) return;
    const char *p = text;
    while (*p && out->len < max_results) {
        if (!ascii_word_start(*p)) {
            p++;
            continue;
        }
        const char *start = p++;
        while (*p && ascii_word_char(*p)) p++;
        char *word = g_strndup(start, (gsize)(p - start));
        add_unique(out, seen, word, prefix, max_results);
        g_free(word);
    }
}

static void collect_c_declarations(GPtrArray *out, GHashTable *seen, const char *text, const char *prefix, guint max_results) {
    if (!text) return;
    const char *p = text;
    while (*p && out->len < max_results) {
        const char *line_start = p;
        const char *line_end = strchr(p, '\n');
        if (!line_end) line_end = p + strlen(p);
        gsize len = (gsize)(line_end - line_start);
        if (len > CLEAF_INDEX_MAX_LINE) len = CLEAF_INDEX_MAX_LINE;
        char *line = g_strndup(line_start, len);
        char *trim = g_strstrip(line);

        if (g_str_has_prefix(trim, "#define")) {
            char *q = trim + 7;
            while (*q && g_ascii_isspace(*q)) q++;
            const char *start = q;
            if (ascii_word_start(*q)) {
                q++;
                while (*q && ascii_word_char(*q)) q++;
                char *word = g_strndup(start, (gsize)(q - start));
                add_unique(out, seen, word, prefix, max_results);
                g_free(word);
            }
        }

        const char *markers[] = {"struct ", "enum ", "union ", "typedef ", NULL};
        for (guint i = 0u; markers[i] && out->len < max_results; i++) {
            char *m = strstr(trim, markers[i]);
            if (!m) continue;
            char *q = m + strlen(markers[i]);
            while (*q && !ascii_word_start(*q)) q++;
            if (!*q) continue;
            const char *start = q;
            q++;
            while (*q && ascii_word_char(*q)) q++;
            char *word = g_strndup(start, (gsize)(q - start));
            add_unique(out, seen, word, prefix, max_results);
            g_free(word);
        }

        char *paren = strchr(trim, '(');
        if (paren) {
            char *q = paren;
            while (q > trim && g_ascii_isspace(q[-1])) q--;
            char *end = q;
            while (q > trim && ascii_word_char(q[-1])) q--;
            if (q < end && ascii_word_start(*q)) {
                char *word = g_strndup(q, (gsize)(end - q));
                if (strcmp(word, "if") != 0 && strcmp(word, "for") != 0 && strcmp(word, "while") != 0 && strcmp(word, "switch") != 0) {
                    add_unique(out, seen, word, prefix, max_results);
                }
                g_free(word);
            }
        }

        g_free(line);
        p = *line_end == '\n' ? line_end + 1 : line_end;
    }
}

static gboolean add_file_path(GPtrArray *paths, GHashTable *seen, const char *path) {
    if (!paths || !seen || !path) return FALSE;
    char *canonical = g_canonicalize_filename(path, NULL);
    if (!canonical) return FALSE;
    if (g_hash_table_contains(seen, canonical)) {
        g_free(canonical);
        return FALSE;
    }
    g_hash_table_add(seen, g_strdup(canonical));
    g_ptr_array_add(paths, canonical);
    return TRUE;
}

static void collect_project_files_rec(GPtrArray *paths, GHashTable *seen, const char *dir, guint depth, GPtrArray *syntaxes) {
    if (!paths || !seen || !dir || depth > 8u || paths->len >= CLEAF_INDEX_MAX_PROJECT_FILES) return;
    GDir *gdir = g_dir_open(dir, 0, NULL);
    if (!gdir) return;
    const char *name = NULL;
    while ((name = g_dir_read_name(gdir)) != NULL && paths->len < CLEAF_INDEX_MAX_PROJECT_FILES) {
        if (strcmp(name, "build") == 0 || strcmp(name, "node_modules") == 0 || strcmp(name, ".git") == 0 ||
            strcmp(name, "dist") == 0 || strcmp(name, "target") == 0 || strcmp(name, "__pycache__") == 0 ||
            strcmp(name, ".cache") == 0 || strcmp(name, ".venv") == 0) continue;
        char *path = g_build_filename(dir, name, NULL);
        if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
            collect_project_files_rec(paths, seen, path, depth + 1u, syntaxes);
        } else if (syntax_path_is_indexable(syntaxes, path)) {
            add_file_path(paths, seen, path);
        }
        g_free(path);
    }
    g_dir_close(gdir);
}

static void collect_project_files_for_window(GPtrArray *paths,
                                             GHashTable *seen,
                                             EditorWindow *win) {
    if (!paths || !seen || !win) return;
    guint count = project_root_count(win);
    for (guint i = 0u; i < count && paths->len < CLEAF_INDEX_MAX_PROJECT_FILES; i++) {
        const char *root = project_root_at(win, i);
        if (root) collect_project_files_rec(paths, seen, root, 0u, win->syntaxes);
    }
}

static char *find_in_projects(EditorWindow *win, const char *basename) {
    if (!win || !basename) return NULL;
    GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    collect_project_files_for_window(paths, seen, win);
    char *found = NULL;
    for (guint i = 0u; i < paths->len; i++) {
        const char *path = g_ptr_array_index(paths, i);
        char *base = g_path_get_basename(path);
        if (base && strcmp(base, basename) == 0) {
            found = g_strdup(path);
            g_free(base);
            break;
        }
        g_free(base);
    }
    g_ptr_array_free(paths, TRUE);
    g_hash_table_destroy(seen);
    return found;
}

