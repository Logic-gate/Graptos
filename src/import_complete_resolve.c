/**
 * @file src/import_complete_resolve.c
 * @brief Import path and member resolution helpers.
 * @details Import completion is intentionally pragmatic. We resolve enough project-local
 *          structure to be helpful when LSP is unavailable, without pretending this is a
 *          compiler for every language.
 */

#include "import_complete_private.h"
#include "app.h"
#include "project.h"

#include <glob.h>
#include <string.h>

/**
 * @brief Name is safe.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param name The name supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean name_is_safe(const char *name) {
    if (!name || name[0] == '\0') return FALSE;
    if (strlen(name) > GRAPTOS_IMPORT_MAX_NAME) return FALSE;
    if (strstr(name, "..")) return FALSE;
    for (const char *p = name; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch < 32u || ch == 127u) return FALSE;
    }
    return TRUE;
}

/**
 * @brief Prefix matches.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param word The symbol text being matched.
 * @param prefix The prefix supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean prefix_matches(const char *word, const char *prefix) {
    if (!word || !prefix) return FALSE;
    if (prefix[0] == '\0') return TRUE;
    return g_ascii_strncasecmp(word, prefix, strlen(prefix)) == 0;
}

/**
 * @brief Add candidate.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param out Output storage filled when the lookup succeeds.
 * @param seen The seen supplied by the caller.
 * @param word The symbol text being matched.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void add_candidate(GPtrArray *out,
                          GHashTable *seen,
                          const char *word,
                          const char *prefix,
                          guint max_results) {
    if (!out || !seen || !word || out->len >= max_results) return;
    if (!name_is_safe(word) || !prefix_matches(word, prefix ? prefix : "")) {
        return;
    }
    if (g_hash_table_contains(seen, word)) return;
    g_hash_table_add(seen, g_strdup(word));
    g_ptr_array_add(out, g_strdup(word));
}
/**
 * @brief Tab directory.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *tab_directory(EditorTab *tab) {
    if (tab && tab->file_path) return g_path_get_dirname(tab->file_path);
    return g_get_current_dir();
}

/**
 * @brief Add root.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param roots The roots supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 */
static void add_root(GPtrArray *roots, const char *path) {
    if (!roots || !path || path[0] == '\0') return;
    char *expanded = NULL;
    if (path[0] == '~') {
        expanded = g_build_filename(g_get_home_dir(), path + 1, NULL);
    } else {
        expanded = g_strdup(path);
    }
    if (!expanded) return;

    glob_t matches;
    memset(&matches, 0, sizeof(matches));
    if (strchr(expanded, '*') && glob(expanded, 0, NULL, &matches) == 0) {
        for (size_t i = 0u; i < matches.gl_pathc; i++) {
            if (g_file_test(matches.gl_pathv[i], G_FILE_TEST_IS_DIR)) {
                g_ptr_array_add(roots, g_strdup(matches.gl_pathv[i]));
            }
        }
        globfree(&matches);
        g_free(expanded);
        return;
    }
    if (g_file_test(expanded, G_FILE_TEST_IS_DIR)) {
        g_ptr_array_add(roots, expanded);
    } else {
        g_free(expanded);
    }
}

/**
 * @brief Add env roots.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param roots The roots supplied by the caller.
 * @param env_name The env name supplied by the caller.
 */
static void add_env_roots(GPtrArray *roots, const char *env_name) {
    const char *value = g_getenv(env_name);
    if (!value || value[0] == '\0') return;
    char **parts = g_strsplit(value, G_SEARCHPATH_SEPARATOR_S, -1);
    for (guint i = 0u; parts && parts[i]; i++) add_root(roots, parts[i]);
    g_strfreev(parts);
}

/**
 * @brief Collect roots.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param roots The roots supplied by the caller.
 */
static void collect_roots(EditorTab *tab, GPtrArray *roots) {
    SyntaxDef *syntax = tab ? tab->active_syntax : NULL;
    char *dir = tab_directory(tab);
    add_root(roots, dir);
    if (tab && tab->win) {
        guint root_count = project_root_count(tab->win);
        for (guint i = 0u; i < root_count; i++) {
            add_root(roots, project_root_at(tab->win, i));
        }
    }

    if (syntax && syntax->import_roots) {
        for (guint i = 0u; i < syntax->import_roots->len; i++) {
            const char *root = g_ptr_array_index(syntax->import_roots, i);
            if (!root || root[0] == '\0') continue;
            if (root[0] == '$') {
                const char *slash = strchr(root, '/');
                if (!slash) add_env_roots(roots, root + 1);
                else {
                    char *env = g_strndup(root + 1, (gsize)(slash - root - 1));
                    const char *value = g_getenv(env);
                    if (value) {
                        char **parts = g_strsplit(value, G_SEARCHPATH_SEPARATOR_S, -1);
                        for (guint j = 0u; parts && parts[j]; j++) {
                            char *joined = g_build_filename(parts[j], slash + 1, NULL);
                            add_root(roots, joined);
                            g_free(joined);
                        }
                        g_strfreev(parts);
                    }
                    g_free(env);
                }
            } else if (g_path_is_absolute(root) || root[0] == '~') {
                add_root(roots, root);
            } else {
                if (tab && tab->win) {
                    guint root_count = project_root_count(tab->win);
                    for (guint r = 0u; r < root_count; r++) {
                        const char *project_root = project_root_at(tab->win, r);
                        if (!project_root) continue;
                        char *p = g_build_filename(project_root, root, NULL);
                        add_root(roots, p);
                        g_free(p);
                    }
                }
                char *p = g_build_filename(dir, root, NULL);
                add_root(roots, p);
                g_free(p);
            }
        }
    }

    if (syntax && syntax->import_env) {
        for (guint i = 0u; i < syntax->import_env->len; i++) {
            add_env_roots(roots, g_ptr_array_index(syntax->import_env, i));
        }
    }
    g_free(dir);
}

/**
 * @brief Suffix allowed.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param syntax The syntax definition used by the editor path.
 * @param name The name supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean suffix_allowed(SyntaxDef *syntax, const char *name) {
    if (!syntax || !syntax->import_extensions ||
        syntax->import_extensions->len == 0u) {
        return TRUE;
    }
    for (guint i = 0u; i < syntax->import_extensions->len; i++) {
        const char *ext = g_ptr_array_index(syntax->import_extensions, i);
        if (ext && g_str_has_suffix(name, ext)) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Strip suffix.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param syntax The syntax definition used by the editor path.
 * @param name The name supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *strip_suffix(SyntaxDef *syntax, const char *name) {
    if (!syntax || !syntax->import_strip_extensions) return g_strdup(name);
    if (!syntax->import_extensions) return g_strdup(name);
    for (guint i = 0u; i < syntax->import_extensions->len; i++) {
        const char *ext = g_ptr_array_index(syntax->import_extensions, i);
        if (ext && g_str_has_suffix(name, ext)) {
            return g_strndup(name, strlen(name) - strlen(ext));
        }
    }
    return g_strdup(name);
}

/**
 * @brief Module fragment.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param syntax The syntax definition used by the editor path.
 * @param module The module supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *module_fragment(SyntaxDef *syntax, const char *module) {
    if (!module) return g_strdup("");
    char *out = g_strdup(module);
    if (syntax && syntax->import_dot_modules) {
        for (char *p = out; *p; p++) if (*p == '.') *p = G_DIR_SEPARATOR;
    }
    return out;
}

/**
 * @brief Scan dir.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param out Output storage filled when the lookup succeeds.
 * @param seen The seen supplied by the caller.
 * @param syntax The syntax definition used by the editor path.
 * @param dir The dir supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void scan_dir(GPtrArray *out, GHashTable *seen, SyntaxDef *syntax,
                     const char *dir, const char *prefix,
                     guint max_results) {
    GDir *gdir = g_dir_open(dir, 0, NULL);
    if (!gdir) return;
    const char *name = NULL;
    guint count = 0u;
    while ((name = g_dir_read_name(gdir)) != NULL &&
           out->len < max_results && count < GRAPTOS_IMPORT_MAX_DIR_ENTRIES) {
        count++;
        if (name[0] == '.' && name[1] != '\0') continue;
        if (strcmp(name, "node_modules") == 0 || strcmp(name, ".git") == 0 ||
            strcmp(name, "build") == 0 || strcmp(name, "dist") == 0 ||
            strcmp(name, "target") == 0 || strcmp(name, "__pycache__") == 0) {
            continue;
        }
        char *path = g_build_filename(dir, name, NULL);
        if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
            char *candidate = g_strdup_printf("%s/", name);
            add_candidate(out, seen, candidate, prefix, max_results);
            g_free(candidate);
        } else if (g_file_test(path, G_FILE_TEST_IS_REGULAR) &&
                   suffix_allowed(syntax, name)) {
            char *candidate = strip_suffix(syntax, name);
            add_candidate(out, seen, candidate, prefix, max_results);
            g_free(candidate);
        }
        g_free(path);
    }
    g_dir_close(gdir);
}

/**
 * @brief Collect module candidates.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param ctx The ctx supplied by the caller.
 * @param out Output storage filled when the lookup succeeds.
 * @param seen The seen supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void collect_module_candidates(EditorTab *tab, ImportParse *ctx,
                                      GPtrArray *out, GHashTable *seen,
                                      guint max_results) {
    SyntaxDef *syntax = tab ? tab->active_syntax : NULL;
    GPtrArray *roots = g_ptr_array_new_with_free_func(g_free);
    collect_roots(tab, roots);
    char *sub = module_fragment(syntax, ctx->dir_part ? ctx->dir_part : "");
    const char *prefix = ctx->base_part ? ctx->base_part : "";
    for (guint i = 0u; roots && i < roots->len && out->len < max_results; i++) {
        char *dir = g_build_filename(g_ptr_array_index(roots, i), sub, NULL);
        scan_dir(out, seen, syntax, dir, prefix, max_results);
        g_free(dir);
    }
    g_free(sub);
    g_ptr_array_free(roots, TRUE);
}

/**
 * @brief Add identifier.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param out Output storage filled when the lookup succeeds.
 * @param seen The seen supplied by the caller.
 * @param word The symbol text being matched.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void add_identifier(GPtrArray *out, GHashTable *seen, const char *word,
                           const char *prefix, guint max_results) {
    if (!word) return;
    if (!g_ascii_isalpha((guchar)word[0]) && word[0] != '_') return;
    add_candidate(out, seen, word, prefix, max_results);
}

/**
 * @brief Scan quoted identifier list.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param out Output storage filled when the lookup succeeds.
 * @param seen The seen supplied by the caller.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void scan_quoted_identifier_list(GPtrArray *out, GHashTable *seen,
                                        const char *line, const char *prefix,
                                        guint max_results) {
    const char *p = line;
    while (p && *p && out->len < max_results) {
        while (*p && *p != '\'' && *p != '"') p++;
        if (!*p) break;
        char quote = *p++;
        const char *start = p;
        while (*p && *p != quote) p++;
        if (*p == quote && p > start) {
            char *word = g_strndup(start, (gsize)(p - start));
            if (word && word[0] &&
                (g_ascii_isalpha((guchar)word[0]) || word[0] == '_')) {
                add_candidate(out, seen, word, prefix, max_results);
            }
            g_free(word);
            p++;
        }
    }
}

/**
 * @brief Scan text symbols.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param out Output storage filled when the lookup succeeds.
 * @param seen The seen supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void scan_text_symbols(GPtrArray *out, GHashTable *seen,
                              const char *text, const char *prefix,
                              guint max_results) {
    static const char *marks[] = {
        "def ", "class ", "function ", "const ", "let ", "var ",
        "fn ", "struct ", "enum ", "trait ",
        "type ", "interface ", "func ", "#define ", "typedef ", NULL
    };
    const char *p = text;
    while (p && *p && out->len < max_results) {
        const char *line_end = strchr(p, '\n');
        gsize line_len = line_end ? (gsize)(line_end - p) : strlen(p);
        char *line = g_strndup(p, line_len);
        if (!line) break;

        if (strstr(line, "__all__") || strstr(line, "export") ||
            strstr(line, "exports.")) {
            scan_quoted_identifier_list(out, seen, line, prefix,
                                        max_results);
        }

        for (guint i = 0u; marks[i]; i++) {
            gsize len = strlen(marks[i]);
            char *hit = g_strstr_len(line, -1, marks[i]);
            if (!hit) continue;
            const char *s = hit + len;
            while (*s && !g_ascii_isalpha((guchar)*s) && *s != '_') s++;
            const char *e = s;
            while (*e && (g_ascii_isalnum((guchar)*e) || *e == '_')) e++;
            if (e > s) {
                char *word = g_strndup(s, (gsize)(e - s));
                add_identifier(out, seen, word, prefix, max_results);
                g_free(word);
            }
        }
        g_free(line);
        if (!line_end) break;
        p = line_end + 1;
    }
}

/**
 * @brief Scan symbol file.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param out Output storage filled when the lookup succeeds.
 * @param seen The seen supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void scan_symbol_file(GPtrArray *out, GHashTable *seen,
                             const char *path, const char *prefix,
                             guint max_results) {
    char *text = NULL;
    gsize len = 0u;
    if (!g_file_get_contents(path, &text, &len, NULL)) return;
    if (len <= GRAPTOS_IMPORT_MAX_FILE_BYTES) {
        scan_text_symbols(out, seen, text, prefix, max_results);
    }
    g_free(text);
}

/**
 * @brief Collect static members.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param syntax The syntax definition used by the editor path.
 * @param out Output storage filled when the lookup succeeds.
 * @param seen The seen supplied by the caller.
 * @param module The module supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void collect_static_members(SyntaxDef *syntax, GPtrArray *out,
                                   GHashTable *seen, const char *module,
                                   const char *prefix, guint max_results) {
    if (!syntax || !syntax->import_static_modules || !module) return;
    for (guint i = 0u; i < syntax->import_static_modules->len; i++) {
        const char *row = g_ptr_array_index(syntax->import_static_modules, i);
        const char *eq = row ? strchr(row, '=') : NULL;
        if (!eq) continue;
        char *name = g_strndup(row, (gsize)(eq - row));
        g_strstrip(name);
        if (strcmp(name, module) == 0) {
            char **members = g_strsplit(eq + 1, "|", -1);
            for (guint j = 0u; members && members[j] && out->len < max_results; j++) {
                char *member = g_strstrip(members[j]);
                add_candidate(out, seen, member, prefix, max_results);
            }
            g_strfreev(members);
        }
        g_free(name);
    }
}

/**
 * @brief Collect member candidates.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param ctx The ctx supplied by the caller.
 * @param out Output storage filled when the lookup succeeds.
 * @param seen The seen supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void collect_member_candidates(EditorTab *tab, ImportParse *ctx,
                                      GPtrArray *out, GHashTable *seen,
                                      guint max_results) {
    SyntaxDef *syntax = tab ? tab->active_syntax : NULL;
    const char *prefix = ctx->base_part ? ctx->base_part : "";
    collect_static_members(syntax, out, seen, ctx->module, prefix, max_results);

    GPtrArray *roots = g_ptr_array_new_with_free_func(g_free);
    collect_roots(tab, roots);
    char *module_path = module_fragment(syntax, ctx->module);
    for (guint i = 0u; roots && i < roots->len && out->len < max_results; i++) {
        const char *root = g_ptr_array_index(roots, i);
        char *base = g_build_filename(root, module_path, NULL);
        if (g_file_test(base, G_FILE_TEST_IS_DIR)) {
            if (syntax && syntax->import_member_files &&
                syntax->import_member_files->len > 0u) {
                for (guint f = 0u; f < syntax->import_member_files->len; f++) {
                    char *p = g_build_filename(base,
                        g_ptr_array_index(syntax->import_member_files, f), NULL);
                    scan_symbol_file(out, seen, p, prefix, max_results);
                    g_free(p);
                }
            }
            scan_dir(out, seen, syntax, base, prefix, max_results);
        } else {
            if (syntax && syntax->import_extensions) {
                for (guint e = 0u; e < syntax->import_extensions->len; e++) {
                    char *p = g_strconcat(base,
                        (char *)g_ptr_array_index(syntax->import_extensions, e), NULL);
                    scan_symbol_file(out, seen, p, prefix, max_results);
                    g_free(p);
                }
            }
            scan_symbol_file(out, seen, base, prefix, max_results);
        }
        g_free(base);
    }
    g_free(module_path);
    g_ptr_array_free(roots, TRUE);
}

/**
 * @brief Import collect candidates.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param ctx The ctx supplied by the caller.
 * @param out Output storage filled when the lookup succeeds.
 * @param max_results The max results supplied by the caller.
 */
void import_collect_candidates(EditorTab *tab,
                               ImportParse *ctx,
                               GPtrArray *out,
                               guint max_results) {
    if (!tab || !ctx || !out) return;
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             g_free, NULL);
    if (!seen) return;
    if (ctx->kind == IMPORT_CTX_MEMBERS) {
        collect_member_candidates(tab, ctx, out, seen, max_results);
    } else {
        collect_module_candidates(tab, ctx, out, seen, max_results);
    }
    g_hash_table_destroy(seen);
}
