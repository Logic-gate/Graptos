/**
 * @file src/project_search.c
 * @brief Project-wide search and replace dialog implementation.
 * @details Projects change underneath the editor. We keep tree rows, filesystem context,
 *          and search helpers away from EditorTab so directory updates do not become
 *          buffer-management problems.
 */

#include "app_private.h"

#include <glib/gstdio.h>
#include <sys/stat.h>
#include <string.h>

/**
 * @brief Project search max file size macro.
 */
#define PROJECT_SEARCH_MAX_FILE_SIZE (2u * 1024u * 1024u)
/**
 * @brief Project search max depth macro.
 */
#define PROJECT_SEARCH_MAX_DEPTH 12u
/**
 * @brief Project search max results macro.
 */
#define PROJECT_SEARCH_MAX_RESULTS 1500u
/**
 * @brief Project search max dir entries macro.
 */
#define PROJECT_SEARCH_MAX_DIR_ENTRIES 4096u

/**
 * @brief Project search type definition.
 */
typedef struct {
    char *path; /**< Path. */
    char *snippet; /**< Snippet. */
    guint line; /**< Line. */
} ProjectMatch;

/**
 * @brief Project search type definition.
 */
typedef struct {
    EditorWindow *win; /**< Win. */
    GtkWidget *window; /**< Window. */
    GtkWidget *find_entry; /**< Find entry. */
    GtkWidget *replace_entry; /**< Replace entry. */
    GtkWidget *count_label; /**< Count label. */
    GtkWidget *result_list; /**< Result list. */
    GPtrArray *matches; /**< Matches. */
} ProjectSearch;

/**
 * @brief Project match free.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param data The callback context passed by the caller.
 */
static void project_match_free(gpointer data) {
    ProjectMatch *match = data;
    if (!match) return;
    g_free(match->path);
    g_free(match->snippet);
    g_free(match);
}

/**
 * @brief Skip dir name.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param name The name supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean skip_dir_name(const char *name) {
    static const char *skip[] = {
        ".git", "node_modules", "build", "dist", "target",
        "__pycache__", ".cache", ".venv", "venv", NULL
    };
    if (!name) return TRUE;
    for (guint i = 0u; skip[i]; i++) {
        if (strcmp(name, skip[i]) == 0) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Readable small file.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean readable_small_file(const char *path) {
    GStatBuf st;
    if (!path || g_stat(path, &st) != 0) return FALSE;
    if (!S_ISREG(st.st_mode)) return FALSE;
    return st.st_size >= 0 && (guint64)st.st_size <= PROJECT_SEARCH_MAX_FILE_SIZE;
}

/**
 * @brief Relative display path.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *relative_display_path(EditorWindow *win, const char *path) {
    const char *root = project_root_for_path(win, path);
    if (root && path) {
        const char *p = path + strlen(root);
        if (*p == G_DIR_SEPARATOR) p++;
        return g_strdup(p && *p ? p : path);
    }
    return g_strdup(path ? path : "");
}

/**
 * @brief Line for offset.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param text The text fragment supplied by the caller.
 * @param pos The pos supplied by the caller.
 * @return The computed value requested by the caller.
 */
static guint line_for_offset(const char *text, const char *pos) {
    guint line = 1u;
    if (!text || !pos) return line;
    for (const char *p = text; *p && p < pos; p++) {
        if (*p == '\n') line++;
    }
    return line;
}

/**
 * @brief Snippet for match.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param line_start The line start supplied by the caller.
 * @param match The match supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *snippet_for_match(const char *line_start, const char *match) {
    if (!line_start || !match) return g_strdup("");
    const char *line_end = strchr(match, '\n');
    if (!line_end) line_end = match + strlen(match);
    gsize len = (gsize)(line_end - line_start);
    if (len > 180u) len = 180u;
    char *snippet = g_strndup(line_start, len);
    if (snippet) g_strstrip(snippet);
    return snippet;
}

/**
 * @brief Add result row.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param state The state supplied by the caller.
 * @param match The match supplied by the caller.
 */
static void add_result_row(ProjectSearch *state, ProjectMatch *match) {
    if (!state || !match || !state->result_list) return;

    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(box, 8);
    gtk_widget_set_margin_end(box, 8);
    gtk_widget_set_margin_top(box, 5);
    gtk_widget_set_margin_bottom(box, 5);

    char *rel = relative_display_path(state->win, match->path);
    char *title = g_strdup_printf("%s:%u", rel, match->line);
    GtkWidget *title_label = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(title_label), 0.0f);
    gtk_widget_add_css_class(title_label, "graptos-ref-title");

    GtkWidget *snippet = gtk_label_new(match->snippet ? match->snippet : "");
    gtk_label_set_xalign(GTK_LABEL(snippet), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(snippet), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(snippet, "graptos-ref-snippet");

    gtk_box_append(GTK_BOX(box), title_label);
    gtk_box_append(GTK_BOX(box), snippet);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    g_object_set_data(G_OBJECT(row), "graptos-project-search-path", match->path);
    g_object_set_data(G_OBJECT(row), "graptos-project-search-line",
                      GUINT_TO_POINTER(match->line));
    gtk_list_box_insert(GTK_LIST_BOX(state->result_list), row, -1);
    g_free(title);
    g_free(rel);
}

/**
 * @brief Scan text for matches.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param state The state supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param query The query supplied by the caller.
 */
static void scan_text_for_matches(ProjectSearch *state,
                                  const char *path,
                                  const char *text,
                                  const char *query) {
    const char *p = text;
    while (p && *p && state->matches->len < PROJECT_SEARCH_MAX_RESULTS) {
        const char *hit = g_strstr_len(p, -1, query);
        if (!hit) break;
        const char *line_start = hit;
        while (line_start > text && *(line_start - 1) != '\n') line_start--;

        ProjectMatch *match = g_new0(ProjectMatch, 1);
        match->path = g_strdup(path);
        match->line = line_for_offset(text, hit);
        match->snippet = snippet_for_match(line_start, hit);
        g_ptr_array_add(state->matches, match);
        add_result_row(state, match);
        p = hit + strlen(query);
    }
}

/**
 * @brief Project scan dir.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param state The state supplied by the caller.
 * @param dir The dir supplied by the caller.
 * @param query The query supplied by the caller.
 * @param depth The depth supplied by the caller.
 */
static void project_scan_dir(ProjectSearch *state,
                             const char *dir,
                             const char *query,
                             guint depth) {
    if (!state || !dir || depth > PROJECT_SEARCH_MAX_DEPTH) return;
    if (state->matches->len >= PROJECT_SEARCH_MAX_RESULTS) return;

    GDir *gdir = g_dir_open(dir, 0, NULL);
    if (!gdir) return;

    const char *name = NULL;
    guint scanned = 0u;
    while ((name = g_dir_read_name(gdir)) != NULL &&
           scanned < PROJECT_SEARCH_MAX_DIR_ENTRIES) {
        scanned++;
        if (name[0] == '.' && strcmp(name, ".env") != 0) continue;
        char *path = g_build_filename(dir, name, NULL);
        if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
            if (!skip_dir_name(name)) project_scan_dir(state, path, query,
                                                       depth + 1u);
        } else if (syntax_path_is_indexable(state->win->syntaxes, path) &&
                   readable_small_file(path)) {
            g_autofree char *text = NULL;
            gsize len = 0u;
            g_autoptr(GError) error = NULL;
            if (g_file_get_contents(path, &text, &len, &error) && text) {
                scan_text_for_matches(state, path, text, query);
            }
        }
        g_free(path);
        if (state->matches->len >= PROJECT_SEARCH_MAX_RESULTS) break;
    }
    g_dir_close(gdir);
}

/**
 * @brief Clear results.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param state The state supplied by the caller.
 */
static void clear_results(ProjectSearch *state) {
    if (!state) return;
    if (state->matches) g_ptr_array_set_size(state->matches, 0u);
    if (state->result_list) graptos_list_box_clear(state->result_list);
}

/**
 * @brief Update count.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param state The state supplied by the caller.
 */
static void update_count(ProjectSearch *state) {
    if (!state || !state->count_label) return;
    char *text = g_strdup_printf("%u result%s", state->matches->len,
        state->matches->len == 1u ? "" : "s");
    gtk_label_set_text(GTK_LABEL(state->count_label), text);
    g_free(text);
}

/**
 * @brief Project search run.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void project_search_run(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ProjectSearch *state = user_data;
    if (!state || !state->win || !project_has_roots(state->win)) return;
    const char *query = gtk_entry_get_text(GTK_ENTRY(state->find_entry));
    clear_results(state);
    if (!query || query[0] == '\0') {
        update_count(state);
        return;
    }

    guint count = project_root_count(state->win);
    for (guint i = 0u; i < count && state->matches->len < PROJECT_SEARCH_MAX_RESULTS; i++) {
        const char *root = project_root_at(state->win, i);
        if (root) project_scan_dir(state, root, query, 0u);
    }
    update_count(state);
}

/**
 * @brief Replace literal.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param text The text fragment supplied by the caller.
 * @param find The find supplied by the caller.
 * @param replace The replace supplied by the caller.
 * @param count_out Output storage filled when the operation can provide a value.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *replace_literal(const char *text,
                             const char *find,
                             const char *replace,
                             guint *count_out) {
    GString *out = g_string_new(NULL);
    const char *p = text;
    gsize find_len = strlen(find);
    guint count = 0u;
    while (p && *p) {
        const char *hit = g_strstr_len(p, -1, find);
        if (!hit) {
            g_string_append(out, p);
            break;
        }
        g_string_append_len(out, p, hit - p);
        g_string_append(out, replace ? replace : "");
        p = hit + find_len;
        count++;
    }
    if (count_out) *count_out = count;
    return g_string_free(out, FALSE);
}

/**
 * @brief Replace in file.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param path The filesystem path supplied by the caller.
 * @param find The find supplied by the caller.
 * @param replace The replace supplied by the caller.
 * @return The computed value requested by the caller.
 */
static guint replace_in_file(const char *path,
                             const char *find,
                             const char *replace) {
    g_autofree char *text = NULL;
    gsize len = 0u;
    g_autoptr(GError) error = NULL;
    guint count = 0u;
    if (!g_file_get_contents(path, &text, &len, &error)) {
        return 0u;
    }
    g_autofree char *updated = replace_literal(text, find, replace, &count);
    if (count > 0u) {
        if (!g_file_set_contents(path, updated, -1, &error)) {
            count = 0u;
        }
    }
    return count;
}

/**
 * @brief Project replace all.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void project_replace_all(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ProjectSearch *state = user_data;
    if (!state || !state->win) return;

    const char *find = gtk_entry_get_text(GTK_ENTRY(state->find_entry));
    const char *replace = gtk_entry_get_text(GTK_ENTRY(state->replace_entry));
    if (!find || find[0] == '\0') return;
    if (state->matches->len == 0u) project_search_run(NULL, state);

    char *msg = g_strdup_printf("Replace %u project result%s?",
        state->matches->len, state->matches->len == 1u ? "" : "s");
    gboolean ok = dialog_confirm_yes_no(app_window_gtk(state->win),
                                        "Project Replace", msg);
    g_free(msg);
    if (!ok) return;

    GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
    guint replacements = 0u;
    guint files = 0u;
    for (guint i = 0u; i < state->matches->len; i++) {
        ProjectMatch *match = g_ptr_array_index(state->matches, i);
        if (!match || g_hash_table_contains(seen, match->path)) continue;
        g_hash_table_add(seen, match->path);
        guint changed = replace_in_file(match->path, find, replace);
        if (changed > 0u) {
            replacements += changed;
            files++;
        }
    }
    g_hash_table_destroy(seen);
    char *done = g_strdup_printf("Replaced %u occurrence%s in %u file%s.",
        replacements, replacements == 1u ? "" : "s",
        files, files == 1u ? "" : "s");
    dialog_info(app_window_gtk(state->win), "Project Replace Complete", done);
    g_free(done);
    project_search_run(NULL, state);
}

/**
 * @brief Result activated.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param box The box supplied by the caller.
 * @param row The row supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void result_activated(GtkListBox *box,
                             GtkListBoxRow *row,
                             gpointer user_data) {
    (void)box;
    ProjectSearch *state = user_data;
    if (!state || !row) return;
    const char *path = g_object_get_data(G_OBJECT(row),
                                         "graptos-project-search-path");
    guint line = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(row),
        "graptos-project-search-line"));
    if (path && app_window_open_file(state->win, path)) {
        editor_tab_jump_to_line(app_window_current_tab(state->win), line);
    }
}

/**
 * @brief Project search closed.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param window The window supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean project_search_closed(GtkWindow *window, gpointer user_data) {
    (void)window;
    ProjectSearch *state = user_data;
    if (!state) return FALSE;
    if (state->matches) g_ptr_array_free(state->matches, TRUE);
    g_free(state);
    return FALSE;
}

/**
 * @brief Action project search.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_project_search(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !project_has_roots(win)) {
        dialog_info(app_window_gtk(win), "No project folder",
                    "Open a project folder before using project search.");
        return;
    }

    ProjectSearch *state = g_new0(ProjectSearch, 1);
    state->win = win;
    state->matches = g_ptr_array_new_with_free_func(project_match_free);
    state->window = gtk_window_new();
    gtk_widget_add_css_class(state->window, "graptos-window");
    gtk_window_set_title(GTK_WINDOW(state->window), "Project Search");
    gtk_window_set_default_size(GTK_WINDOW(state->window), 820, 520);
    gtk_window_set_transient_for(GTK_WINDOW(state->window), app_window_gtk(win));

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(root, "graptos-root");
    graptos_set_all_margins(root, 10);
    gtk_window_set_child(GTK_WINDOW(state->window), root);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    state->find_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->find_entry), "Find in project");
    gtk_widget_set_hexpand(state->find_entry, TRUE);
    state->replace_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->replace_entry), "Replace with");
    gtk_widget_set_hexpand(state->replace_entry, TRUE);
    gtk_box_append(GTK_BOX(row), state->find_entry);
    gtk_box_append(GTK_BOX(row), state->replace_entry);
    gtk_box_append(GTK_BOX(row), graptos_flat_button_new(
        "Search", NULL, G_CALLBACK(project_search_run), state));
    gtk_box_append(GTK_BOX(row), graptos_flat_button_new(
        "Replace All", NULL, G_CALLBACK(project_replace_all), state));
    gtk_box_append(GTK_BOX(root), row);

    state->count_label = gtk_label_new("0 results");
    gtk_label_set_xalign(GTK_LABEL(state->count_label), 0.0f);
    gtk_box_append(GTK_BOX(root), state->count_label);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    state->result_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(state->result_list),
                                    GTK_SELECTION_SINGLE);
    g_signal_connect(state->result_list, "row-activated",
                     G_CALLBACK(result_activated), state);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),
                                  state->result_list);
    gtk_box_append(GTK_BOX(root), scrolled);

    g_signal_connect(state->window, "close-request",
                     G_CALLBACK(project_search_closed), state);
    gtk_window_present(GTK_WINDOW(state->window));
    gtk_widget_grab_focus(state->find_entry);
}
