GraptosProjectTemplate *graptos_project_template_load(const char *template_dir,
                                                      GraptosProjectTemplateSource source,
                                                      GError **error) {
    g_autofree char *manifest = g_build_filename(template_dir, "template.yaml", NULL);
    g_autofree char *text = NULL;
    gsize len = 0u;
    if (!g_file_get_contents(manifest, &text, &len, error)) return NULL;

    GraptosProjectTemplate *t = template_new();
    t->source = source;
    t->base_path = g_canonicalize_filename(template_dir, NULL);
    enum { SECTION_TOP, SECTION_VARIABLES, SECTION_DIRECTORIES, SECTION_FILES,
           SECTION_ACTIONS, SECTION_OPEN } section = SECTION_TOP;
    GraptosProjectVariable *current_var = NULL;
    GraptosProjectDirectory *current_dir = NULL;
    GraptosProjectFile *current_file = NULL;
    GraptosProjectAction *current_action = NULL;

    char **lines = g_strsplit(text, "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        char *line = lines[i];
        strip_comment(line);
        g_autofree char *trim_copy = g_strdup(line);
        char *trim = g_strstrip(trim_copy);
        if (trim[0] == '\0') continue;
        guint indent = 0u;
        while (line[indent] == ' ') indent++;

        if (indent == 0u) {
            current_var = NULL;
            current_dir = NULL;
            current_file = NULL;
            current_action = NULL;
            if (g_strcmp0(trim, "variables:") == 0) {
                section = SECTION_VARIABLES;
                continue;
            }
            if (g_strcmp0(trim, "directories:") == 0) {
                section = SECTION_DIRECTORIES;
                continue;
            }
            if (g_strcmp0(trim, "files:") == 0) {
                section = SECTION_FILES;
                continue;
            }
            if (g_strcmp0(trim, "actions:") == 0) {
                section = SECTION_ACTIONS;
                continue;
            }
            if (g_strcmp0(trim, "open:") == 0) {
                section = SECTION_OPEN;
                continue;
            }
            section = SECTION_TOP;
        }

        if (section == SECTION_VARIABLES && indent == 2u && g_str_has_suffix(trim, ":")) {
            g_autofree char *name = g_strndup(trim, strlen(trim) - 1u);
            g_strstrip(name);
            current_var = g_new0(GraptosProjectVariable, 1);
            current_var->name = g_strdup(name);
            current_var->values = g_ptr_array_new_with_free_func(g_free);
            g_ptr_array_add(t->variables, current_var);
            continue;
        }

        if ((section == SECTION_DIRECTORIES || section == SECTION_FILES ||
             section == SECTION_ACTIONS) && indent == 2u &&
            g_str_has_prefix(trim, "-")) {
            char *rest = g_strstrip(trim + 1);
            current_dir = NULL;
            current_file = NULL;
            current_action = NULL;
            if (section == SECTION_DIRECTORIES) {
                current_dir = g_new0(GraptosProjectDirectory, 1);
                g_ptr_array_add(t->directories, current_dir);
                if (rest[0] && !strchr(rest, ':')) current_dir->path = scalar_dup(rest);
            } else if (section == SECTION_FILES) {
                current_file = g_new0(GraptosProjectFile, 1);
                current_file->render = TRUE;
                g_ptr_array_add(t->files, current_file);
            } else {
                current_action = g_new0(GraptosProjectAction, 1);
                g_ptr_array_add(t->actions, current_action);
            }
            if (rest[0] && strchr(rest, ':')) {
                g_autofree char *key = NULL;
                g_autofree char *value = NULL;
                if (!parse_key_value(rest, &key, &value)) {
                    g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                                GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                                "Template error in %s: unsupported or malformed line %u: %s",
                                manifest, i + 1u, trim);
                    goto fail;
                }
                if (current_dir && g_strcmp0(key, "path") == 0) set_string(&current_dir->path, value);
                else if (current_file && g_strcmp0(key, "path") == 0) set_string(&current_file->path, value);
                else if (current_action && g_strcmp0(key, "type") == 0) set_string(&current_action->type, value);
                else {
                    g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                                GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                                "Template error in %s: unsupported or malformed line %u: %s",
                                manifest, i + 1u, trim);
                    goto fail;
                }
            }
            continue;
        }

        g_autofree char *key = NULL;
        g_autofree char *value = NULL;
        if (!parse_key_value(trim, &key, &value)) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                        "Template error in %s: unsupported or malformed line %u: %s",
                        manifest, i + 1u, trim);
            goto fail;
        }

        if (section == SECTION_TOP) {
            if (g_strcmp0(key, "version") == 0) t->version = (guint)g_ascii_strtoull(value, NULL, 10);
            else if (g_strcmp0(key, "id") == 0) set_string(&t->id, value);
            else if (g_strcmp0(key, "name") == 0) set_string(&t->name, value);
            else if (g_strcmp0(key, "description") == 0) set_string(&t->description, value);
            else if (g_strcmp0(key, "language") == 0) set_string(&t->language, value);
            else if (g_strcmp0(key, "icon") == 0 || g_strcmp0(key, "category") == 0) {}
            else {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                            "Template error in %s: unsupported or malformed line %u: %s",
                            manifest, i + 1u, trim);
                goto fail;
            }
        } else if (section == SECTION_VARIABLES && current_var) {
            if (g_strcmp0(key, "label") == 0) set_string(&current_var->label, value);
            else if (g_strcmp0(key, "type") == 0) set_string(&current_var->type, value);
            else if (g_strcmp0(key, "default") == 0) set_string(&current_var->default_value, value);
            else if (g_strcmp0(key, "values") == 0) {
                if (current_var->values) g_ptr_array_free(current_var->values, TRUE);
                current_var->values = parse_inline_list(value, error);
                if (!current_var->values) goto fail;
            } else {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                            "Template error in %s: unsupported or malformed line %u: %s",
                            manifest, i + 1u, trim);
                goto fail;
            }
        } else if (section == SECTION_DIRECTORIES && current_dir) {
            if (g_strcmp0(key, "path") == 0) set_string(&current_dir->path, value);
            else if (g_strcmp0(key, "when") == 0) set_string(&current_dir->condition, value);
            else {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                            "Template error in %s: unsupported or malformed line %u: %s",
                            manifest, i + 1u, trim);
                goto fail;
            }
        } else if (section == SECTION_FILES && current_file) {
            if (g_strcmp0(key, "path") == 0) set_string(&current_file->path, value);
            else if (g_strcmp0(key, "template") == 0) set_string(&current_file->template_path, value);
            else if (g_strcmp0(key, "source") == 0) set_string(&current_file->source_path, value);
            else if (g_strcmp0(key, "when") == 0) set_string(&current_file->condition, value);
            else if (g_strcmp0(key, "render") == 0) current_file->render = g_ascii_strcasecmp(value, "false") != 0;
            else {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                            "Template error in %s: unsupported or malformed line %u: %s",
                            manifest, i + 1u, trim);
                goto fail;
            }
        } else if (section == SECTION_ACTIONS && current_action) {
            if (g_strcmp0(key, "type") == 0) set_string(&current_action->type, value);
            else if (g_strcmp0(key, "path") == 0) set_string(&current_action->path, value);
            else if (g_strcmp0(key, "when") == 0) set_string(&current_action->condition, value);
            else {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                            "Template error in %s: unsupported or malformed line %u: %s",
                            manifest, i + 1u, trim);
                goto fail;
            }
        } else if (section == SECTION_OPEN) {
            if (g_strcmp0(key, "project") == 0) set_string(&t->open_project, value);
            else if (g_strcmp0(key, "file") == 0) set_string(&t->open_file, value);
            else {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                            "Template error in %s: unsupported or malformed line %u: %s",
                            manifest, i + 1u, trim);
                goto fail;
            }
        } else {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                        "Template error in %s: unsupported or malformed line %u: %s",
                        manifest, i + 1u, trim);
            goto fail;
        }
    }
    g_strfreev(lines);
    lines = NULL;

    if (!t->version || t->version != 1u || !t->id || !t->name || !t->language) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                    "Template error in %s: version, id, name, and language are required",
                    manifest);
        goto fail;
    }
    if (!valid_identifier(t->id)) {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                    "Template error in %s: invalid template id %s", manifest, t->id);
        goto fail;
    }
    return t;

fail:
    if (lines) g_strfreev(lines);
    graptos_project_template_free(t);
    return NULL;
}

/**
 * @brief Check whether a pointer array contains a string.
 * @details Choice default validation uses this helper so malformed manifests
 *          fail before the template reaches the UI.
 * @param array The string array to search.
 * @param value The string to find.
 * @return TRUE when the value is present.
 */
static gboolean string_array_contains(GPtrArray *array, const char *value) {
    if (!array || !value) return FALSE;
    for (guint i = 0u; i < array->len; i++) {
        if (g_strcmp0(g_ptr_array_index(array, i), value) == 0) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Validate a parsed template.
 * @details Parser checks shape, while validation checks semantic rules that may
 *          require looking at multiple sections or source files.
 * @param t The template to validate.
 * @param error Return location for validation errors.
 * @return TRUE when the template is valid.
 */
static gboolean template_validate(GraptosProjectTemplate *t, GError **error) {
    g_autoptr(GHashTable) names = g_hash_table_new(g_str_hash, g_str_equal);
    for (guint i = 0u; i < t->variables->len; i++) {
        GraptosProjectVariable *var = g_ptr_array_index(t->variables, i);
        if (!var->name || !valid_variable_name(var->name) ||
            !var->type || !var->default_value) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                        "Template %s variable %u is incomplete or invalid", t->id, i);
            return FALSE;
        }
        if (g_hash_table_contains(names, var->name)) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                        "Template %s duplicates variable %s", t->id, var->name);
            return FALSE;
        }
        g_hash_table_add(names, var->name);
        if (g_strcmp0(var->type, "text") == 0) {}
        else if (g_strcmp0(var->type, "boolean") == 0) {
            if (g_ascii_strcasecmp(var->default_value, "true") != 0 &&
                g_ascii_strcasecmp(var->default_value, "false") != 0) {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                            "Template %s boolean %s default must be true or false",
                            t->id, var->name);
                return FALSE;
            }
        } else if (g_strcmp0(var->type, "choice") == 0) {
            if (!var->values || var->values->len == 0u ||
                !string_array_contains(var->values, var->default_value)) {
                g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                            GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                            "Template %s choice %s default is not in values",
                            t->id, var->name);
                return FALSE;
            }
        } else {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                        "Template %s has unknown variable type %s",
                        t->id, var->type);
            return FALSE;
        }
    }

    for (guint i = 0u; i < t->files->len; i++) {
        GraptosProjectFile *file = g_ptr_array_index(t->files, i);
        const char *source = file->template_path ? file->template_path : file->source_path;
        if (!file->path || !source) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                        "Template %s files[%u] requires path and template/source",
                        t->id, i);
            return FALSE;
        }
        g_autofree char *safe = NULL;
        if (!graptos_project_path_is_safe(source, &safe, error)) return FALSE;
        g_autofree char *src_path = g_build_filename(t->base_path, safe, NULL);
        if (g_file_test(src_path, G_FILE_TEST_IS_SYMLINK) ||
            !g_file_test(src_path, G_FILE_TEST_IS_REGULAR)) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_IO,
                        "Template %s missing source file: %s", t->id, safe);
            return FALSE;
        }
    }

    for (guint i = 0u; i < t->actions->len; i++) {
        GraptosProjectAction *action = g_ptr_array_index(t->actions, i);
        if (!action->type ||
            (g_strcmp0(action->type, "git-init") != 0 &&
             g_strcmp0(action->type, "mark-executable") != 0)) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_UNKNOWN_ACTION,
                        "Template %s has unsupported action %s",
                        t->id, action->type ? action->type : "(missing)");
            return FALSE;
        }
        if (g_strcmp0(action->type, "mark-executable") == 0 && !action->path) {
            g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                        GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                        "Template %s mark-executable requires path", t->id);
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * @brief Convert project name to a lowercase slug.
 * @details Built-in variables need predictable forms for filenames, C symbols,
 *          and include guards. The derivation is intentionally internal rather
 *          than exposed as a template filter language.
 * @param name The project name.
 * @param separator The separator to place between words.
 * @param uppercase TRUE to uppercase generated characters.
 * @return An owned derived identifier.
 */
