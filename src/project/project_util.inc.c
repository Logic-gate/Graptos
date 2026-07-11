static void project_row_free(gpointer data) {
    ProjectRow *row = data;
    if (!row) return;
    g_free(row->path);
    g_free(row);
}

static void project_action_free(gpointer data) {
    ProjectAction *action = data;
    if (!action) return;
    g_free(action->path);
    g_free(action);
}

static ProjectAction *project_action_new(EditorWindow *win,
                                         const char *path,
                                         gboolean is_dir) {
    ProjectAction *action = g_new0(ProjectAction, 1);
    action->win = win;
    action->path = g_strdup(path);
    action->is_dir = is_dir;
    return action;
}

static gint compare_names(gconstpointer a, gconstpointer b) {
    const char *sa = *(char * const *)a;
    const char *sb = *(char * const *)b;
    gsize alen = sa ? strlen(sa) : 0u;
    gsize blen = sb ? strlen(sb) : 0u;
    gboolean ad = alen > 0u && sa[alen - 1u] == '/';
    gboolean bd = blen > 0u && sb[blen - 1u] == '/';

    if (ad != bd) return ad ? -1 : 1;
    return g_ascii_strcasecmp(sa ? sa : "", sb ? sb : "");
}

static gboolean should_skip_name(const char *name) {
    static const char *skip[] = {
        ".git", ".cache", ".venv", ".cleaf-backups", ".cleaf-autosave", ".cleaf-latex-build",
        "node_modules", "build", "dist", "target", "__pycache__", NULL
    };

    if (!name || name[0] == '\0') return TRUE;
    for (guint i = 0u; skip[i]; i++) {
        if (strcmp(name, skip[i]) == 0) return TRUE;
    }
    return FALSE;
}

static GPtrArray *sorted_dir_names(const char *path) {
    GDir *dir = g_dir_open(path, 0, NULL);
    if (!dir) return NULL;

    GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
    const char *name = NULL;
    while ((name = g_dir_read_name(dir)) != NULL) {
        if (should_skip_name(name)) continue;

        char *child = g_build_filename(path, name, NULL);
        gboolean is_dir = g_file_test(child, G_FILE_TEST_IS_DIR);
        char *sort_name = is_dir ? g_strdup_printf("%s/", name)
                                 : g_strdup(name);
        g_ptr_array_add(names, sort_name);
        g_free(child);
    }
    g_dir_close(dir);

    g_ptr_array_sort(names, compare_names);
    for (guint i = 0u; i < names->len; i++) {
        char *entry = g_ptr_array_index(names, i);
        gsize len = strlen(entry);
        if (len > 0u && entry[len - 1u] == '/') entry[len - 1u] = '\0';
    }
    return names;
}

static void add_icon_candidate(GPtrArray *candidates, const char *name) {
    if (!candidates || !name || name[0] == '\0') return;

    for (guint i = 0u; i < candidates->len; i++) {
        const char *existing = g_ptr_array_index(candidates, i);
        if (g_strcmp0(existing, name) == 0) return;
    }
    g_ptr_array_add(candidates, g_strdup(name));
}

static void add_icon_candidates_from_gicon(GPtrArray *candidates, GIcon *icon) {
    if (!candidates || !icon) return;

    if (G_IS_THEMED_ICON(icon)) {
        const char * const *names = g_themed_icon_get_names(G_THEMED_ICON(icon));
        if (names) {
            for (guint i = 0u; names[i]; i++) add_icon_candidate(candidates, names[i]);
        }
    }
}

static void add_extension_icon_candidates(GPtrArray *candidates, const char *path) {
    if (!candidates || !path) return;

    const char *dot = strrchr(path, '.');
    if (!dot || dot[1] == '\0') return;

    const char *ext = dot + 1;
    if (g_ascii_strcasecmp(ext, "c") == 0) {
        add_icon_candidate(candidates, "text-x-csrc");
    } else if (g_ascii_strcasecmp(ext, "h") == 0) {
        add_icon_candidate(candidates, "text-x-chdr");
    } else if (g_ascii_strcasecmp(ext, "cc") == 0
               || g_ascii_strcasecmp(ext, "cpp") == 0
               || g_ascii_strcasecmp(ext, "cxx") == 0) {
        add_icon_candidate(candidates, "text-x-c++src");
    } else if (g_ascii_strcasecmp(ext, "hh") == 0
               || g_ascii_strcasecmp(ext, "hpp") == 0
               || g_ascii_strcasecmp(ext, "hxx") == 0) {
        add_icon_candidate(candidates, "text-x-c++hdr");
    } else if (g_ascii_strcasecmp(ext, "py") == 0) {
        add_icon_candidate(candidates, "text-x-python");
    } else if (g_ascii_strcasecmp(ext, "js") == 0
               || g_ascii_strcasecmp(ext, "mjs") == 0
               || g_ascii_strcasecmp(ext, "cjs") == 0) {
        add_icon_candidate(candidates, "application-javascript");
        add_icon_candidate(candidates, "text-x-javascript");
    } else if (g_ascii_strcasecmp(ext, "ts") == 0
               || g_ascii_strcasecmp(ext, "tsx") == 0) {
        add_icon_candidate(candidates, "text-x-typescript");
        add_icon_candidate(candidates, "application-typescript");
    } else if (g_ascii_strcasecmp(ext, "json") == 0) {
        add_icon_candidate(candidates, "application-json");
    } else if (g_ascii_strcasecmp(ext, "md") == 0
               || g_ascii_strcasecmp(ext, "markdown") == 0) {
        add_icon_candidate(candidates, "text-markdown");
    } else if (g_ascii_strcasecmp(ext, "html") == 0
               || g_ascii_strcasecmp(ext, "htm") == 0) {
        add_icon_candidate(candidates, "text-html");
    } else if (g_ascii_strcasecmp(ext, "css") == 0) {
        add_icon_candidate(candidates, "text-css");
    } else if (g_ascii_strcasecmp(ext, "sh") == 0
               || g_ascii_strcasecmp(ext, "bash") == 0) {
        add_icon_candidate(candidates, "application-x-shellscript");
        add_icon_candidate(candidates, "text-x-script");
    } else if (g_ascii_strcasecmp(ext, "xml") == 0) {
        add_icon_candidate(candidates, "text-xml");
        add_icon_candidate(candidates, "application-xml");
    } else if (g_ascii_strcasecmp(ext, "rs") == 0) {
        add_icon_candidate(candidates, "text-rust");
    } else if (g_ascii_strcasecmp(ext, "go") == 0) {
        add_icon_candidate(candidates, "text-x-go");
    } else if (g_ascii_strcasecmp(ext, "java") == 0) {
        add_icon_candidate(candidates, "text-x-java");
    } else if (g_ascii_strcasecmp(ext, "php") == 0) {
        add_icon_candidate(candidates, "application-x-php");
        add_icon_candidate(candidates, "text-x-php");
    } else if (g_ascii_strcasecmp(ext, "sql") == 0) {
        add_icon_candidate(candidates, "text-x-sql");
    } else if (g_ascii_strcasecmp(ext, "yaml") == 0
               || g_ascii_strcasecmp(ext, "yml") == 0) {
        add_icon_candidate(candidates, "text-x-yaml");
    } else if (g_ascii_strcasecmp(ext, "toml") == 0) {
        add_icon_candidate(candidates, "text-x-toml");
    } else if (g_ascii_strcasecmp(ext, "tex") == 0) {
        add_icon_candidate(candidates, "text-x-tex");
    }
}

static GPtrArray *project_icon_candidates_for_path(const char *path,
                                                   gboolean is_dir) {
    GPtrArray *candidates = g_ptr_array_new_with_free_func(g_free);

    if (is_dir) {
        add_icon_candidate(candidates, "folder");
        add_icon_candidate(candidates, "folder-symbolic");
        return candidates;
    }

    add_extension_icon_candidates(candidates, path);

    gboolean uncertain = FALSE;
    char *content_type = path
        ? g_content_type_guess(path, NULL, 0, &uncertain)
        : NULL;
    (void)uncertain;

    if (content_type) {
        GIcon *icon = g_content_type_get_icon(content_type);
        add_icon_candidates_from_gicon(candidates, icon);
        if (icon) g_object_unref(icon);
        g_free(content_type);
    }

    add_icon_candidate(candidates, "text-x-generic");
    add_icon_candidate(candidates, "application-x-generic");
    add_icon_candidate(candidates, "text-x-generic-symbolic");
    return candidates;
}

