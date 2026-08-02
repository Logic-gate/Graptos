/**
 * @file path_autocomplete.c
 * @brief Native plugin that completes filesystem paths.
 * @details Path detection and directory listing live in the plugin. Graptoς
 *          only provides the cursor context and renders the completion popup.
 */

#include "plugin_api.h"

#include <glib/gstdio.h>
#include <string.h>

#define PATH_COMPLETION_MAX_RESULTS 64u

/**
 * @brief Return whether a character separates path tokens.
 * @param ch Character to inspect.
 * @return TRUE when the character ends the current path token.
 */
static gboolean path_token_separator(gunichar ch) {
    return g_unichar_isspace(ch) ||
           ch == (gunichar)'"' ||
           ch == (gunichar)'\'' ||
           ch == (gunichar)'`' ||
           ch == (gunichar)'(' ||
           ch == (gunichar)'[' ||
           ch == (gunichar)'{' ||
           ch == (gunichar)'<' ||
           ch == (gunichar)'=';
}

/**
 * @brief Extract the path-like token before the cursor.
 * @param line_prefix Text before the cursor on the active line.
 * @return Owned path token, or NULL when no path-like token is present.
 */
static char *path_token_before_cursor(const char *line_prefix) {
    if (!line_prefix || !line_prefix[0]) return NULL;
    const char *start = line_prefix;
    const char *p = line_prefix;
    while (*p) {
        const char *next = g_utf8_next_char(p);
        if (path_token_separator(g_utf8_get_char(p))) start = next;
        p = next;
    }
    if (!start || !start[0]) return NULL;
    char *token = g_strdup(start);
    if (!token) return NULL;
    if (g_str_has_prefix(token, "/") ||
        g_str_has_prefix(token, "~/") ||
        g_strcmp0(token, "~") == 0 ||
        g_str_has_prefix(token, "./") ||
        g_str_has_prefix(token, "../") ||
        strchr(token, '/')) {
        return token;
    }
    g_free(token);
    return NULL;
}

/**
 * @brief Expand a token path for filesystem access.
 * @param token Path token from the editor.
 * @return Owned expanded path.
 */
static char *expand_token_path(const char *token) {
    if (!token) return NULL;
    if (g_strcmp0(token, "~") == 0) return g_strdup(g_get_home_dir());
    if (g_str_has_prefix(token, "~/")) {
        return g_build_filename(g_get_home_dir(), token + 2, NULL);
    }
    return g_strdup(token);
}

/**
 * @brief Split a path token into directory and name prefix.
 * @param token Editor path token.
 * @param expanded_dir_out Owned filesystem directory path.
 * @param display_dir_out Owned editor-visible directory prefix.
 * @param name_prefix_out Owned filename prefix.
 * @return TRUE when the token can be completed.
 */
static gboolean split_path_token(const char *token,
                                 char **expanded_dir_out,
                                 char **display_dir_out,
                                 char **name_prefix_out) {
    if (!token || !expanded_dir_out || !display_dir_out || !name_prefix_out) {
        return FALSE;
    }
    const char *slash = strrchr(token, '/');
    char *display_dir = NULL;
    char *name_prefix = NULL;
    if (slash) {
        display_dir = g_strndup(token, (gsize)(slash - token) + 1u);
        name_prefix = g_strdup(slash + 1);
    } else if (g_strcmp0(token, "~") == 0) {
        display_dir = g_strdup("~/");
        name_prefix = g_strdup("");
    } else {
        return FALSE;
    }

    char *expanded_display_dir = expand_token_path(display_dir);
    if (!expanded_display_dir || !g_file_test(expanded_display_dir, G_FILE_TEST_IS_DIR)) {
        g_free(display_dir);
        g_free(name_prefix);
        g_free(expanded_display_dir);
        return FALSE;
    }

    *expanded_dir_out = expanded_display_dir;
    *display_dir_out = display_dir;
    *name_prefix_out = name_prefix;
    return TRUE;
}

/**
 * @brief Return whether a candidate already exists.
 * @param items Candidate array.
 * @param value Candidate text.
 * @return TRUE when value is already present.
 */
static gboolean candidate_exists(GPtrArray *items, const char *value) {
    if (!items || !value) return FALSE;
    for (guint i = 0u; i < items->len; i++) {
        if (g_strcmp0(g_ptr_array_index(items, i), value) == 0) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Add one path completion candidate.
 * @param items Candidate array.
 * @param display_dir Editor-visible directory prefix.
 * @param expanded_dir Filesystem directory path.
 * @param name Directory entry name.
 */
static void add_path_candidate(GPtrArray *items,
                               const char *display_dir,
                               const char *expanded_dir,
                               const char *name) {
    if (!items || !display_dir || !expanded_dir || !name || name[0] == '.') {
        return;
    }
    char *candidate = g_strconcat(display_dir, name, NULL);
    char *child = g_build_filename(expanded_dir, name, NULL);
    if (g_file_test(child, G_FILE_TEST_IS_DIR)) {
        char *with_slash = g_strconcat(candidate, "/", NULL);
        g_free(candidate);
        candidate = with_slash;
    }
    if (!candidate_exists(items, candidate)) g_ptr_array_add(items, candidate);
    else g_free(candidate);
    g_free(child);
}

/**
 * @brief Build path completion candidates for the cursor.
 * @param context Command context supplied by Graptoς.
 * @param replace_prefix_out Owned replacement prefix returned to Graptoς.
 * @param user_data Plugin data supplied during registration.
 * @return GPtrArray of owned char* candidates, or NULL.
 */
static GPtrArray *path_completion_candidates(GraptosPluginCommandContext *context,
                                             char **replace_prefix_out,
                                             gpointer user_data) {
    (void)user_data;
    if (replace_prefix_out) *replace_prefix_out = NULL;
    char *line_prefix = graptos_plugin_context_line_prefix(context);
    char *token = path_token_before_cursor(line_prefix);
    g_free(line_prefix);
    if (!token) return NULL;

    char *expanded_dir = NULL;
    char *display_dir = NULL;
    char *name_prefix = NULL;
    if (!split_path_token(token, &expanded_dir, &display_dir, &name_prefix)) {
        g_free(token);
        return NULL;
    }
    if (!g_path_is_absolute(expanded_dir) && !g_str_has_prefix(display_dir, "~")) {
        char *file_path = graptos_plugin_context_file_path(context);
        if (file_path) {
            char *base_dir = g_path_get_dirname(file_path);
            char *resolved = g_build_filename(base_dir, expanded_dir, NULL);
            g_free(expanded_dir);
            expanded_dir = resolved;
            g_free(base_dir);
            g_free(file_path);
        }
    }

    GDir *dir = g_dir_open(expanded_dir, 0, NULL);
    if (!dir) {
        g_free(token);
        g_free(expanded_dir);
        g_free(display_dir);
        g_free(name_prefix);
        return NULL;
    }

    GPtrArray *items = g_ptr_array_new_with_free_func(g_free);
    const char *name = NULL;
    while (items && items->len < PATH_COMPLETION_MAX_RESULTS &&
           (name = g_dir_read_name(dir)) != NULL) {
        if (name_prefix[0] != '\0' && !g_str_has_prefix(name, name_prefix)) {
            continue;
        }
        add_path_candidate(items, display_dir, expanded_dir, name);
    }
    g_dir_close(dir);

    if (!items || items->len == 0u) {
        if (items) g_ptr_array_free(items, TRUE);
        items = NULL;
    } else if (replace_prefix_out) {
        *replace_prefix_out = g_strdup(token);
    }

    g_free(token);
    g_free(expanded_dir);
    g_free(display_dir);
    g_free(name_prefix);
    return items;
}

/**
 * @brief Show path completions for the shortcut command.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void show_path_completion(GraptosPluginCommandContext *context,
                                 gpointer user_data) {
    char *replace_prefix = NULL;
    GPtrArray *items = path_completion_candidates(context,
                                                  &replace_prefix,
                                                  user_data);
    if (!items || items->len == 0u || !replace_prefix) {
        graptos_plugin_context_set_status(context, "No path completions");
        g_free(replace_prefix);
        if (items) g_ptr_array_free(items, TRUE);
        return;
    }
    graptos_plugin_context_show_completions(context,
                                            replace_prefix,
                                            "Paths",
                                            items);
    g_free(replace_prefix);
    g_ptr_array_free(items, TRUE);
}

/**
 * @brief Register this plugin with Graptoς.
 * @param host Host capability object supplied by Graptoς.
 * @return TRUE when registration succeeds.
 */
gboolean graptos_plugin_register(GraptosPluginHost *host) {
    if (graptos_plugin_host_api_version(host) != GRAPTOS_PLUGIN_API_VERSION) {
        return FALSE;
    }
    gboolean ok = graptos_plugin_host_register_completion_provider(host,
                                                                   "path-autocomplete",
                                                                   "Paths",
                                                                   path_completion_candidates,
                                                                   NULL,
                                                                   NULL);
    ok = graptos_plugin_host_register_editor_line_command_with_shortcut(host,
                                                                        "path-autocomplete-show",
                                                                        "Path Autocomplete",
                                                                        "Ctrl+Alt+P",
                                                                        show_path_completion,
                                                                        NULL,
                                                                        NULL) && ok;
    return ok;
}
