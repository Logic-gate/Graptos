/**
 * @file src/git/git_core.inc.c
 * @brief Graptoς git core module.
 * @details Git already knows the repository truth. Graptoς adds UI, status parsing,
 *          credentials, and command wiring, but we avoid building a half-Git inside the
 *          editor.
 * @param result The result supplied by the caller.
 */

static void git_result_clear(GraptosGitResult *result) {
    if (!result) return;
    g_clear_pointer(&result->out, g_free);
    g_clear_pointer(&result->err, g_free);
    g_clear_pointer(&result->message, g_free);
    result->exit_code = 0;
    result->kind = GRAPTOS_GIT_ERROR_NONE;
}

/**
 * @brief Contains ascii.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param haystack The haystack supplied by the caller.
 * @param needle The needle supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean contains_ascii(const char *haystack, const char *needle) {
    if (!haystack || !needle) return FALSE;
    char *h = g_ascii_strdown(haystack, -1);
    char *n = g_ascii_strdown(needle, -1);
    gboolean found = h && n && strstr(h, n) != NULL;
    g_free(h);
    g_free(n);
    return found;
}

/**
 * @brief Classify git error.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param out Output storage filled when the lookup succeeds.
 * @param err The err supplied by the caller.
 * @return The computed value requested by the caller.
 */
static GraptosGitErrorKind classify_git_error(const char *out, const char *err) {
    const char *text = err && err[0] ? err : out;
    if (!text || text[0] == '\0') return GRAPTOS_GIT_ERROR_OTHER;
    if (contains_ascii(text, "not a git repository")) return GRAPTOS_GIT_ERROR_NOT_REPO;
    if (contains_ascii(text, "authentication failed")
        || contains_ascii(text, "invalid username")
        || contains_ascii(text, "invalid password")
        || contains_ascii(text, "permission denied")
        || contains_ascii(text, "could not read username")
        || contains_ascii(text, "could not read password")) {
        return GRAPTOS_GIT_ERROR_AUTH;
    }
    if (contains_ascii(text, "no configured push destination")
        || contains_ascii(text, "does not appear to be a git repository")
        || contains_ascii(text, "no such remote")) {
        return GRAPTOS_GIT_ERROR_NO_REMOTE;
    }
    if (contains_ascii(text, "could not resolve host")
        || contains_ascii(text, "failed to connect")
        || contains_ascii(text, "network is unreachable")
        || contains_ascii(text, "connection timed out")) {
        return GRAPTOS_GIT_ERROR_NETWORK;
    }
    if (contains_ascii(text, "conflict") || contains_ascii(text, "merge conflict")) {
        return GRAPTOS_GIT_ERROR_CONFLICT;
    }
    if (contains_ascii(text, "local changes would be overwritten")
        || contains_ascii(text, "please commit your changes or stash them")) {
        return GRAPTOS_GIT_ERROR_LOCAL_CHANGES;
    }
    if (contains_ascii(text, "non-fast-forward")
        || contains_ascii(text, "fetch first")
        || contains_ascii(text, "not possible to fast-forward")) {
        return GRAPTOS_GIT_ERROR_NON_FAST_FORWARD;
    }
    if (contains_ascii(text, "detached head")) return GRAPTOS_GIT_ERROR_DETACHED_HEAD;
    if (contains_ascii(text, "nothing to commit")
        || contains_ascii(text, "no changes added to commit")) {
        return GRAPTOS_GIT_ERROR_NOTHING_TO_COMMIT;
    }
    return GRAPTOS_GIT_ERROR_OTHER;
}

/**
 * @brief Git error title.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param kind The kind supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *git_error_title(GraptosGitErrorKind kind) {
    switch (kind) {
    case GRAPTOS_GIT_ERROR_NOT_REPO: return "Not a Git repository";
    case GRAPTOS_GIT_ERROR_AUTH: return "Git authentication failed";
    case GRAPTOS_GIT_ERROR_NO_REMOTE: return "Git remote is not configured";
    case GRAPTOS_GIT_ERROR_NETWORK: return "Git network error";
    case GRAPTOS_GIT_ERROR_CONFLICT: return "Git conflict";
    case GRAPTOS_GIT_ERROR_LOCAL_CHANGES: return "Local changes block this Git command";
    case GRAPTOS_GIT_ERROR_NON_FAST_FORWARD: return "Git non-fast-forward error";
    case GRAPTOS_GIT_ERROR_DETACHED_HEAD: return "Git detached HEAD";
    case GRAPTOS_GIT_ERROR_NOTHING_TO_COMMIT: return "Nothing to commit";
    case GRAPTOS_GIT_ERROR_GIT_MISSING: return "Git executable was not found";
    case GRAPTOS_GIT_ERROR_NONE: return "Git command completed";
    case GRAPTOS_GIT_ERROR_OTHER:
    default: return "Git command failed";
    }
}

/**
 * @brief Git error hint.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param kind The kind supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *git_error_hint(GraptosGitErrorKind kind) {
    switch (kind) {
    case GRAPTOS_GIT_ERROR_AUTH:
        return "For GitHub over HTTPS, use a personal access token instead of an account password. GitLab and Gitea may also require a token depending on server policy.";
    case GRAPTOS_GIT_ERROR_NO_REMOTE:
        return "Check `git remote -v` or configure an upstream remote before pushing or pulling.";
    case GRAPTOS_GIT_ERROR_NETWORK:
        return "Check the network connection, DNS, VPN/proxy settings, and remote URL.";
    case GRAPTOS_GIT_ERROR_CONFLICT:
        return "Resolve conflict markers in the working tree, stage the resolved files, then commit.";
    case GRAPTOS_GIT_ERROR_LOCAL_CHANGES:
        return "Commit, stage, discard, or stash local changes before running this command.";
    case GRAPTOS_GIT_ERROR_NON_FAST_FORWARD:
        return "Pull/rebase or fetch and inspect remote changes before pushing.";
    case GRAPTOS_GIT_ERROR_DETACHED_HEAD:
        return "Checkout or create a branch before committing/pushing normal branch work.";
    case GRAPTOS_GIT_ERROR_NOTHING_TO_COMMIT:
        return "Stage files first, or use Status to inspect current changes.";
    case GRAPTOS_GIT_ERROR_GIT_MISSING:
        return "Install Git and make sure the `git` executable is on PATH.";
    default:
        return "Inspect Git output below.";
    }
}
/**
 * @brief Limit output.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param text The text fragment supplied by the caller.
 */
static void limit_output(char **text) {
    if (!text || !*text) return;
    if (strlen(*text) <= GRAPTOS_GIT_MAX_OUTPUT) return;
    (*text)[GRAPTOS_GIT_MAX_OUTPUT] = '\0';
}

/**
 * @brief Run a Git command from an argv array.
 * @details Git commands are launched with argv, never through a shell. That
 *          preserves spaces in paths, avoids quoting bugs, and prevents
 *          user-supplied arguments from becoming shell syntax.
 * @param argv The NULL-terminated Git command argument vector.
 * @param stdin_text Optional stdin text passed to the subprocess.
 * @param result The result object populated with stdout, stderr, and status.
 * @return TRUE when the subprocess was launched and collected; otherwise FALSE.
 *
 * Git commands are launched with argv, never through a shell.  That preserves
 * spaces in paths, avoids quoting bugs, and prevents user-supplied arguments
 * from becoming shell syntax.
 */
static gboolean run_argv(const char * const *argv,
                         const char *stdin_text,
                         GraptosGitResult *result) {
    if (!argv || !argv[0] || !result) return FALSE;
    memset(result, 0, sizeof(*result));

    g_autoptr(GError) error = NULL;
    g_autoptr(GSubprocess) proc = g_subprocess_newv(argv,
        G_SUBPROCESS_FLAGS_STDIN_PIPE
        | G_SUBPROCESS_FLAGS_STDOUT_PIPE
        | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error);
    if (!proc) {
        result->exit_code = 127;
        result->kind = GRAPTOS_GIT_ERROR_GIT_MISSING;
        result->err = g_strdup(error ? error->message : "Could not start git.");
        result->message = g_strdup(git_error_title(result->kind));
        return FALSE;
    }

    g_autofree char *out = NULL;
    g_autofree char *err = NULL;
    gboolean ok = g_subprocess_communicate_utf8(proc,
                                                stdin_text,
                                                NULL,
                                                &out,
                                                &err,
                                                &error);
    result->out = out ? g_steal_pointer(&out) : g_strdup("");
    result->err = err ? g_steal_pointer(&err) : g_strdup("");
    /*
     * Git can emit very large diffs or hook output.  Keep result buffers bounded
     * before they are copied into dialogs/tabs so a failed command cannot make
     * the UI unresponsive.
     */
    limit_output(&result->out);
    limit_output(&result->err);

    if (!ok) {
        result->exit_code = 1;
        result->kind = classify_git_error(result->out, error ? error->message : result->err);
        result->message = g_strdup(error ? error->message : git_error_title(result->kind));
        return FALSE;
    }

    result->exit_code = g_subprocess_get_exit_status(proc);
    if (result->exit_code == 0) {
        result->kind = GRAPTOS_GIT_ERROR_NONE;
        result->message = g_strdup("Git command completed.");
        return TRUE;
    }

    result->kind = classify_git_error(result->out, result->err);
    result->message = g_strdup(git_error_title(result->kind));
    return FALSE;
}

/**
 * @brief Git argv new.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param repo The repo supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GPtrArray *git_argv_new(const char *repo) {
    GPtrArray *argv = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(argv, g_strdup("git"));
    if (repo && repo[0]) {
        g_ptr_array_add(argv, g_strdup("-C"));
        g_ptr_array_add(argv, g_strdup(repo));
    }
    return argv;
}

/**
 * @brief Argv add.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param argv The argv supplied by the caller.
 * @param arg The arg supplied by the caller.
 */
static void argv_add(GPtrArray *argv, const char *arg) {
    if (argv && arg) g_ptr_array_add(argv, g_strdup(arg));
}

/**
 * @brief Run git args.
 * @details Git is still the authority; we only build a controlled argv around
 *          it. Keeping stdin, output capture, and repo `-C` handling in this
 *          helper prevents each Git action from inventing its own process rules.
 * @param repo The repo supplied by the caller.
 * @param stdin_text The stdin text supplied by the caller.
 * @param result The result supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean run_git_args(const char *repo,
                             const char *stdin_text,
                             GraptosGitResult *result,
                             ...) {
    GPtrArray *argv = git_argv_new(repo);
    va_list ap;
    va_start(ap, result);
    const char *arg = NULL;
    while ((arg = va_arg(ap, const char *)) != NULL) argv_add(argv, arg);
    va_end(ap);
    g_ptr_array_add(argv, NULL);
    gboolean ok = run_argv((const char * const *)argv->pdata, stdin_text, result);
    g_ptr_array_free(argv, TRUE);
    return ok;
}

/**
 * @brief Chomp dup.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *chomp_dup(const char *text) {
    char *copy = g_strdup(text ? text : "");
    g_strchomp(copy);
    return copy;
}

/**
 * @brief Dir for path.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *dir_for_path(const char *path) {
    if (!path || path[0] == '\0') return NULL;
    if (g_file_test(path, G_FILE_TEST_IS_DIR)) return g_canonicalize_filename(path, NULL);
    char *dir = g_path_get_dirname(path);
    char *canon = dir ? g_canonicalize_filename(dir, NULL) : NULL;
    g_free(dir);
    return canon;
}

/**
 * @brief Graptoς git repo for path.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_git_repo_for_path(const char *path) {
    char *dir = dir_for_path(path);
    if (!dir) return NULL;
    GraptosGitResult result;
    gboolean ok = run_git_args(dir, NULL, &result,
                               "rev-parse", "--show-toplevel", NULL);
    g_free(dir);
    if (!ok) {
        git_result_clear(&result);
        return NULL;
    }
    char *repo = chomp_dup(result.out);
    git_result_clear(&result);
    if (!repo || repo[0] == '\0') g_clear_pointer(&repo, g_free);
    return repo;
}

/**
 * @brief Relpath for repo.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param repo The repo supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *relpath_for_repo(const char *repo, const char *path) {
    if (!repo || !path) return NULL;
    char *canon = g_canonicalize_filename(path, NULL);
    if (!canon) return NULL;
    gsize len = strlen(repo);
    char *rel = NULL;
    if (g_strcmp0(canon, repo) == 0) {
        rel = g_strdup(".");
    } else if (g_str_has_prefix(canon, repo) && canon[len] == G_DIR_SEPARATOR) {
        rel = g_strdup(canon + len + 1u);
    }
    g_free(canon);
    return rel;
}

/**
 * @brief Current repo.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param win The win supplied by the caller.
 * @param rel_file_out Output storage filled when the operation can provide a value.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *current_repo(EditorWindow *win, char **rel_file_out) {
    if (rel_file_out) *rel_file_out = NULL;
    EditorTab *tab = app_window_current_tab(win);
    if (tab && tab->file_path) {
        char *repo = graptos_git_repo_for_path(tab->file_path);
        if (repo) {
            if (rel_file_out) *rel_file_out = relpath_for_repo(repo, tab->file_path);
            return repo;
        }
    }

    if (win && win->project_root) {
        return graptos_git_repo_for_path(win->project_root);
    }
    if (project_root_count(win) > 0u) {
        return graptos_git_repo_for_path(project_root_at(win, project_root_count(win) - 1u));
    }
    return NULL;
}

/**
 * @brief Marker for status.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param record The record supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *marker_for_status(const char *record) {
    if (!record || strlen(record) < 3u) return "";
    char x = record[0];
    char y = record[1];
    if (x == '?' || y == '?') return "?";
    if (x == 'U' || y == 'U' || (x == 'A' && y == 'A') || (x == 'D' && y == 'D')) return "!";
    if (x == 'R' || y == 'R') return "R";
    if (x == 'D' || y == 'D') return "D";
    if (x == 'A' || y == 'A') return "A";
    if (x != ' ' && x != '\0') return "S";
    if (y == 'M') return "M";
    if (y != ' ' && y != '\0') return "M";
    return "";
}

/**
 * @brief Status map add.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param win The win supplied by the caller.
 * @param repo The repo supplied by the caller.
 * @param record The record supplied by the caller.
 */
static void status_map_add(EditorWindow *win,
                           const char *repo,
                           const char *record) {
    if (!win || !win->git_file_status || !repo || !record) return;
    if (record[0] == '#' && record[1] == '#') return;
    if (strlen(record) < 4u) return;

    const char *rel = record + 3u;
    if (!rel || rel[0] == '\0') return;

    const char *marker = marker_for_status(record);
    if (!marker || marker[0] == '\0') return;

    char *path = g_build_filename(repo, rel, NULL);
    char *canon = g_canonicalize_filename(path, NULL);
    g_hash_table_replace(win->git_file_status,
                         canon ? canon : g_strdup(path),
                         g_strdup(marker));
    g_free(path);
}

/**
 * @brief Refresh repo status.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param win The win supplied by the caller.
 * @param repo The repo supplied by the caller.
 */
static void refresh_repo_status(EditorWindow *win, const char *repo) {
    if (!win || !repo) return;
    GraptosGitResult result;
    gboolean ok = run_git_args(repo, NULL, &result,
                               "status", "--porcelain=v1", "--branch", NULL);
    if (!ok) {
        git_result_clear(&result);
        return;
    }

    char **lines = g_strsplit(result.out ? result.out : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        if (lines[i][0] == '\0') continue;
        status_map_add(win, repo, lines[i]);
    }
    g_strfreev(lines);
    git_result_clear(&result);
}

/**
 * @brief Graptoς git status for file.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
const char *graptos_git_status_for_file(EditorWindow *win, const char *path) {
    if (!win || !win->git_file_status || !path) return NULL;
    char *canon = g_canonicalize_filename(path, NULL);
    const char *status = canon ? g_hash_table_lookup(win->git_file_status, canon) : NULL;
    g_free(canon);
    return status;
}

/**
 * @brief Graptoς git refresh all.
 * @details Project roots can overlap inside the same repository. We remember
 *          repos already refreshed so the file tree gets one coherent status
 *          snapshot instead of repeating the same Git command per root.
 * @param win The win supplied by the caller.
 */
void graptos_git_refresh_all(EditorWindow *win) {
    if (!win) return;
    if (!win->git_file_status) {
        win->git_file_status = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    }
    g_hash_table_remove_all(win->git_file_status);

    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    guint count = project_root_count(win);
    for (guint i = 0u; i < count; i++) {
        const char *root = project_root_at(win, i);
        char *repo = graptos_git_repo_for_path(root);
        if (repo && !g_hash_table_contains(seen, repo)) {
            g_hash_table_add(seen, g_strdup(repo));
            refresh_repo_status(win, repo);
        }
        g_free(repo);
    }
    g_hash_table_destroy(seen);
}

/**
 * @brief Graptoς git refresh and rebuild.
 * @details Git actions are user-facing wrappers around command output. The comment keeps the boundary clear between collecting results and presenting them in Graptoς dialogs.
 * @param win The win supplied by the caller.
 */
void graptos_git_refresh_and_rebuild(EditorWindow *win) {
    graptos_git_refresh_all(win);
    project_tree_refresh(win);
}
