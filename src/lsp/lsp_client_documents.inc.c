/**
 * @file src/lsp/lsp_client_documents.inc.c
 * @brief LSP document lifetime and change tracking.
 * @details Language servers care about document lifetime more than users do. We track
 *          didOpen, didChange, didSave, and didClose here so the server sees the same
 *          buffer story the editor sees.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */

/**
 * @brief Find the nearest ancestor that contains a marker file.
 * @details Package-aware servers are fastest when initialized at the package
 *          root, not just the folder the user happened to open.
 * @param start_dir Directory to begin searching from.
 * @param markers NULL-terminated marker file names.
 * @return Newly allocated root path, or NULL when no marker is found.
 */
static char *lsp_find_nearest_marker_root(const char *start_dir,
                                          const char * const *markers) {
    if (!start_dir || !markers) return NULL;
    char *current = g_canonicalize_filename(start_dir, NULL);
    while (current && current[0] != '\0') {
        for (guint i = 0u; markers[i]; i++) {
            g_autofree char *candidate = g_build_filename(current, markers[i], NULL);
            if (candidate && g_file_test(candidate, G_FILE_TEST_EXISTS)) {
                return current;
            }
        }
        g_autofree char *parent = g_path_get_dirname(current);
        if (!parent || g_strcmp0(parent, current) == 0) break;
        g_free(current);
        current = g_strdup(parent);
    }
    g_free(current);
    return NULL;
}

/**
 * @brief Find the nearest root advertised by syntax filename markers.
 * @details Syntax YAML already names exact project files such as build.zig.
 *          Reusing those markers keeps LSP root detection declarative instead
 *          of baking one language's package rules into the backend.
 * @param syntax Syntax definition that may advertise root marker filenames.
 * @param start_dir Directory to begin searching from.
 * @return Newly allocated root path, or NULL when no marker is found.
 */
static char *lsp_find_syntax_marker_root(SyntaxDef *syntax,
                                         const char *start_dir) {
    if (!syntax || !syntax->filenames || syntax->filenames->len == 0u) return NULL;
    GPtrArray *markers = g_ptr_array_new();
    if (!markers) return NULL;
    for (guint i = 0u; i < syntax->filenames->len; i++) {
        const char *name = g_ptr_array_index(syntax->filenames, i);
        if (name && name[0] != '\0') g_ptr_array_add(markers, (gpointer)name);
    }
    g_ptr_array_add(markers, NULL);
    char *root = lsp_find_nearest_marker_root(start_dir,
                                              (const char * const *)markers->pdata);
    g_ptr_array_free(markers, TRUE);
    return root;
}

/**
 * @brief Resolve the workspace root used to key an LSP server.
 * @details The root key isolates server caches and lets language servers bind
 *          themselves to the nearest package marker advertised by syntax YAML.
 * @param client LSP client that owns project context through the window.
 * @param tab Editor tab requesting an LSP feature.
 * @return Newly allocated canonical root path, or NULL when unavailable.
 */
static char *lsp_client_root_for_tab(LspClient *client, EditorTab *tab) {
    if (!client || !tab || !tab->file_path) return NULL;
    g_autofree char *canonical_file = g_canonicalize_filename(tab->file_path, NULL);
    g_autofree char *file_dir = g_path_get_dirname(canonical_file);
    char *marker_root = lsp_find_syntax_marker_root(tab->active_syntax, file_dir);
    if (marker_root) return marker_root;
    const char *project_root = project_root_for_path(client->win, canonical_file);
    if (project_root && project_root[0] != '\0') {
        return g_canonicalize_filename(project_root, NULL);
    }
    return file_dir ? g_strdup(file_dir) : NULL;
}

/**
 * @brief Build the key used for failed LSP server starts.
 * @details Missing executables should not be retried on every hover,
 *          completion, or diagnostics pass. The key includes syntax, root, and
 *          command so changing any part gives the server another chance.
 * @param syntax Syntax definition that owns the command.
 * @param root Workspace root used for the server.
 * @return Newly allocated cache key.
 */
static char *lsp_failed_server_key(SyntaxDef *syntax, const char *root) {
    return g_strdup_printf("%s|%s|%s",
                           syntax && syntax->name ? syntax->name : "",
                           root ? root : "",
                           syntax && syntax->lsp_command ? syntax->lsp_command : "");
}

/**
 * @brief Report an LSP server start failure once for a syntax/root pair.
 * @details A missing language server is actionable user configuration, not an
 *          editor crash. We show it in status and debug mode once, then stop
 *          hammering process spawn on every LSP request.
 * @param client The LSP client that owns the failed-start cache.
 * @param syntax Syntax definition whose server failed.
 * @param root Workspace root for the failed server.
 */
static void lsp_client_note_failed_server(LspClient *client,
                                          SyntaxDef *syntax,
                                          const char *root) {
    if (!client || !syntax) return;
    g_autofree char *key = lsp_failed_server_key(syntax, root);
    if (!key || (client->failed_server_keys &&
                 g_hash_table_contains(client->failed_server_keys, key))) {
        return;
    }
    if (client->failed_server_keys) {
        g_hash_table_add(client->failed_server_keys, g_strdup(key));
    }
    g_autofree char *message =
        g_strdup_printf("Could not start %s for %s. Install it or edit lsp_command in syntax/%s.yaml.",
                        syntax->lsp_command ? syntax->lsp_command : "language server",
                        syntax->name ? syntax->name : "this language",
                        syntax->name ? syntax->name : "language");
    lsp_client_debug(client, "%s root=%s", message ? message : "LSP start failed", root ? root : "");
    app_window_set_error_status(client->win, "LSP unavailable", message);
}

static LspServer *lsp_client_server_for_tab(LspClient *client, EditorTab *tab) {
    SyntaxDef *syntax = tab ? tab->active_syntax : NULL;
    if (!client || !syntax_has_lsp(syntax)) {
        lsp_client_debug(client,
                         "no server configured for syntax=%s file=%s",
                         syntax && syntax->name ? syntax->name : "(none)",
                         tab && tab->file_path ? tab->file_path : "(unsaved)");
        return NULL;
    }
    g_autofree char *root = lsp_client_root_for_tab(client, tab);
    if (!root) {
        lsp_client_debug(client,
                         "no LSP root for syntax=%s file=%s",
                         syntax && syntax->name ? syntax->name : "(none)",
                         tab && tab->file_path ? tab->file_path : "(unsaved)");
        return NULL;
    }
    g_autofree char *failed_key = lsp_failed_server_key(syntax, root);
    if (client->failed_server_keys && failed_key &&
        g_hash_table_contains(client->failed_server_keys, failed_key)) {
        lsp_client_debug(client,
                         "server start skipped after previous failure syntax=%s root=%s command=%s",
                         syntax && syntax->name ? syntax->name : "(none)",
                         root,
                         syntax && syntax->lsp_command ? syntax->lsp_command : "(none)");
        return NULL;
    }
    for (guint i = 0u; client->servers && i < client->servers->len; i++) {
        LspServer *server = g_ptr_array_index(client->servers, i);
        if (server && server->syntax == syntax && server->alive &&
            g_strcmp0(server->root_path, root) == 0) {
            return server;
        }
    }
    LspServer *server = lsp_server_new(client->win, syntax, root);
    if (server) {
        g_ptr_array_add(client->servers, server);
    } else {
        lsp_client_note_failed_server(client, syntax, root);
    }
    return server;
}

/**
 * @brief Find an already-started server for a tab without spawning.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static LspServer *lsp_client_existing_server_for_tab(LspClient *client,
                                                    EditorTab *tab) {
    SyntaxDef *syntax = tab ? tab->active_syntax : NULL;
    if (!client || !syntax) return NULL;
    g_autofree char *root = lsp_client_root_for_tab(client, tab);
    for (guint i = 0u; client->servers && i < client->servers->len; i++) {
        LspServer *server = g_ptr_array_index(client->servers, i);
        if (server && server->syntax == syntax && server->alive &&
            (!root || g_strcmp0(server->root_path, root) == 0)) {
            return server;
        }
    }
    return NULL;
}

/**
 * @brief Remove pending location requests owned by a tab from a table.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param table The table supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void lsp_cancel_tab_location_requests(GHashTable *table,
                                             EditorTab *tab) {
    if (!table || !tab) return;
    GList *remove_keys = NULL;
    GHashTableIter iter;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iter, table);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
/**
 * @brief Path to uri.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
        LspLocationPending *pending = value;
        if (pending && pending->tab == tab) {
            remove_keys = g_list_prepend(remove_keys, key);
        }
    }
    for (GList *node = remove_keys; node; node = node->next) {
/**
 * @brief Iter to position.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param iter The text iterator that anchors the lookup.
 * @param line_out Output storage filled when the operation can provide a value.
 * @param character_out Output storage filled when the operation can provide a value.
 */
        g_hash_table_remove(table, node->data);
    }
    g_list_free(remove_keys);
}

/**
 * @brief Convert a filesystem path to an LSP URI.
 * @details Language servers speak in document URIs while Graptoς stores editor
 *          tabs as filesystem paths. Canonicalizing before conversion keeps
 *          duplicate relative paths from creating separate server documents.
 * @param path The filesystem path to convert.
 * @return An owned URI string, or NULL when conversion fails.
 */
char *lsp_path_to_uri(const char *path) {
    if (!path || path[0] == '\0') return NULL;
    g_autofree char *canonical = g_canonicalize_filename(path, NULL);
    return g_filename_to_uri(canonical, NULL, NULL);
}

/**
 * @brief Convert a GtkTextIter to an LSP line/character pair.
 * @details GtkTextIter already tracks line and line-offset in the form LSP
 *          expects for the current UTF-8 buffer. The helper also supplies zero
 *          defaults for defensive callers.
 * @param iter The iterator being converted.
 * @param line_out Output storage for the zero-based line.
 * @param character_out Output storage for the zero-based character.
 */
void lsp_iter_to_position(GtkTextIter *iter, gint *line_out, gint *character_out) {
    if (line_out) *line_out = iter ? gtk_text_iter_get_line(iter) : 0;
    if (character_out) *character_out = iter ? gtk_text_iter_get_line_offset(iter) : 0;
}

/**
 * @brief Return full text for a tab.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *lsp_tab_text(EditorTab *tab) {
    return buffer_text(tab);
}

/**
 * @brief Build text document item params.
 * @details LSP wants a slightly different shape depending on the message. We
 *          build both didOpen-with-text and later URI-only notifications here
 *          so version, language id, and URI formatting do not drift apart.
 * @param builder The builder supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param uri The document URI used for LSP or protocol routing.
 * @param include_text The include text supplied by the caller.
 */
static void lsp_builder_add_text_document(JsonBuilder *builder,
                                          EditorTab *tab,
                                          const char *uri,
                                          gboolean include_text) {
    json_builder_set_member_name(builder, "textDocument");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "uri");
    json_builder_add_string_value(builder, uri);
    if (include_text) {
        json_builder_set_member_name(builder, "languageId");
        json_builder_add_string_value(builder, syntax_lsp_language_id(tab->active_syntax));
        json_builder_set_member_name(builder, "version");
        json_builder_add_int_value(builder, (gint64)tab->lsp_version);
        json_builder_set_member_name(builder, "text");
        g_autofree char *text = lsp_tab_text(tab);
        json_builder_add_string_value(builder, text ? text : "");
    }
    json_builder_end_object(builder);
}

/**
 * @brief Send a document notification with optional text.
 * @details The server only knows what we tell it. This helper keeps the common
 *          notification framing in one place so didOpen, didChange, didSave,
 *          and didClose do not accidentally disagree about the same buffer.
 * @param server The server supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param method The method supplied by the caller.
 * @param include_text The include text supplied by the caller.
 */
static void lsp_send_document_notification(LspServer *server,
                                           EditorTab *tab,
                                           const char *method,
                                           gboolean include_text) {
    if (!server || !tab || !tab->file_path || !tab->active_syntax) return;
    g_autofree char *uri = lsp_path_to_uri(tab->file_path);
    if (!uri) return;
    g_autoptr(JsonBuilder) builder = lsp_builder_base(method, 0u);
    json_builder_set_member_name(builder, "params");
    json_builder_begin_object(builder);
    lsp_builder_add_text_document(builder, tab, uri, include_text);
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    g_autoptr(JsonNode) node = json_builder_get_root(builder);
    (void)lsp_server_write_json(server, node);
}

/**
 * @brief Send didOpen for one tab on an initialized server.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void lsp_server_open_tab(LspServer *server, EditorTab *tab) {
    if (!server || !tab || !tab->file_path || !tab->active_syntax ||
        tab->active_syntax != server->syntax || !lsp_server_ready(server)) {
        return;
    }
    g_autofree char *uri = lsp_path_to_uri(tab->file_path);
    if (!uri) return;
    if (g_hash_table_contains(server->open_uris, uri)) return;

    tab->lsp_version = 1u;
    lsp_tab_clear_completion_retry(tab);
    lsp_server_clear_completion_state(server);
    g_hash_table_add(server->open_uris, g_strdup(uri));
    lsp_debug(server,
              "didOpen uri=%s language_id=%s version=%u",
              uri,
              syntax_lsp_language_id(tab->active_syntax),
              tab->lsp_version);
    lsp_send_document_notification(server, tab, "textDocument/didOpen", TRUE);
}

/**
 * @brief Send didOpen for matching tabs after a server is ready.
 * @details A language server can finish initializing after files are already
 *          open. Walking the notebook here gives the server the current buffer
 *          state without forcing tab creation code to know about LSP startup
 *          timing.
 * @param server The initialized language server that should receive open tabs.
 */
static void lsp_server_open_matching_tabs(LspServer *server) {
    if (!server || !server->win || !server->win->notebook ||
        !lsp_server_ready(server)) {
        return;
    }

    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(server->win->notebook));
    for (gint i = 0; i < pages; i++) {
/**
 * @brief Client document changed.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(server->win->notebook), i);
        EditorTab *tab = child ? g_object_get_data(G_OBJECT(child), "graptos-tab") : NULL;
        g_autofree char *root = tab ? lsp_client_root_for_tab(server->win->lsp_client, tab) : NULL;
        if (tab && g_strcmp0(server->root_path, root) == 0) {
            lsp_server_open_tab(server, tab);
        }
    }
}

/**
 * @brief Send a queued text change before an LSP feature request.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void lsp_client_flush_pending_change(LspClient *client, EditorTab *tab) {
    if (!client || !tab || tab->lsp_change_timeout == 0u) return;
    graptos_source_cancel(&tab->lsp_change_timeout);
    lsp_client_document_changed(client, tab);
}

/**
 * @brief Notify LSP that a document is open.
 * @details Tabs may open before a language server finishes initialize. Deferring
 *          the didOpen path keeps startup order simple while still letting the
 *          server catch up once it becomes ready.
 * @param client The LSP client that owns the server map.
 * @param tab The editor tab whose document should be opened.
 */
void lsp_client_document_opened(LspClient *client, EditorTab *tab) {
    LspServer *server = lsp_client_server_for_tab(client, tab);
    if (!server || !tab || !tab->file_path) return;
    if (!lsp_server_ready(server)) {
        lsp_debug(server,
                  "didOpen deferred until initialize completes file=%s",
                  tab->file_path);
        return;
    }
    lsp_server_open_tab(server, tab);
}

/**
 * @brief Notify LSP that a document changed.
 * @details Graptoς sends full text changes because that is simple and reliable
 *          for the current buffer model. The version is bumped only here so
 *          stale completion and diagnostic replies can be compared with the live
 *          tab state.
 * @param client The LSP client that owns the server map.
 * @param tab The editor tab whose document changed.
 */
void lsp_client_document_changed(LspClient *client, EditorTab *tab) {
    LspServer *server = lsp_client_server_for_tab(client, tab);
    if (!server || !tab || !tab->file_path) return;
    if (!lsp_server_ready(server)) {
        lsp_debug(server,
                  "didChange deferred until initialize completes file=%s",
                  tab->file_path);
        return;
    }
    tab->lsp_version++;
    g_autofree char *uri = lsp_path_to_uri(tab->file_path);
    if (!uri) return;
    if (!g_hash_table_contains(server->open_uris, uri)) {
        lsp_debug(server, "didChange requested before didOpen uri=%s; opening first", uri);
        lsp_client_document_opened(client, tab);
        return;
    }
    lsp_tab_clear_completion_retry(tab);
    lsp_server_clear_completion_state(server);
    lsp_debug(server, "didChange uri=%s version=%u", uri, tab->lsp_version);
    g_autoptr(JsonBuilder) builder = lsp_builder_base("textDocument/didChange", 0u);
    json_builder_set_member_name(builder, "params");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "textDocument");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "uri");
    json_builder_add_string_value(builder, uri);
    json_builder_set_member_name(builder, "version");
    json_builder_add_int_value(builder, (gint64)tab->lsp_version);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "contentChanges");
    json_builder_begin_array(builder);
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "text");
    g_autofree char *text = lsp_tab_text(tab);
    json_builder_add_string_value(builder, text ? text : "");
    json_builder_end_object(builder);
    json_builder_end_array(builder);
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    g_autoptr(JsonNode) node = json_builder_get_root(builder);
    (void)lsp_server_write_json(server, node);
}

/**
 * @brief Notify LSP that a document was saved.
 * @details Some servers only publish diagnostics or refresh semantic state on
 *          didSave. Sending the current text with the save notification keeps
 *          those servers aligned with Graptoς even when autosave or manual save
 *          paths triggered the write.
 * @param client The LSP client that owns the server map.
 * @param tab The editor tab whose document was saved.
 */
void lsp_client_document_saved(LspClient *client, EditorTab *tab) {
    LspServer *server = lsp_client_existing_server_for_tab(client, tab);
    if (!server || !tab || !tab->file_path) return;
    if (!lsp_server_ready(server)) return;
    lsp_debug(server, "didSave file=%s", tab->file_path);
    g_autofree char *uri = lsp_path_to_uri(tab->file_path);
    if (!uri) return;
    g_autoptr(JsonBuilder) builder = lsp_builder_base("textDocument/didSave", 0u);
    json_builder_set_member_name(builder, "params");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "textDocument");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "uri");
    json_builder_add_string_value(builder, uri);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "text");
    g_autofree char *text = lsp_tab_text(tab);
    json_builder_add_string_value(builder, text ? text : "");
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    g_autoptr(JsonNode) node = json_builder_get_root(builder);
    (void)lsp_server_write_json(server, node);
}

/**
 * @brief Notify LSP that a document closed.
 * @details Closing a tab invalidates pending reference and definition requests
 *          before didClose is sent. That order prevents late replies from
 *          rebuilding popovers for a buffer that no longer belongs to the UI.
 * @param client The LSP client that owns the server map.
 * @param tab The editor tab whose document closed.
 */
void lsp_client_document_closed(LspClient *client, EditorTab *tab) {
    LspServer *server = lsp_client_existing_server_for_tab(client, tab);
    if (!server || !tab) return;
    lsp_cancel_tab_location_requests(server->references_pending, tab);
    lsp_cancel_tab_location_requests(server->definition_pending, tab);
    if (!tab->file_path) return;
    g_autofree char *uri = lsp_path_to_uri(tab->file_path);
    if (!uri) return;
    lsp_debug(server, "didClose uri=%s", uri);
    lsp_send_document_notification(server, tab, "textDocument/didClose", FALSE);
    g_hash_table_remove(server->open_uris, uri);
}
