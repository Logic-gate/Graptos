/**
 * @brief Send one LSP location request.
 * @details LSP over stdio is just enough protocol to be annoying. We keep framing, stderr
 *          logging, shutdown, and stale-response dispatch in one layer so feature code can
 *          deal with parsed JSON instead of partial reads.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param character The character supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @param method The method supplied by the caller.
 * @param kind The kind supplied by the caller.
 * @param pending_table_for_server The pending table for server supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean lsp_client_request_location(LspClient *client,
                                            EditorTab *tab,
                                            guint line,
                                            guint character,
                                            guint max_results,
                                            const char *method,
                                            const char *kind,
                                            GHashTable *(*pending_table_for_server)(LspServer *server),
                                            LspLocationCallback callback,
                                            gpointer user_data) {
    if (!client || !tab || !tab->file_path || !method || !callback) return FALSE;
    LspServer *server = lsp_client_server_for_tab(client, tab);
    if (!server) {
        lsp_client_debug(client,
                         "%s skipped file=%s reason=no server",
                         method,
                         tab && tab->file_path ? tab->file_path : "(unsaved)");
        return FALSE;
    }
    lsp_client_document_opened(client, tab);
    if (!lsp_server_ready(server)) {
        lsp_debug(server,
                  "%s deferred until initialize completes file=%s",
                  method,
                  tab->file_path);
        return FALSE;
    }
    lsp_client_flush_pending_change(client, tab);
    g_autofree char *uri = lsp_path_to_uri(tab->file_path);
    if (!uri) return FALSE;
    guint effective_max = max_results == 0u ? 30u : max_results;
    if (strcmp(method, "textDocument/references") == 0 &&
        client->win && client->win->lsp_references_max_results > 0u) {
        effective_max = client->win->lsp_references_max_results;
    }
    GHashTable *pending_table = pending_table_for_server
        ? pending_table_for_server(server) : NULL;
    if (!pending_table) return FALSE;

    guint64 id = server->next_id++;
    lsp_debug(server,
              "%s request id=%" G_GUINT64_FORMAT " uri=%s line=%u character=%u max=%u",
              method,
              id,
              uri,
              line,
              character,
              effective_max);
    LspLocationPending *pending = g_new0(LspLocationPending, 1);
    if (!pending) return FALSE;
    pending->tab = tab;
    pending->callback = callback;
    pending->user_data = user_data;
    pending->max_results = effective_max;
    pending->kind = g_strdup(kind ? kind : "LSP");
    g_hash_table_replace(pending_table, GUINT_TO_POINTER((guint)id), pending);

    g_autoptr(JsonBuilder) builder = lsp_builder_base(method, id);
    json_builder_set_member_name(builder, "params");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "textDocument");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "uri");
    json_builder_add_string_value(builder, uri);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "position");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "line");
    json_builder_add_int_value(builder, (gint64)line);
    json_builder_set_member_name(builder, "character");
    json_builder_add_int_value(builder, (gint64)character);
    json_builder_end_object(builder);
    if (strcmp(method, "textDocument/references") == 0) {
        json_builder_set_member_name(builder, "context");
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "includeDeclaration");
        json_builder_add_boolean_value(builder, TRUE);
        json_builder_end_object(builder);
    }
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    g_autoptr(JsonNode) node = json_builder_get_root(builder);
    if (!lsp_server_write_json(server, node)) {
        g_hash_table_remove(pending_table, GUINT_TO_POINTER((guint)id));
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Return the references pending table.
 * @details References and definitions share the same request plumbing but keep
 *          separate pending maps. Returning the table through a helper makes the
 *          ownership boundary clear before a request ID is handed to the server.
 * @param server The language server whose pending references are being tracked.
 * @return The pending-request table owned by the server.
 */
static GHashTable *lsp_references_pending_table(LspServer *server) {
    return server ? server->references_pending : NULL;
}

/**
 * @brief Return definition pending table.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GHashTable *lsp_definition_pending_table(LspServer *server) {
    return server ? server->definition_pending : NULL;
}

gboolean lsp_client_request_references(LspClient *client,
/**
 * @brief Client request definition.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param character The character supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
                                       EditorTab *tab,
                                       guint line,
                                       guint character,
                                       guint max_results,
                                       LspLocationCallback callback,
                                       gpointer user_data) {
    return lsp_client_request_location(client,
                                       tab,
                                       line,
                                       character,
                                       max_results,
                                       "textDocument/references",
                                       "LSP reference",
                                       lsp_references_pending_table,
                                       callback,
                                       user_data);
}

/**
 * @brief Request an LSP definition for the current symbol.
 * @details Definition requests use the same pending-request cleanup as
 *          references, but only ask for one target. The callback still receives
 *          an array so the hover UI can render LSP and Graptoς results through
 *          the same path.
 * @param client The LSP client that owns server state.
 * @param tab The editor tab whose document is queried.
 * @param line The zero-based LSP line.
 * @param character The zero-based LSP character.
 * @param callback Callback invoked with the resolved locations.
 * @param user_data Caller data forwarded to the callback.
 * @return TRUE when a request was queued; otherwise FALSE.
 */
gboolean lsp_client_request_definition(LspClient *client,
                                       EditorTab *tab,
                                       guint line,
                                       guint character,
                                       LspLocationCallback callback,
                                       gpointer user_data) {
    return lsp_client_request_location(client,
                                       tab,
                                       line,
                                       character,
                                       1u,
                                       "textDocument/definition",
                                       "LSP definition",
                                       lsp_definition_pending_table,
                                       callback,
                                       user_data);
}
