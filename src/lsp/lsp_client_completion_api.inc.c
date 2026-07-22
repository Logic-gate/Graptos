/**
 * @file src/lsp/lsp_client_completion_api.inc.c
 * @brief Public LSP completion API used by editor completion UI.
 * @details This is the synchronous-looking edge around asynchronous completion work. We
 *          return cached results immediately, ask the server for fresher data, and keep
 *          Graptoς completions available while the server catches up.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param prefix The prefix supplied by the caller.
 * @param member_context The member context supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */

GPtrArray *lsp_client_completion_candidates(LspClient *client,
                                            EditorTab *tab,
                                            const char *prefix,
                                            gboolean member_context,
                                            guint max_results) {
    if (client && client->win && client->win->lsp_completion_max_results > 0u) {
        max_results = client->win->lsp_completion_max_results;
    } else if (max_results == 0u) {
        max_results = GRAPTOS_COMPLETION_DEFAULT_MAX_RESULTS;
    }
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    LspServer *server = lsp_client_server_for_tab(client, tab);
    if (!out || !server || !tab || !tab->file_path) {
        lsp_client_debug(client,
                         "completion skipped file=%s reason=%s",
                         tab && tab->file_path ? tab->file_path : "(unsaved)",
                         server ? "missing tab/file" : "no server");
        return out;
    }
    lsp_client_document_opened(client, tab);
    if (!lsp_server_ready(server)) {
        lsp_debug(server,
                  "completion deferred until initialize completes file=%s",
                  tab->file_path);
        return out;
    }
    /* Request fire-and-forget in v1. Existing Graptoς completions remain the
     * synchronous fallback until async completion-result plumbing is added.
     */
    g_autofree char *uri = lsp_path_to_uri(tab->file_path);
    if (!uri) return out;
    if (tab->completion_manual) {
        lsp_client_flush_pending_change(client, tab);
    } else if (tab->lsp_change_timeout != 0u) {
        lsp_debug(server,
                  "completion using last synced document; didChange still debounced file=%s",
                  tab->file_path);
    }

    GtkTextIter cursor;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, mark);
    GtkTextIter request_iter = cursor;
    if (member_context && prefix && prefix[0] != '\0') {
        gint prefix_chars = (gint)g_utf8_strlen(prefix, -1);
        if (prefix_chars > 0) {
            gtk_text_iter_backward_chars(&request_iter, prefix_chars);
        }
    }
    gint line = 0;
    gint character = 0;
    lsp_iter_to_position(&request_iter, &line, &character);
    g_autofree char *cache_key = lsp_completion_cache_key(uri,
                                                          line,
                                                          character,
                                                          prefix,
                                                          member_context);
    GPtrArray *cached = server->completion_cache
        ? g_hash_table_lookup(server->completion_cache, cache_key) : NULL;
    if (cached) {
        GPtrArray *copy = completion_array_copy(cached, prefix, max_results);
        g_ptr_array_free(out, TRUE);
        out = copy;
        lsp_debug(server,
                  "completion cache hit uri=%s key=%s prefix=%s items=%u",
                  uri,
                  cache_key,
                  prefix ? prefix : "",
                  out ? out->len : 0u);
    } else {
        lsp_debug(server,
                  "completion cache miss uri=%s key=%s prefix=%s",
                  uri,
                  cache_key,
                  prefix ? prefix : "");
    }
    if (tab->completion_refreshing_from_lsp) {
        lsp_debug(server,
                  "completion refresh from cache; request suppressed uri=%s prefix=%s",
                  uri,
                  prefix ? prefix : "");
        return out;
    }
    if (server->completion_pending) {
        GHashTableIter pending_iter;
        gpointer pending_value = NULL;
        g_hash_table_iter_init(&pending_iter, server->completion_pending);
        while (g_hash_table_iter_next(&pending_iter, NULL, &pending_value)) {
            LspCompletionPending *pending = pending_value;
            if (pending &&
                g_strcmp0(pending->cache_key, cache_key) == 0) {
                lsp_debug(server,
                          "completion request suppressed; pending exists uri=%s key=%s prefix=%s",
                          uri,
                          cache_key,
                          prefix ? prefix : "");
                return out;
            }
        }
    }
    guint64 id = server->next_id++;
    lsp_debug(server,
              "completion request id=%" G_GUINT64_FORMAT " uri=%s key=%s line=%d character=%d cursor_line=%d cursor_character=%d prefix=%s member=%d",
              id,
              uri,
              cache_key,
              line,
              character,
              gtk_text_iter_get_line(&cursor),
              gtk_text_iter_get_line_offset(&cursor),
              prefix ? prefix : "",
              member_context ? 1 : 0);
    if (server->completion_pending) {
        LspCompletionPending *pending = g_new0(LspCompletionPending, 1);
        if (pending) {
            pending->tab = tab;
            pending->uri = g_strdup(uri);
            pending->cache_key = g_strdup(cache_key);
            pending->prefix = g_strdup(prefix ? prefix : "");
            pending->version = tab->lsp_version;
            pending->retry_count = g_strcmp0(tab->lsp_completion_retry_key, cache_key) == 0
                ? tab->lsp_completion_retry_count
                : 0u;
            pending->manual = tab->completion_manual;
            g_hash_table_replace(server->completion_pending,
                                 GUINT_TO_POINTER((guint)id),
                                 pending);
        }
    }
    g_autoptr(JsonBuilder) builder = lsp_builder_base("textDocument/completion", id);
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
    json_builder_add_int_value(builder, line);
    json_builder_set_member_name(builder, "character");
    json_builder_add_int_value(builder, character);
    json_builder_end_object(builder);
    if (member_context) {
        json_builder_set_member_name(builder, "context");
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "triggerKind");
        json_builder_add_int_value(builder, 2);
        json_builder_set_member_name(builder, "triggerCharacter");
        json_builder_add_string_value(builder, ".");
        json_builder_end_object(builder);
    }
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    g_autoptr(JsonNode) node = json_builder_get_root(builder);
    (void)lsp_server_write_json(server, node);
    return out;
}
