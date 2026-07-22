/**
 * @file src/lsp/lsp_client_completion.inc.c
 * @brief Internal LSP completion request and cache handling.
 * @details Completion is the hottest LSP path. We cache and retry carefully because
 *          language servers can answer before their own document state is ready, and we do
 *          not want that to freeze typing.
 * @param src The src supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */

static GPtrArray *completion_array_copy(GPtrArray *src,
                                        const char *prefix,
                                        guint max_results) {
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    if (!out || !src) return out;
    for (guint i = 0u; i < src->len && out->len < max_results; i++) {
        const char *item = g_ptr_array_index(src, i);
        if (!item || item[0] == '\0') continue;
        if (prefix && prefix[0] != '\0' &&
            !g_str_has_prefix(item, prefix)) {
            continue;
        }
        g_ptr_array_add(out, g_strdup(item));
    }
    return out;
}

/**
 * @brief Build a position-specific completion cache key.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param uri The document URI used for LSP or protocol routing.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param character The character supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @param member_context The member context supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *lsp_completion_cache_key(const char *uri,
                                      gint line,
                                      gint character,
                                      const char *prefix,
                                      gboolean member_context) {
    return g_strdup_printf("%s:%d:%d:%s:%d",
                           uri ? uri : "",
                           line,
                           character,
                           prefix ? prefix : "",
                           member_context ? 1 : 0);
}

/**
 * @brief Add one completion label to an array.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param items The items supplied by the caller.
 * @param label The label supplied by the caller.
 */
static void completion_array_add_unique(GPtrArray *items, const char *label) {
    if (!items || !label || label[0] == '\0') return;
    for (guint i = 0u; i < items->len; i++) {
        const char *existing = g_ptr_array_index(items, i);
        if (existing && strcmp(existing, label) == 0) return;
    }
    g_ptr_array_add(items, g_strdup(label));
}

/**
 * @brief Extract completion labels from an LSP result node.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param result The result supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GPtrArray *completion_items_from_result(JsonNode *result) {
    GPtrArray *items = g_ptr_array_new_with_free_func(g_free);
    if (!items || !result) return items;

    JsonArray *array = NULL;
    if (JSON_NODE_HOLDS_ARRAY(result)) {
        array = json_node_get_array(result);
    } else if (JSON_NODE_HOLDS_OBJECT(result)) {
        JsonObject *object = json_node_get_object(result);
        JsonNode *items_node = object && json_object_has_member(object, "items")
            ? json_object_get_member(object, "items") : NULL;
        if (items_node && JSON_NODE_HOLDS_ARRAY(items_node)) {
            array = json_node_get_array(items_node);
        }
    }
    if (!array) return items;

    guint length = json_array_get_length(array);
    for (guint i = 0u; i < length; i++) {
        JsonNode *item_node = json_array_get_element(array, i);
        if (!item_node || !JSON_NODE_HOLDS_OBJECT(item_node)) continue;
        JsonObject *item = json_node_get_object(item_node);
        const char *label = json_object_has_member(item, "insertText")
            ? json_object_get_string_member(item, "insertText")
            : json_object_has_member(item, "label")
            ? json_object_get_string_member(item, "label")
            : NULL;
        completion_array_add_unique(items, label);
    }
    return items;
}
