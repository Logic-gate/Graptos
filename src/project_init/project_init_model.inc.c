/**
 * @brief Return the project-init error quark.
 * @details A dedicated domain lets callers distinguish template errors from
 *          lower-level GLib IO failures while still propagating a normal GError.
 * @return The stable project-init error quark.
 */
GQuark graptos_project_init_error_quark(void) {
    return g_quark_from_static_string("graptos-project-init-error-quark");
}

/**
 * @brief Free a template variable.
 * @details Variables own their choice array because discovery returns templates
 *          that can outlive the parsed file buffer.
 * @param data The GraptosProjectVariable to free.
 */
static void variable_free(gpointer data) {
    GraptosProjectVariable *var = data;
    if (!var) return;
    g_free(var->name);
    g_free(var->label);
    g_free(var->type);
    g_free(var->default_value);
    if (var->values) g_ptr_array_free(var->values, TRUE);
    g_free(var);
}

/**
 * @brief Free a template directory entry.
 * @details Directory declarations own path and condition strings parsed from
 *          the manifest.
 * @param data The GraptosProjectDirectory to free.
 */
static void directory_free(gpointer data) {
    GraptosProjectDirectory *dir = data;
    if (!dir) return;
    g_free(dir->path);
    g_free(dir->condition);
    g_free(dir);
}

/**
 * @brief Free a template file entry.
 * @details File declarations own both rendered and static source paths because
 *          validation may inspect them after the manifest buffer is gone.
 * @param data The GraptosProjectFile to free.
 */
static void file_free(gpointer data) {
    GraptosProjectFile *file = data;
    if (!file) return;
    g_free(file->path);
    g_free(file->template_path);
    g_free(file->source_path);
    g_free(file->condition);
    g_free(file);
}

/**
 * @brief Free a template action entry.
 * @details Actions carry only built-in operation names and optional paths, but
 *          they still follow the same owned-string cleanup rule as files.
 * @param data The GraptosProjectAction to free.
 */
static void action_free(gpointer data) {
    GraptosProjectAction *action = data;
    if (!action) return;
    g_free(action->type);
    g_free(action->path);
    g_free(action->condition);
    g_free(action);
}

/**
 * @brief Free a planned directory.
 * @details Planned entries are detached from the template so preview and apply
 *          can outlive parser state safely.
 * @param data The GraptosProjectPlannedDirectory to free.
 */
static void planned_directory_free(gpointer data) {
    GraptosProjectPlannedDirectory *dir = data;
    if (!dir) return;
    g_free(dir->path);
    g_free(dir);
}

/**
 * @brief Free a planned file.
 * @details Rendered content is stored as GBytes so worker threads can consume a
 *          stable immutable payload.
 * @param data The GraptosProjectPlannedFile to free.
 */
static void planned_file_free(gpointer data) {
    GraptosProjectPlannedFile *file = data;
    if (!file) return;
    g_free(file->path);
    if (file->content) g_bytes_unref(file->content);
    g_free(file);
}

/**
 * @brief Free a planned action.
 * @details Planned actions contain only validated built-in operations, so the
 *          apply phase never needs template context.
 * @param data The GraptosProjectPlannedAction to free.
 */
static void planned_action_free(gpointer data) {
    GraptosProjectPlannedAction *action = data;
    if (!action) return;
    g_free(action->type);
    g_free(action->path);
    g_free(action);
}

/**
 * @brief Free a parsed template.
 * @details The template is immutable after parsing, so all owned arrays can be
 *          released from one cleanup point.
 * @param template_def The template to free.
 */
void graptos_project_template_free(GraptosProjectTemplate *template_def) {
    if (!template_def) return;
    g_free(template_def->id);
    g_free(template_def->name);
    g_free(template_def->description);
    g_free(template_def->language);
    g_free(template_def->base_path);
    if (template_def->variables) g_ptr_array_free(template_def->variables, TRUE);
    if (template_def->directories) g_ptr_array_free(template_def->directories, TRUE);
    if (template_def->files) g_ptr_array_free(template_def->files, TRUE);
    if (template_def->actions) g_ptr_array_free(template_def->actions, TRUE);
    g_free(template_def->open_project);
    g_free(template_def->open_file);
    g_free(template_def);
}

/**
 * @brief Free a resolved project plan.
 * @details Plans can be built repeatedly while the user edits options in the
 *          dialog. This cleanup owns rendered bytes and final operation arrays.
 * @param plan The plan to free.
 */
void graptos_project_plan_free(GraptosProjectPlan *plan) {
    if (!plan) return;
    g_free(plan->destination);
    if (plan->variables) g_hash_table_unref(plan->variables);
    if (plan->directories) g_ptr_array_free(plan->directories, TRUE);
    if (plan->files) g_ptr_array_free(plan->files, TRUE);
    if (plan->actions) g_ptr_array_free(plan->actions, TRUE);
    g_free(plan->open_project);
    g_free(plan->open_file);
    g_free(plan);
}

/**
 * @brief Strip matching quotes from a YAML scalar.
 * @details The manifest subset accepts simple quoted and unquoted scalars. The
 *          returned string is always owned so parser state never points into the
 *          mutable line buffer.
 * @param value The scalar text after the colon.
 * @return An owned scalar string.
 */
static char *scalar_dup(const char *value) {
    if (!value) return g_strdup("");
    g_autofree char *copy = g_strdup(value);
    char *s = g_strstrip(copy);
    gsize len = strlen(s);
    if (len >= 2u && ((s[0] == '"' && s[len - 1u] == '"') ||
                      (s[0] == '\'' && s[len - 1u] == '\''))) {
        s[len - 1u] = '\0';
        return g_strdup(s + 1);
    }
    return g_strdup(s);
}

/**
 * @brief Remove comments from a manifest line.
 * @details Templates in v1 do not need YAML escapes. Treating # as a comment
 *          only outside quotes gives human-readable manifests without pulling in
 *          a full YAML parser.
 * @param line The mutable line to trim.
 */
static void strip_comment(char *line) {
    gboolean quoted = FALSE;
    char quote = '\0';
    for (char *p = line; p && *p; p++) {
        if ((*p == '"' || *p == '\'') && (p == line || p[-1] != '\\')) {
            if (!quoted) {
                quoted = TRUE;
                quote = *p;
            } else if (*p == quote) {
                quoted = FALSE;
            }
        }
        if (*p == '#' && !quoted) {
            *p = '\0';
            return;
        }
    }
}

/**
 * @brief Test whether a string is a valid template identifier.
 * @details Variable names and template ids feed placeholder lookup and duplicate
 *          detection. Restricting them avoids surprising names that look like
 *          expressions.
 * @param text The identifier candidate.
 * @return TRUE when the identifier is accepted.
 */
static gboolean valid_identifier(const char *text) {
    if (!text || !(g_ascii_isalpha(text[0]) || text[0] == '_')) return FALSE;
    for (const char *p = text + 1; *p; p++) {
        if (!(g_ascii_isalnum(*p) || *p == '_' || *p == '-')) return FALSE;
    }
    return TRUE;
}

/**
 * @brief Test whether a string is a valid variable name.
 * @details Variable names become placeholder keys. Restricting them to C-like
 *          identifiers keeps the placeholder parser simple and unambiguous.
 * @param text The variable name candidate.
 * @return TRUE when the name is accepted.
 */
static gboolean valid_variable_name(const char *text) {
    if (!text || !(g_ascii_isalpha(text[0]) || text[0] == '_')) return FALSE;
    for (const char *p = text + 1; *p; p++) {
        if (!(g_ascii_isalnum(*p) || *p == '_')) return FALSE;
    }
    return TRUE;
}

/**
 * @brief Parse an inline string list.
 * @details Choice values use the documented `[a, b, c]` form. Keeping this
 *          parser narrow gives clear errors when a template tries unsupported
 *          YAML.
 * @param value The scalar list text.
 * @param error Return location for validation errors.
 * @return An owned string array, or NULL on failure.
 */
static GPtrArray *parse_inline_list(const char *value, GError **error) {
    g_autofree char *copy = scalar_dup(value);
    char *s = g_strstrip(copy);
    gsize len = strlen(s);
    if (len < 2u || s[0] != '[' || s[len - 1u] != ']') {
        g_set_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                    GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
                    "choice values must use an inline [a, b] list");
        return NULL;
    }
    s[len - 1u] = '\0';
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    char **parts = g_strsplit(s + 1, ",", -1);
    for (guint i = 0u; parts && parts[i]; i++) {
        g_autofree char *item = scalar_dup(parts[i]);
        if (item[0] != '\0') g_ptr_array_add(out, g_strdup(item));
    }
    g_strfreev(parts);
    return out;
}

/**
 * @brief Set an owned string slot.
 * @details Parser code repeatedly replaces optional fields as lines are read,
 *          so the helper keeps old values from leaking.
 * @param slot The destination string pointer.
 * @param value The new value.
 */
static void set_string(char **slot, const char *value) {
    g_free(*slot);
    *slot = scalar_dup(value);
}

/**
 * @brief Parse a scalar key/value line.
 * @details The subset parser only supports `key: value` lines. Callers decide
 *          which keys are valid in the current section.
 * @param line The trimmed line.
 * @param key_out Output storage for an owned key.
 * @param value_out Output storage for an owned value.
 * @return TRUE when a key/value pair was parsed.
 */
static gboolean parse_key_value(const char *line, char **key_out, char **value_out) {
    const char *colon = strchr(line, ':');
    if (!colon) return FALSE;
    g_autofree char *key = g_strndup(line, (gsize)(colon - line));
    g_autofree char *value = g_strdup(colon + 1);
    g_strstrip(key);
    g_strstrip(value);
    if (key[0] == '\0') return FALSE;
    if (key_out) *key_out = g_strdup(key);
    if (value_out) *value_out = g_strdup(value);
    return TRUE;
}

/**
 * @brief Create an empty parsed template shell.
 * @details Every array has a free function from the start, so failure paths can
 *          call the public template cleanup without checking partial state.
 * @return A newly allocated template.
 */
static GraptosProjectTemplate *template_new(void) {
    GraptosProjectTemplate *t = g_new0(GraptosProjectTemplate, 1);
    t->variables = g_ptr_array_new_with_free_func(variable_free);
    t->directories = g_ptr_array_new_with_free_func(directory_free);
    t->files = g_ptr_array_new_with_free_func(file_free);
    t->actions = g_ptr_array_new_with_free_func(action_free);
    return t;
}

/**
 * @brief Parse a template manifest.
 * @details This is a strict subset parser for the documented schema. It rejects
 *          unsupported shapes rather than guessing because generation later
 *          writes files to disk.
 * @param template_dir The directory containing template.yaml.
 * @param source The discovery source assigned to the template.
 * @param error Return location for parser errors.
 * @return A parsed template, or NULL on failure.
 */
