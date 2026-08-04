static gboolean is_account_word(const char *word) {
    return word && starts_account_root(word);
}

/**
 * @brief Build an include-aware index for the active ledger.
 * @param context Command context supplied by Graptoς.
 * @return Newly allocated ledger index.
 */
static BeanIndex *index_for_context(GraptosPluginCommandContext *context) {
    char *text = graptos_plugin_context_text(context);
    char *path = graptos_plugin_context_file_path(context);
    char *base_dir = path ? g_path_get_dirname(path) : NULL;
    BeanIndex *index = bean_index_new();
    index_text(index, text, base_dir);
    g_free(base_dir);
    g_free(path);
    g_free(text);
    return index;
}

/**
 * @brief Accumulate child account balances into a summary table.
 * @param summary Commodity balance table receiving totals.
 * @param table Commodity balance table from one account.
 */
static void merge_balance_table(GHashTable *summary, GHashTable *table) {
    if (!summary || !table) return;
    GHashTableIter iter;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const char *commodity = key;
        double amount = value ? *((double *)value) : 0.0;
        double *slot = g_hash_table_lookup(summary, commodity);
        if (!slot) {
            slot = g_new0(double, 1);
            g_hash_table_insert(summary, g_strdup(commodity), slot);
        }
        *slot += amount;
    }
}

/**
 * @brief Add formatted balance lines to a string.
 * @param out Output string.
 * @param summary Commodity balance table.
 */
static void append_balance_lines(GString *out, GHashTable *summary) {
    if (!out || !summary) return;
    GList *commodities = g_hash_table_get_keys(summary);
    commodities = g_list_sort(commodities, (GCompareFunc)g_strcmp0);
    for (GList *l = commodities; l; l = l->next) {
        const char *commodity = l->data;
        double *amount = g_hash_table_lookup(summary, commodity);
        if (!amount || *amount == 0.0) continue;
        g_string_append_printf(out, "%.2f %s\n", *amount, commodity);
    }
    g_list_free(commodities);
}

/**
 * @brief Return a Beancount account balance hover.
 * @details Balances are summed from postings in the active buffer and included
 *          ledgers. Amounts remain grouped by commodity and are not converted.
 * @param context Command context supplied by Graptoς.
 * @param word Account token under the pointer.
 * @param user_data Plugin data supplied during registration.
 * @return Owned hover body, or NULL.
 */
static char *account_balance_hover(GraptosPluginCommandContext *context,
                                   const char *word,
                                   gpointer user_data) {
    (void)user_data;
    if (!is_beancount_context(context) || !is_account_word(word)) return NULL;

    BeanIndex *index = index_for_context(context);
    GHashTable *summary = g_hash_table_new_full(g_str_hash,
                                                g_str_equal,
                                                g_free,
                                                g_free);
    GHashTableIter iter;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iter, index->balances);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const char *account = key;
        if (account_is_under(account, word)) {
            merge_balance_table(summary, value);
        }
    }

    GString *out = g_string_new(NULL);
    g_string_append_printf(out, "%s\n", word);
    append_balance_lines(out, summary);
    if (out->len == strlen(word) + 1u) {
        g_string_append(out, "No local postings found.");
    } else if (index->files > 0u) {
        g_string_append_printf(out, "Includes: %u file%s",
                               index->files,
                               index->files == 1u ? "" : "s");
    }

    g_hash_table_destroy(summary);
    bean_index_free(index);
    return g_string_free(out, FALSE);
}
