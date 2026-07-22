/**
 * @brief Return a single line copied from a text snapshot.
 * @details LSP over stdio is just enough protocol to be annoying. We keep framing, stderr
 *          logging, shutdown, and stale-response dispatch in one layer so feature code can
 *          deal with parsed JSON instead of partial reads.
 * @param text The text fragment supplied by the caller.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *line_from_text(const char *text, guint line) {
    if (!text || line == 0u) return g_strdup("");
    const char *p = text;
    guint current = 1u;
    while (*p && current < line) {
        if (*p == '\n') current++;
        p++;
    }
    if (current != line) return g_strdup("");
    const char *end = strchr(p, '\n');
    if (!end) end = p + strlen(p);
    gsize len = (gsize)(end - p);
    if (len > GRAPTOS_LSP_SNIPPET_MAX_LINE) len = GRAPTOS_LSP_SNIPPET_MAX_LINE;
    char *out = g_strndup(p, len);
    return out ? g_strstrip(out) : g_strdup("");
}

/**
 * @brief Return a compact location display path.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *lsp_reference_display_path(EditorTab *tab, const char *path) {
    if (!path) return g_strdup("current buffer");
    const char *root = tab && tab->win ? project_root_for_path(tab->win, path) : NULL;
    if (root && g_str_has_prefix(path, root)) {
        const char *rel = path + strlen(root);
        if (*rel == G_DIR_SEPARATOR) rel++;
        return g_strdup(rel && *rel ? rel : path);
    }
    return g_filename_display_name(path);
}

/**
 * @brief Read a location snippet from a tab or file.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param path The filesystem path supplied by the caller.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *lsp_reference_snippet(EditorTab *tab, const char *path, guint line) {
    char *text = NULL;
    if (tab && path && tab->file_path && strcmp(path, tab->file_path) == 0) {
        text = lsp_tab_text(tab);
    } else if (path) {
        g_file_get_contents(path, &text, NULL, NULL);
    } else if (tab) {
        text = lsp_tab_text(tab);
    }
    char *snippet = line_from_text(text, line);
    g_free(text);
    return snippet;
}

/**
 * @brief Create an index-compatible reference from an LSP location.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param path The filesystem path supplied by the caller.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param kind The kind supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static IndexReference *lsp_reference_new(EditorTab *tab,
                                         const char *path,
                                         guint line,
                                         const char *kind) {
    if (line == 0u) return NULL;
    IndexReference *ref = g_new0(IndexReference, 1);
    if (!ref) return NULL;
    ref->path = path ? g_strdup(path) : NULL;
    ref->display = lsp_reference_display_path(tab, path);
    ref->line = line;
    ref->snippet = lsp_reference_snippet(tab, path, line);
    ref->kind = g_strdup(kind ? kind : "LSP");
    return ref;
}

/**
 * @brief Add one LSP location object to an output array.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param out Output storage filled when the lookup succeeds.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param object The object supplied by the caller.
 * @param kind The kind supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
static void lsp_locations_add_object(GPtrArray *out,
                                     EditorTab *tab,
                                     JsonObject *object,
                                     const char *kind,
                                     guint max_results) {
    if (!out || !object || out->len >= max_results) return;
    const char *uri = NULL;
    JsonObject *range = NULL;
    if (json_object_has_member(object, "uri")) {
        uri = json_object_get_string_member(object, "uri");
        range = json_object_has_member(object, "range")
            ? json_object_get_object_member(object, "range") : NULL;
    } else if (json_object_has_member(object, "targetUri")) {
        uri = json_object_get_string_member(object, "targetUri");
        range = json_object_has_member(object, "targetRange")
            ? json_object_get_object_member(object, "targetRange") : NULL;
    }
    if (!uri || !range || !g_str_has_prefix(uri, "file://")) return;
    JsonObject *start = json_object_has_member(range, "start")
        ? json_object_get_object_member(range, "start") : NULL;
    if (!start || !json_object_has_member(start, "line")) return;
    guint line = (guint)json_object_get_int_member(start, "line") + 1u;
    g_autofree char *path = g_filename_from_uri(uri, NULL, NULL);
    if (!path) return;
    IndexReference *ref = lsp_reference_new(tab, path, line, kind);
    if (ref) g_ptr_array_add(out, ref);
}

/**
 * @brief Convert LSP Location or LocationLink result into references.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param result The result supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param kind The kind supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GPtrArray *lsp_locations_from_result(JsonNode *result,
                                            EditorTab *tab,
                                            const char *kind,
                                            guint max_results) {
    if (max_results == 0u) max_results = 30u;
    GPtrArray *out = g_ptr_array_new_with_free_func(index_reference_free);
    if (!out || !result || JSON_NODE_HOLDS_NULL(result)) return out;
    if (JSON_NODE_HOLDS_OBJECT(result)) {
        lsp_locations_add_object(out,
                                 tab,
                                 json_node_get_object(result),
                                 kind,
                                 max_results);
        return out;
    }
    if (!JSON_NODE_HOLDS_ARRAY(result)) return out;
    JsonArray *array = json_node_get_array(result);
    guint length = json_array_get_length(array);
    for (guint i = 0u; i < length && out->len < max_results; i++) {
        JsonNode *node = json_array_get_element(array, i);
        if (node && JSON_NODE_HOLDS_OBJECT(node)) {
            lsp_locations_add_object(out,
                                     tab,
                                     json_node_get_object(node),
                                     kind,
                                     max_results);
        }
    }
    return out;
}

/**
 * @brief Duplicate a cached string array.
 */

