static char *derive_name(const char *name, char separator, gboolean uppercase) {
    GString *out = g_string_new(NULL);
    gboolean last_sep = TRUE;
    for (const char *p = name ? name : "project"; *p; p++) {
        if (g_ascii_isalnum(*p)) {
            g_string_append_c(out, uppercase ? g_ascii_toupper(*p) : g_ascii_tolower(*p));
            last_sep = FALSE;
        } else if (!last_sep) {
            g_string_append_c(out, separator);
            last_sep = TRUE;
        }
    }
    while (out->len > 0u && out->str[out->len - 1u] == separator) {
        g_string_truncate(out, out->len - 1u);
    }
    if (out->len == 0u) g_string_append(out, "project");
    return g_string_free(out, FALSE);
}

/**
 * @brief Add or replace a variable in the resolved map.
 * @details All variable values are owned by the map, including built-ins and
 *          dialog overrides, so later resolution can mutate safely.
 * @param vars The variable map.
 * @param key The variable name.
 * @param value The variable value.
 */
static void vars_set(GHashTable *vars, const char *key, const char *value) {
    g_hash_table_replace(vars, g_strdup(key), g_strdup(value ? value : ""));
}

/**
 * @brief Copy user variable overrides into a plan variable map.
 * @details Overrides are applied after built-ins and manifest defaults, matching
 *          the dialog behavior where the user has final say.
 * @param vars The destination variable map.
 * @param user_variables Optional override map.
 */
static void vars_copy_user(GHashTable *vars, GHashTable *user_variables) {
    if (!user_variables) return;
    GHashTableIter iter;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iter, user_variables);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        vars_set(vars, key, value);
    }
}

/**
 * @brief Resolve placeholders inside variable values.
 * @details Text defaults may refer to built-in variables such as
 *          `${project_slug}`. Resolving the map once before planning keeps
 *          template defaults useful without adding a filter language.
 * @param vars The variable map to update in place.
 * @param error Return location for render errors.
 * @return TRUE when every variable value resolved.
 */
static gboolean variables_resolve(GHashTable *vars, GError **error) {
    GPtrArray *keys = g_ptr_array_new_with_free_func(g_free);
    GHashTableIter iter;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iter, vars);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        g_ptr_array_add(keys, g_strdup(key));
    }
    for (guint i = 0u; i < keys->len; i++) {
        const char *name = g_ptr_array_index(keys, i);
        const char *raw = g_hash_table_lookup(vars, name);
        g_autofree char *rendered = graptos_project_render_string(raw, vars, error);
        if (!rendered) {
            g_ptr_array_free(keys, TRUE);
            return FALSE;
        }
        vars_set(vars, name, rendered);
    }
    g_ptr_array_free(keys, TRUE);
    return TRUE;
}

/**
 * @brief Build the default variable map for a project.
 * @param t The source template.
 * @param project_name The user-facing project name.
 * @param destination The final project directory.
 * @param user_variables Optional user/template variable overrides.
 * @return A newly allocated variable map.
 */
static GHashTable *variables_build(GraptosProjectTemplate *t,
                                   const char *project_name,
                                   const char *destination,
                                   GHashTable *user_variables) {
    GHashTable *vars = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_autofree char *slug = derive_name(project_name, '-', FALSE);
    g_autofree char *ident = derive_name(project_name, '_', FALSE);
    g_autofree char *macro = derive_name(project_name, '_', TRUE);
    GDateTime *now = g_date_time_new_now_local();
    g_autofree char *year = g_strdup_printf("%d", now ? g_date_time_get_year(now) : 2026);
    if (now) g_date_time_unref(now);

    vars_set(vars, "project_name", project_name);
    vars_set(vars, "project_slug", slug);
    vars_set(vars, "project_ident", ident);
    vars_set(vars, "project_macro", macro);
    vars_set(vars, "project_dir", destination);
    vars_set(vars, "author", g_get_real_name() ? g_get_real_name() : g_get_user_name());
    vars_set(vars, "email", "");
    vars_set(vars, "year", year);
    vars_set(vars, "license", "AGPL-3.0");
    for (guint i = 0u; i < t->variables->len; i++) {
        GraptosProjectVariable *var = g_ptr_array_index(t->variables, i);
        vars_set(vars, var->name, var->default_value);
    }
    vars_copy_user(vars, user_variables);
    return vars;
}

/**
 * @brief Evaluate a v1 condition.
 * @details Conditions are deliberately not expressions. The accepted grammar is
 *          small enough to audit and gives clear errors when a template author
 *          reaches for unsupported logic.
 * @param condition The condition text, or NULL for unconditional.
 * @param variables The resolved variable map.
 * @param error Return location for validation errors.
 * @return TRUE when the entry should be included.
 */
gboolean graptos_project_condition_eval(const char *condition,
                                        GHashTable *variables,
                                        GError **error) {
    if (!condition || condition[0] == '\0') return TRUE;
    g_autofree char *copy = scalar_dup(condition);
    char *s = g_strstrip(copy);
    gboolean negate = FALSE;
    if (g_str_has_prefix(s, "not ")) {
        negate = TRUE;
        s = g_strstrip(s + 4);
    }
    char *op = strstr(s, "==");
    gboolean not_equal = FALSE;
    if (!op) {
        op = strstr(s, "!=");
        not_equal = TRUE;
    }
    if (op) {
        g_autofree char *name = g_strndup(s, (gsize)(op - s));
        g_autofree char *expected = scalar_dup(op + 2);
        g_strstrip(name);
        const char *actual = g_hash_table_lookup(variables, name);
        if (!actual) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_UNKNOWN_VARIABLE,
                        "Condition references unknown variable: %s", name);
            return FALSE;
        }
        gboolean equal = g_strcmp0(actual, expected) == 0;
        return not_equal ? !equal : equal;
    }
    if (!valid_variable_name(s)) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                    "Unsupported condition: %s", condition);
        return FALSE;
    }
    const char *actual = g_hash_table_lookup(variables, s);
    if (!actual) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_UNKNOWN_VARIABLE,
                    "Condition references unknown variable: %s", s);
        return FALSE;
    }
    gboolean truth = g_ascii_strcasecmp(actual, "true") == 0 ||
                     g_ascii_strcasecmp(actual, "yes") == 0 ||
                     g_strcmp0(actual, "1") == 0;
    return negate ? !truth : truth;
}

/**
 * @brief Render placeholders in a string.
 * @details Unknown placeholders are rejected before generation. That keeps a
 *          misspelled template variable from producing broken output silently.
 * @param text The template text.
 * @param variables The resolved variable map.
 * @param error Return location for render errors.
 * @return An owned rendered string, or NULL on failure.
 */
char *graptos_project_render_string(const char *text,
                                    GHashTable *variables,
                                    GError **error) {
    GString *out = g_string_new(NULL);
    const char *p = text ? text : "";
    while (*p) {
        if (p[0] == '$' && p[1] == '{') {
            const char *end = strchr(p + 2, '}');
            if (!end) {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_RENDER,
                            "Unclosed placeholder in template text");
                g_string_free(out, TRUE);
                return NULL;
            }
            g_autofree char *name = g_strndup(p + 2, (gsize)(end - (p + 2)));
            const char *value = g_hash_table_lookup(variables, name);
            if (!value) {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_UNKNOWN_VARIABLE,
                            "Unknown variable: ${%s}", name);
                g_string_free(out, TRUE);
                return NULL;
            }
            g_string_append(out, value);
            p = end + 1;
            continue;
        }
        g_string_append_c(out, *p++);
    }
    return g_string_free(out, FALSE);
}

/**
 * @brief Validate and normalize a project-relative path.
 * @details All template-controlled paths must stay below the generated root.
 *          The check rejects absolute paths, Windows absolute syntax, empty
 *          paths, and traversal components before any filesystem write happens.
 * @param path The path to validate.
 * @param normalized_out Optional output storage for the normalized path.
 * @param error Return location for validation errors.
 * @return TRUE when the path is safe.
 */
gboolean graptos_project_path_is_safe(const char *path,
                                      char **normalized_out,
                                      GError **error) {
    if (normalized_out) *normalized_out = NULL;
    if (!path || path[0] == '\0' || g_path_is_absolute(path) ||
        strchr(path, '\\') || strstr(path, ":")) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_INVALID_PATH,
                    "Invalid project path: %s", path ? path : "(empty)");
        return FALSE;
    }
    GPtrArray *parts = g_ptr_array_new_with_free_func(g_free);
    char **tokens = g_strsplit(path, "/", -1);
    for (guint i = 0u; tokens && tokens[i]; i++) {
        if (tokens[i][0] == '\0' || g_strcmp0(tokens[i], ".") == 0) continue;
        if (g_strcmp0(tokens[i], "..") == 0) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_INVALID_PATH,
                        "Path escapes project root: %s", path);
            g_strfreev(tokens);
            g_ptr_array_free(parts, TRUE);
            return FALSE;
        }
        g_ptr_array_add(parts, g_strdup(tokens[i]));
    }
    g_strfreev(tokens);
    if (parts->len == 0u) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_INVALID_PATH,
                    "Invalid empty project path");
        g_ptr_array_free(parts, TRUE);
        return FALSE;
    }
    GString *norm = g_string_new(NULL);
    for (guint i = 0u; i < parts->len; i++) {
        if (i > 0u) g_string_append_c(norm, G_DIR_SEPARATOR);
        g_string_append(norm, g_ptr_array_index(parts, i));
    }
    g_ptr_array_free(parts, TRUE);
    if (normalized_out) *normalized_out = g_string_free(norm, FALSE);
    else g_string_free(norm, TRUE);
    return TRUE;
}

/**
 * @brief Add a resolved output path to duplicate tracking.
 * @details Duplicate checks happen after placeholder rendering because two
 *          different manifest paths can collapse to the same final filename.
 * @param seen The hash table of previously planned paths.
 * @param path The path to add.
 * @param error Return location for duplicate errors.
 * @return TRUE when the path was not already present.
 */
static gboolean seen_add(GHashTable *seen, const char *path, GError **error) {
    if (g_hash_table_contains(seen, path)) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_DUPLICATE_PATH,
                    "Duplicate output path: %s", path);
        return FALSE;
    }
    g_hash_table_add(seen, g_strdup(path));
    return TRUE;
}

/**
 * @brief Build a resolved plan from a template.
 * @details Planning is where conditions, variables, path rendering, duplicate
 *          checks, and file rendering converge. If planning succeeds, preview
 *          and apply can use the plan without needing the source manifest.
 * @param t The parsed template.
 * @param project_name The project name entered by the user.
 * @param destination_parent The parent directory selected by the user.
 * @param user_variables Optional user variable overrides.
 * @param error Return location for planning errors.
 * @return A resolved plan, or NULL on failure.
 */
GraptosProjectPlan *graptos_project_plan_build(GraptosProjectTemplate *t,
                                               const char *project_name,
                                               const char *destination_parent,
                                               GHashTable *user_variables,
                                               GError **error) {
    if (!t || !project_name || project_name[0] == '\0' ||
        !destination_parent || destination_parent[0] == '\0') {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                    "Project name and destination are required");
        return NULL;
    }
    if (!template_validate(t, error)) return NULL;
    g_autofree char *slug = derive_name(project_name, '-', FALSE);
    g_autofree char *destination = g_build_filename(destination_parent, slug, NULL);

    GraptosProjectPlan *plan = g_new0(GraptosProjectPlan, 1);
    plan->destination = g_canonicalize_filename(destination, NULL);
    plan->variables = variables_build(t, project_name, plan->destination, user_variables);
    if (!variables_resolve(plan->variables, error)) {
        graptos_project_plan_free(plan);
        return NULL;
    }
    plan->directories = g_ptr_array_new_with_free_func(planned_directory_free);
    plan->files = g_ptr_array_new_with_free_func(planned_file_free);
    plan->actions = g_ptr_array_new_with_free_func(planned_action_free);
    g_autoptr(GHashTable) seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    for (guint i = 0u; i < t->directories->len; i++) {
        GraptosProjectDirectory *dir = g_ptr_array_index(t->directories, i);
        if (!graptos_project_condition_eval(dir->condition, plan->variables, error)) continue;
        g_autofree char *rendered = graptos_project_render_string(dir->path, plan->variables, error);
        if (!rendered) goto fail;
        g_autofree char *safe = NULL;
        if (!graptos_project_path_is_safe(rendered, &safe, error)) goto fail;
        GraptosProjectPlannedDirectory *pd = g_new0(GraptosProjectPlannedDirectory, 1);
        pd->path = g_strdup(safe);
        g_ptr_array_add(plan->directories, pd);
    }

    for (guint i = 0u; i < t->files->len; i++) {
        GraptosProjectFile *file = g_ptr_array_index(t->files, i);
        if (!graptos_project_condition_eval(file->condition, plan->variables, error)) continue;
        g_autofree char *rendered_path = graptos_project_render_string(file->path, plan->variables, error);
        if (!rendered_path) goto fail;
        g_autofree char *safe_path = NULL;
        if (!graptos_project_path_is_safe(rendered_path, &safe_path, error)) goto fail;
        if (!seen_add(seen, safe_path, error)) goto fail;

        const char *src_rel = file->template_path ? file->template_path : file->source_path;
        g_autofree char *safe_src = NULL;
        if (!graptos_project_path_is_safe(src_rel, &safe_src, error)) goto fail;
        g_autofree char *src_path = g_build_filename(t->base_path, safe_src, NULL);
        g_autofree char *content = NULL;
        gsize len = 0u;
        if (!g_file_get_contents(src_path, &content, &len, error)) goto fail;
        if (len > GRAPTOS_PROJECT_INIT_MAX_FILE_BYTES) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_IO,
                        "Template source is too large: %s", src_rel);
            goto fail;
        }
        GraptosProjectPlannedFile *pf = g_new0(GraptosProjectPlannedFile, 1);
        pf->path = g_strdup(safe_path);
        if (file->render) {
            g_autofree char *rendered_content = graptos_project_render_string(content, plan->variables, error);
            if (!rendered_content) {
                planned_file_free(pf);
                goto fail;
            }
            pf->content = g_bytes_new(rendered_content, strlen(rendered_content));
        } else {
            pf->content = g_bytes_new(content, len);
        }
        g_ptr_array_add(plan->files, pf);
    }

    for (guint i = 0u; i < t->actions->len; i++) {
        GraptosProjectAction *action = g_ptr_array_index(t->actions, i);
        if (!graptos_project_condition_eval(action->condition, plan->variables, error)) continue;
        GraptosProjectPlannedAction *pa = g_new0(GraptosProjectPlannedAction, 1);
        pa->type = g_strdup(action->type);
        if (action->path) {
            g_autofree char *rendered = graptos_project_render_string(action->path, plan->variables, error);
            g_autofree char *safe = NULL;
            if (!rendered || !graptos_project_path_is_safe(rendered, &safe, error)) {
                planned_action_free(pa);
                goto fail;
            }
            pa->path = g_strdup(safe);
        }
        g_ptr_array_add(plan->actions, pa);
    }

    if (t->open_project) {
        g_autofree char *rendered = graptos_project_render_string(t->open_project, plan->variables, error);
        if (rendered && g_strcmp0(rendered, ".") == 0) plan->open_project = g_strdup(".");
        else if (rendered) {
            if (!graptos_project_path_is_safe(rendered, &plan->open_project, error)) goto fail;
        } else goto fail;
    }
    if (t->open_file) {
        g_autofree char *rendered = graptos_project_render_string(t->open_file, plan->variables, error);
        if (!rendered || !graptos_project_path_is_safe(rendered, &plan->open_file, error)) goto fail;
    }
    return plan;

fail:
    graptos_project_plan_free(plan);
    return NULL;
}

/**
 * @brief Build a human-readable preview for a plan.
 * @details The preview is text so tests and the GTK dialog can share the same
 *          representation before a richer tree widget is needed.
 * @param plan The resolved plan.
 * @return An owned preview string.
 */
char *graptos_project_plan_preview_text(GraptosProjectPlan *plan) {
    if (!plan) return g_strdup("");
    GString *out = g_string_new(NULL);
    g_string_append_printf(out, "Destination:\n%s\n\nDirectories:\n", plan->destination);
    for (guint i = 0u; i < plan->directories->len; i++) {
        GraptosProjectPlannedDirectory *dir = g_ptr_array_index(plan->directories, i);
        g_string_append_printf(out, "  %s/\n", dir->path);
    }
    g_string_append(out, "\nFiles:\n");
    for (guint i = 0u; i < plan->files->len; i++) {
        GraptosProjectPlannedFile *file = g_ptr_array_index(plan->files, i);
        g_string_append_printf(out, "  %s%s\n", file->path, file->executable ? " [executable]" : "");
    }
    g_string_append(out, "\nActions:\n");
    if (plan->actions->len == 0u) g_string_append(out, "  none\n");
    for (guint i = 0u; i < plan->actions->len; i++) {
        GraptosProjectPlannedAction *action = g_ptr_array_index(plan->actions, i);
        g_string_append_printf(out, "  %s%s%s\n", action->type,
                               action->path ? ": " : "",
                               action->path ? action->path : "");
    }
    if (plan->open_project || plan->open_file) {
        g_string_append(out, "\nOpen:\n");
        if (plan->open_project) g_string_append_printf(out, "  project: %s\n", plan->open_project);
        if (plan->open_file) g_string_append_printf(out, "  file: %s\n", plan->open_file);
    }
    return g_string_free(out, FALSE);
}

/**
 * @brief Remove a directory tree.
 * @details Temporary generation directories must be cleaned on failure. This
 *          helper only runs on paths Graptoς created for the apply operation.
 * @param path The directory or file path to remove.
 */
