static void remove_tree(const char *path) {
    if (!path) return;
    if (g_file_test(path, G_FILE_TEST_IS_DIR) && !g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
        GDir *dir = g_dir_open(path, 0, NULL);
        if (dir) {
            const char *name = NULL;
            while ((name = g_dir_read_name(dir))) {
                g_autofree char *child = g_build_filename(path, name, NULL);
                remove_tree(child);
            }
            g_dir_close(dir);
        }
        (void)g_rmdir(path);
    } else {
        (void)g_unlink(path);
    }
}

/**
 * @brief Run a safe built-in action.
 * @param root The temporary project root.
 * @param action The action to run.
 * @param error Return location for action failures.
 * @return TRUE when the action succeeded.
 */
static gboolean apply_action(const char *root,
                             GraptosProjectPlannedAction *action,
                             GError **error) {
    if (g_strcmp0(action->type, "git-init") == 0) {
        const char *argv[] = { "git", "init", NULL };
        g_autoptr(GSubprocessLauncher) launcher =
            g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
                                      G_SUBPROCESS_FLAGS_STDERR_PIPE);
        g_subprocess_launcher_set_cwd(launcher, root);
        g_autoptr(GSubprocess) proc =
            g_subprocess_launcher_spawnv(launcher, argv, error);
        if (!proc) return FALSE;
        g_autofree char *stderr_text = NULL;
        if (!g_subprocess_communicate_utf8(proc, NULL, NULL, NULL, &stderr_text, error)) {
            return FALSE;
        }
        if (!g_subprocess_get_successful(proc)) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_ACTION_FAILED,
                        "git init failed: %s",
                        stderr_text && stderr_text[0] ? stderr_text : "unknown error");
            return FALSE;
        }
        return TRUE;
    }
    if (g_strcmp0(action->type, "mark-executable") == 0) {
        g_autofree char *path = g_build_filename(root, action->path, NULL);
        if (g_chmod(path, 0755) != 0) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_ACTION_FAILED,
                        "Could not mark executable: %s", path);
            return FALSE;
        }
        return TRUE;
    }
    g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                GRAPTOS_PROJECT_INIT_ERROR_UNKNOWN_ACTION,
                "Unknown action: %s", action->type);
    return FALSE;
}

/**
 * @brief Apply a resolved plan atomically.
 * @details All operations happen in a sibling temporary directory first. Only a
 *          complete successful tree is renamed to the final destination.
 * @param plan The resolved plan to apply.
 * @param error Return location for apply errors.
 * @return TRUE when the project was created.
 */
gboolean graptos_project_plan_apply(GraptosProjectPlan *plan, GError **error) {
    if (!plan || !plan->destination) return FALSE;
    if (g_file_test(plan->destination, G_FILE_TEST_EXISTS)) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_DESTINATION_EXISTS,
                    "The destination already exists: %s", plan->destination);
        return FALSE;
    }
    g_autofree char *parent = g_path_get_dirname(plan->destination);
    g_autofree char *base = g_path_get_basename(plan->destination);
    g_autofree char *tmpl = g_strdup_printf(".graptos-init-%s-XXXXXX", base);
    g_autofree char *tmp_path = g_build_filename(parent, tmpl, NULL);
    if (!g_mkdtemp(tmp_path)) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_IO,
                    "Could not create temporary project directory: %s",
                    g_strerror(errno));
        return FALSE;
    }

    for (guint i = 0u; i < plan->directories->len; i++) {
        GraptosProjectPlannedDirectory *dir = g_ptr_array_index(plan->directories, i);
        g_autofree char *path = g_build_filename(tmp_path, dir->path, NULL);
        if (g_mkdir_with_parents(path, 0755) != 0) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_IO,
                        "Could not create directory %s: %s", dir->path, g_strerror(errno));
            remove_tree(tmp_path);
            return FALSE;
        }
    }
    for (guint i = 0u; i < plan->files->len; i++) {
        GraptosProjectPlannedFile *file = g_ptr_array_index(plan->files, i);
        g_autofree char *path = g_build_filename(tmp_path, file->path, NULL);
        g_autofree char *dir = g_path_get_dirname(path);
        if (g_mkdir_with_parents(dir, 0755) != 0) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_IO,
                        "Could not create parent directory for %s", file->path);
            remove_tree(tmp_path);
            return FALSE;
        }
        gsize size = 0u;
        const char *bytes = g_bytes_get_data(file->content, &size);
        if (!g_file_set_contents(path, bytes, (gssize)size, error)) {
            remove_tree(tmp_path);
            return FALSE;
        }
        if (file->executable && g_chmod(path, 0755) != 0) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_IO,
                        "Could not set executable permissions on %s", file->path);
            remove_tree(tmp_path);
            return FALSE;
        }
    }
    for (guint i = 0u; i < plan->actions->len; i++) {
        GraptosProjectPlannedAction *action = g_ptr_array_index(plan->actions, i);
        if (!apply_action(tmp_path, action, error)) {
            remove_tree(tmp_path);
            return FALSE;
        }
    }
    if (g_rename(tmp_path, plan->destination) != 0) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_IO,
                    "Could not move project into place: %s", g_strerror(errno));
        remove_tree(tmp_path);
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Add templates from one directory to a registry.
 * @param templates The template output array.
 * @param by_id Hash table used for source precedence.
 * @param root The directory containing template subdirectories.
 * @param source The source assigned to discovered templates.
 */
static void discover_from_root(GPtrArray *templates,
                               GHashTable *by_id,
                               const char *root,
                               GraptosProjectTemplateSource source) {
    GDir *dir = g_dir_open(root, 0, NULL);
    if (!dir) return;
    const char *name = NULL;
    while ((name = g_dir_read_name(dir))) {
        g_autofree char *template_dir = g_build_filename(root, name, NULL);
        if (!g_file_test(template_dir, G_FILE_TEST_IS_DIR)) continue;
        g_autoptr(GError) error = NULL;
        GraptosProjectTemplate *t = graptos_project_template_load(template_dir, source, &error);
        if (!t) {
            g_warning("Project template skipped: %s", error ? error->message : template_dir);
            continue;
        }
        if (!template_validate(t, &error)) {
            g_warning("Project template invalid: %s", error ? error->message : t->id);
            graptos_project_template_free(t);
            continue;
        }
        gpointer old = g_hash_table_lookup(by_id, t->id);
        if (old) {
            g_warning("Project template %s replaced by higher precedence source", t->id);
            g_ptr_array_remove(templates, old);
        }
        g_hash_table_replace(by_id, g_strdup(t->id), t);
        g_ptr_array_add(templates, t);
    }
    g_dir_close(dir);
}

/**
 * @brief Discover built-in and user templates.
 * @details Built-ins are loaded first and user templates second, so user copies
 *          with the same id override the shipped template without merging.
 * @param error Currently unused; reserved for fatal discovery errors.
 * @return An array of GraptosProjectTemplate values.
 */
GPtrArray *graptos_project_init_discover_templates(GError **error) {
    (void)error;
    GPtrArray *templates = g_ptr_array_new_with_free_func((GDestroyNotify)graptos_project_template_free);
    g_autoptr(GHashTable) by_id = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    g_autofree char *builtin = g_build_filename(DATADIR, "project-templates", NULL);
    g_autofree char *user = g_build_filename(g_get_user_config_dir(), "graptos", "project-templates", NULL);
    discover_from_root(templates, by_id, builtin, GRAPTOS_PROJECT_TEMPLATE_BUILTIN);
    discover_from_root(templates, by_id, "data/project-templates", GRAPTOS_PROJECT_TEMPLATE_BUILTIN);
    discover_from_root(templates, by_id, user, GRAPTOS_PROJECT_TEMPLATE_USER);
    return templates;
}
