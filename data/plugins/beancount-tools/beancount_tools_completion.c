static char *token_before_cursor(const char *line_prefix) {
    const char *start = line_prefix ? line_prefix : "";
    const char *p = start;
    while (*p) {
        if (g_ascii_isspace(*p) || *p == '"' || *p == '\'') start = p + 1;
        p++;
    }
    return g_strdup(start);
}

/**
 * @brief Add matching values from a set into completion candidates.
 * @param items Candidate array.
 * @param set Hash table used as a set.
 * @param prefix Current token prefix.
 */
static void add_matches(GPtrArray *items, GHashTable *set, const char *prefix) {
    if (!items || !set) return;
    GHashTableIter iter;
    gpointer key = NULL;
    g_hash_table_iter_init(&iter, set);
    while (items->len < BEANCOUNT_MAX_COMPLETIONS && g_hash_table_iter_next(&iter, &key, NULL)) {
        const char *value = key;
        if (!prefix || !prefix[0] || g_str_has_prefix(value, prefix)) {
            g_ptr_array_add(items, g_strdup(value));
        }
    }
}

/**
 * @brief Build completion candidates for Beancount context.
 * @param context Command context supplied by Graptoς.
 * @param replace_prefix_out Owned replacement prefix returned to Graptoς.
 * @param user_data Plugin data supplied during registration.
 * @return GPtrArray of owned char* candidates, or NULL.
 */
static GPtrArray *beancount_completion_candidates(GraptosPluginCommandContext *context,
                                                  char **replace_prefix_out,
                                                  gpointer user_data) {
    (void)user_data;
    if (replace_prefix_out) *replace_prefix_out = NULL;
    if (!is_beancount_context(context)) return NULL;

    char *prefix_line = graptos_plugin_context_line_prefix(context);
    char *token = token_before_cursor(prefix_line);
    char *text = graptos_plugin_context_text(context);
    char *path = graptos_plugin_context_file_path(context);
    char *base_dir = path ? g_path_get_dirname(path) : NULL;

    BeanIndex *index = bean_index_new();
    for (guint i = 0u; i < G_N_ELEMENTS(DIRECTIVES); i++) set_add(index->metadata, DIRECTIVES[i]);
    index_text(index, text, base_dir);

    GPtrArray *items = g_ptr_array_new_with_free_func(g_free);
    if (g_str_has_prefix(token, "#")) {
        add_matches(items, index->tags, token);
    } else if (g_str_has_prefix(token, "^")) {
        add_matches(items, index->links, token);
    } else if (starts_account_root(token) || strchr(token, ':')) {
        add_matches(items, index->accounts, token);
    } else {
        add_matches(items, index->accounts, token);
        add_matches(items, index->commodities, token);
        add_matches(items, index->payees, token);
        add_matches(items, index->narrations, token);
        add_matches(items, index->metadata, token);
    }

    if (items->len == 0u) {
        g_ptr_array_free(items, TRUE);
        items = NULL;
    } else if (replace_prefix_out) {
        *replace_prefix_out = g_strdup(token);
    }

    bean_index_free(index);
    g_free(base_dir);
    g_free(path);
    g_free(text);
    g_free(token);
    g_free(prefix_line);
    return items;
}

/**
 * @brief Show Beancount completions for the shortcut command.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void show_beancount_completion(GraptosPluginCommandContext *context,
                                      gpointer user_data) {
    char *replace_prefix = NULL;
    GPtrArray *items = beancount_completion_candidates(context, &replace_prefix, user_data);
    if (!items || items->len == 0u || !replace_prefix) {
        graptos_plugin_context_set_status(context, "No Beancount completions");
        g_free(replace_prefix);
        if (items) g_ptr_array_free(items, TRUE);
        return;
    }
    graptos_plugin_context_show_completions(context, replace_prefix, "Beancount", items);
    g_free(replace_prefix);
    g_ptr_array_free(items, TRUE);
}

/**
 * @brief Return whether a token is a Beancount account.
 * @param word Hover token.
 * @return TRUE when the token starts with an account root.
 */
