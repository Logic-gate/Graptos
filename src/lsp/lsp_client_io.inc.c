/**
 * @file src/lsp/lsp_client_io.inc.c
 * @brief LSP process I/O and message dispatch.
 * @details LSP over stdio is just enough protocol to be annoying. We keep framing, stderr
 *          logging, shutdown, and stale-response dispatch in one layer so feature code can
 *          deal with parsed JSON instead of partial reads.
 * @param server The server supplied by the caller.
 */

static void lsp_server_read_next(LspServer *server);
static void lsp_server_read_body_next(LspBodyRead *state);
static void lsp_server_read_separator_next(LspBodyRead *state);

/**
 * @brief Continue reading LSP stderr debug lines.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 */
static void lsp_server_read_stderr_next(LspServer *server);

/**
 * @brief Handle one LSP stderr line in debug mode.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param source The source supplied by the caller.
 * @param res The res supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void lsp_server_read_stderr_cb(GObject *source,
                                      GAsyncResult *res,
                                      gpointer user_data) {
    LspServer *server = user_data;
    g_autoptr(GError) error = NULL;
    gsize line_len = 0u;
    char *line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(source),
                                                           res,
                                                           &line_len,
                                                           &error);
    if (!server) {
        g_free(line);
        return;
    }
    if (error || !line) {
        lsp_debug(server,
                  "stderr closed%s%s",
                  error ? ": " : "",
                  error ? error->message : "");
/**
 * @brief Server read stderr next.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 */
        g_free(line);
        return;
    }
    if (line[0] != '\0') lsp_debug(server, "stderr: %s", line);
    g_free(line);
    lsp_server_read_stderr_next(server);
}

/**
 * @brief Queue the next stderr read for a language server.
 * @details Server stderr is diagnostic output for Graptoς debug mode, not editor
 *          data. Reading it at low priority keeps noisy servers from blocking
 *          protocol responses on stdout.
 * @param server The language server whose stderr stream should be read.
 */
static void lsp_server_read_stderr_next(LspServer *server) {
    if (!server || !server->alive || !server->error_output) return;
    g_data_input_stream_read_line_async(server->error_output,
                                        G_PRIORITY_LOW,
                                        server->cancellable,
                                        lsp_server_read_stderr_cb,
                                        server);
}

/**
 * @brief Parse one LSP payload.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @param payload The payload supplied by the caller.
 */
static void lsp_server_handle_payload(LspServer *server, const char *payload) {
    if (!server || !payload) return;
    g_autoptr(JsonParser) parser = json_parser_new();
    g_autoptr(GError) parse_error = NULL;
    if (!json_parser_load_from_data(parser, payload, -1, &parse_error)) {
        lsp_debug(server,
                  "failed to parse payload: %s",
                  parse_error ? parse_error->message : "unknown parse error");
        return;
    }
    JsonNode *root = json_parser_get_root(parser);
    if (!root || !JSON_NODE_HOLDS_OBJECT(root)) return;
    JsonObject *object = json_node_get_object(root);
    const char *method = json_object_has_member(object, "method")
        ? json_object_get_string_member(object, "method") : NULL;
    JsonNode *request_id_node = json_object_has_member(object, "id")
        ? json_object_get_member(object, "id") : NULL;
    if (method && strcmp(method, "textDocument/publishDiagnostics") == 0) {
        JsonObject *params = json_object_has_member(object, "params")
            ? json_object_get_object_member(object, "params") : NULL;
        JsonArray *diagnostics = params && json_object_has_member(params, "diagnostics")
            ? json_object_get_array_member(params, "diagnostics") : NULL;
        const char *uri = params && json_object_has_member(params, "uri")
            ? json_object_get_string_member(params, "uri") : "(unknown)";
        lsp_debug(server,
                  "publishDiagnostics uri=%s count=%u",
                  uri,
                  diagnostics ? json_array_get_length(diagnostics) : 0u);
        lsp_apply_publish_diagnostics(server, uri, diagnostics);
        return;
    }
    if (method) {
        lsp_debug(server, "notification/request method=%s", method);
        if (request_id_node) {
            JsonObject *params = json_object_has_member(object, "params")
                ? json_object_get_object_member(object, "params") : NULL;
            (void)lsp_server_reply_to_request(server, request_id_node, method, params);
        }
    }
    if (json_object_has_member(object, "result")) {
        guint id = json_object_has_member(object, "id")
            ? (guint)json_object_get_int_member(object, "id") : 0u;
        JsonNode *result = json_object_get_member(object, "result");
        if (!result) return;
        lsp_debug(server, "response id=%u received", id);
        if (server->initialize_id != 0u &&
            id == (guint)server->initialize_id &&
            !server->initialized_notification_sent) {
            server->initialized_notification_sent = TRUE;
            lsp_server_send_initialized(server);
            lsp_debug(server, "initialize complete; opening matching tabs");
            lsp_server_open_matching_tabs(server);
            return;
        }
        LspCompletionPending *completion_pending = id != 0u && server->completion_pending
            ? g_hash_table_lookup(server->completion_pending, GUINT_TO_POINTER(id))
            : NULL;
        if (completion_pending) {
            g_hash_table_steal(server->completion_pending, GUINT_TO_POINTER(id));
            GPtrArray *items = completion_items_from_result(result);
            lsp_debug(server,
                      "completion response id=%u uri=%s key=%s version=%u current=%u items=%u",
                      id,
                      completion_pending->uri ? completion_pending->uri : "(unknown)",
                      completion_pending->cache_key ? completion_pending->cache_key : "(none)",
                      completion_pending->version,
                      completion_pending->tab ? completion_pending->tab->lsp_version : 0u,
                      items ? items->len : 0u);
            if (completion_pending->tab &&
                completion_pending->version != completion_pending->tab->lsp_version) {
                lsp_debug(server,
                          "completion response id=%u discarded; document version changed",
                          id);
                g_ptr_array_free(items, TRUE);
                lsp_completion_pending_free(completion_pending);
                return;
            }
            if (items && items->len == 0u &&
                lsp_schedule_empty_completion_retry(server, completion_pending)) {
                g_ptr_array_free(items, TRUE);
                lsp_completion_pending_free(completion_pending);
                return;
            }
            if (items && items->len > 0u && completion_pending->tab) {
                graptos_source_cancel(&completion_pending->tab->lsp_completion_retry_timeout);
                completion_pending->tab->lsp_completion_retry_count = 0u;
                g_clear_pointer(&completion_pending->tab->lsp_completion_retry_key, g_free);
            }
            g_hash_table_replace(server->completion_cache,
                                 g_strdup(completion_pending->cache_key),
                                 items);
            if (completion_pending->tab &&
                completion_pending->tab->autocomplete_enabled) {
                completion_pending->tab->completion_refreshing_from_lsp = TRUE;
                editor_tab_show_completion(completion_pending->tab,
                                           completion_pending->manual);
                completion_pending->tab->completion_refreshing_from_lsp = FALSE;
            }
            lsp_completion_pending_free(completion_pending);
            return;
        }
        LspLocationPending *pending = server->references_pending
            ? g_hash_table_lookup(server->references_pending, GUINT_TO_POINTER(id))
            : NULL;
        GHashTable *pending_table = server->references_pending;
        if (!pending && server->definition_pending) {
            pending = g_hash_table_lookup(server->definition_pending, GUINT_TO_POINTER(id));
            pending_table = server->definition_pending;
        }
        if (pending) {
            g_hash_table_steal(pending_table, GUINT_TO_POINTER(id));
            GPtrArray *locations = lsp_locations_from_result(result,
                                                             pending->tab,
                                                             pending->kind,
                                                             pending->max_results);
            lsp_debug(server,
                      "location response id=%u kind=%s locations=%u",
                      id,
                      pending->kind ? pending->kind : "LSP",
                      locations ? locations->len : 0u);
            if (pending->callback) {
                pending->callback(pending->tab, locations, pending->user_data);
            } else if (locations) {
                g_ptr_array_free(locations, TRUE);
            }
            lsp_location_pending_free(pending);
            return;
        }
        lsp_debug(server, "response id=%u had no matching pending request", id);
    } else if (json_object_has_member(object, "error")) {
        guint id = json_object_has_member(object, "id")
            ? (guint)json_object_get_int_member(object, "id") : 0u;
        JsonObject *error = json_object_get_object_member(object, "error");
        const char *message = error && json_object_has_member(error, "message")
            ? json_object_get_string_member(error, "message") : "unknown LSP error";
        gint code = error && json_object_has_member(error, "code")
            ? (gint)json_object_get_int_member(error, "code") : 0;
        lsp_debug(server, "error response id=%u code=%d message=%s", id, code, message);
    }
}

/**
 * @brief Read one framed LSP body after a Content-Length header.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param source The source supplied by the caller.
 * @param res The res supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void lsp_body_read_free(LspBodyRead *state) {
    if (!state) return;
    g_free(state->data);
    g_free(state);
}

/**
 * @brief Continue an exact Content-Length body read.
 * @details Pipe reads are allowed to return short chunks. ZLS often sends
 *          larger payloads than clangd for completion and diagnostics, so the
 *          reader must accumulate bytes until the declared body length is met.
 * @param source The source supplied by the caller.
 * @param res The res supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void lsp_server_read_body_cb(GObject *source, GAsyncResult *res, gpointer user_data) {
    LspBodyRead *state = user_data;
    LspServer *server = state ? state->server : NULL;
    g_autoptr(GError) error = NULL;
    gssize nread = g_input_stream_read_finish(G_INPUT_STREAM(source), res, &error);
    if (!server || error || nread <= 0) {
        lsp_body_read_free(state);
        if (server) {
            lsp_debug(server,
                      "body read failed: %s",
                      error ? error->message : "unexpected EOF");
            server->alive = FALSE;
        }
        return;
    }

    state->received += (gsize)nread;
    if (state->received < state->length) {
        lsp_server_read_body_next(state);
        return;
    }

    state->data[state->length] = '\0';
    lsp_server_handle_payload(server, state->data);
    lsp_body_read_free(state);
    lsp_server_read_next(server);
}

/**
 * @brief Queue the next chunk of an exact LSP body read.
 * @details The buffer is owned by LspBodyRead until either the full payload is
 *          parsed or an error cancels the server.
 * @param state Body read state with Content-Length and accumulated bytes.
 */
static void lsp_server_read_body_next(LspBodyRead *state) {
    if (!state || !state->server || !state->server->alive || !state->data) {
        lsp_body_read_free(state);
        return;
    }
    g_input_stream_read_async(G_INPUT_STREAM(state->server->output),
                              state->data + state->received,
                              state->length - state->received,
                              G_PRIORITY_LOW,
                              state->server->cancellable,
                              lsp_server_read_body_cb,
                              state);
}

/**
 * @brief Check whether an LSP header line is the blank body separator.
 * @details LSP framing uses CRLF, but GDataInputStream removes only the line
 *          feed. Some servers therefore hand us "\r" for the separator rather
 *          than an empty string. Treating CR, spaces, and tabs as blank keeps
 *          the reader from consuming the following JSON body as fake headers.
 * @param line Header line returned by GDataInputStream.
 * @return TRUE when the line separates headers from the JSON body.
 */
static gboolean lsp_header_line_is_blank(const char *line) {
    if (!line) return FALSE;
    for (const char *cursor = line; *cursor != '\0'; cursor++) {
        if (*cursor != '\r' && *cursor != ' ' && *cursor != '\t') {
            return FALSE;
        }
    }
    return TRUE;
}

/**
 * @brief Consume LSP header lines until the blank body separator.
 * @details The LSP stream is handled on the GTK main loop, so every read in
 *          the framing path must stay asynchronous. Some servers send extra
 *          headers after Content-Length; waiting for the empty line keeps those
 *          headers out of the JSON body without freezing the UI.
 * @param source The data input stream that owns the header read.
 * @param res The async result for the finished line read.
 * @param user_data The LspBodyRead state for the body that follows.
 */
static void lsp_server_read_separator_cb(GObject *source,
                                         GAsyncResult *res,
                                         gpointer user_data) {
    LspBodyRead *state = user_data;
    LspServer *server = state ? state->server : NULL;
    g_autoptr(GError) error = NULL;
    gsize line_len = 0u;
    char *line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(source),
                                                           res,
                                                           &line_len,
                                                           &error);
    if (!server || error || !line) {
        if (server) {
            lsp_debug(server,
                      "header separator read failed: %s",
                      error ? error->message : "no line");
            server->alive = FALSE;
        }
        g_free(line);
        lsp_body_read_free(state);
        return;
    }

    if (line_len == 0u || lsp_header_line_is_blank(line)) {
        g_free(line);
        lsp_server_read_body_next(state);
        return;
    }

    lsp_debug(server, "extra header ignored before body: %s", line);
    g_free(line);
    lsp_server_read_separator_next(state);
}

/**
 * @brief Queue the next async read while looking for the LSP blank separator.
 * @details This keeps the protocol reader non-blocking even when a server is
 *          slow to finish its headers.
 * @param state Body read state waiting for the separator before JSON bytes.
 */
static void lsp_server_read_separator_next(LspBodyRead *state) {
    if (!state || !state->server || !state->server->alive ||
        !state->server->output) {
        lsp_body_read_free(state);
        return;
    }
    g_data_input_stream_read_line_async(state->server->output,
                                        G_PRIORITY_LOW,
                                        state->server->cancellable,
                                        lsp_server_read_separator_cb,
                                        state);
}

/**
 * @brief Read one LSP header line.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param source The source supplied by the caller.
 * @param res The res supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void lsp_server_read_header_cb(GObject *source, GAsyncResult *res, gpointer user_data) {
    LspServer *server = user_data;
    g_autoptr(GError) error = NULL;
    gsize line_len = 0u;
    char *line = g_data_input_stream_read_line_finish_utf8(G_DATA_INPUT_STREAM(source),
                                                           res,
                                                           &line_len,
                                                           &error);
    if (!server || error || !line) {
        g_free(line);
        if (server) {
            lsp_debug(server,
                      "header read failed: %s",
                      error ? error->message : "no line");
            server->alive = FALSE;
        }
        return;
    }
    guint64 content_length = 0u;
    if (g_ascii_strncasecmp(line, "Content-Length:", 15) == 0) {
        content_length = g_ascii_strtoull(line + 15, NULL, 10);
    }
    g_free(line);
    if (content_length == 0u) {
        lsp_server_read_next(server);
        return;
    }
    LspBodyRead *state = g_new0(LspBodyRead, 1);
    if (!state) {
        server->alive = FALSE;
        return;
    }
    state->server = server;
    state->length = (gsize)content_length;
    state->data = g_malloc0(state->length + 1u);
    if (!state->data) {
        lsp_body_read_free(state);
        server->alive = FALSE;
        return;
    }
    lsp_server_read_separator_next(state);
}

/**
 * @brief Queue the next LSP header read.
 * @details LSP messages arrive as a header followed by a JSON body. Starting
 *          each read from this helper keeps cancellation and server-liveness
 *          checks in one place before async I/O owns the callback.
 * @param server The language server whose stdout stream should be read.
 */
static void lsp_server_read_next(LspServer *server) {
    if (!server || !server->alive || !server->output) return;
    g_data_input_stream_read_line_async(server->output,
                                        G_PRIORITY_LOW,
                                        server->cancellable,
                                        lsp_server_read_header_cb,
                                        server);
}

/**
 * @brief Spawn a server for syntax.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param win The win supplied by the caller.
 * @param syntax The syntax definition used by the editor path.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static LspServer *lsp_server_new(EditorWindow *win, SyntaxDef *syntax, const char *root_path) {
    if (!syntax_has_lsp(syntax)) return NULL;
    char **argv = lsp_server_argv(syntax);
    if (!argv) return NULL;
    g_autoptr(GError) error = NULL;
    GSubprocessFlags flags = G_SUBPROCESS_FLAGS_STDIN_PIPE |
        G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        (win && win->debug_mode ? G_SUBPROCESS_FLAGS_STDERR_PIPE
                                : G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(flags);
    if (root_path && root_path[0] != '\0') {
        g_subprocess_launcher_set_cwd(launcher, root_path);
    }
    GSubprocess *process = g_subprocess_launcher_spawnv(launcher,
                                                        (const char * const *)argv,
                                                        &error);
    g_object_unref(launcher);
    g_strfreev(argv);
    if (!process) {
        if (win && win->debug_mode) {
            g_message("LSP: failed to start %s: %s",
                      syntax->lsp_command,
                      error ? error->message : "unknown error");
        }
        return NULL;
    }

    LspServer *server = g_new0(LspServer, 1);
    if (!server) {
        g_object_unref(process);
        return NULL;
    }
    server->syntax = syntax;
    server->win = win;
    server->root_path = g_strdup(root_path && root_path[0] != '\0'
                                 ? root_path
                                 : g_get_home_dir());
    server->process = process;
    server->debug = win ? win->debug_mode : FALSE;
    server->input = g_object_ref(g_subprocess_get_stdin_pipe(process));
    server->output = g_data_input_stream_new(g_subprocess_get_stdout_pipe(process));
    if (server->debug) {
        server->error_output = g_data_input_stream_new(g_subprocess_get_stderr_pipe(process));
    }
    server->cancellable = g_cancellable_new();
    server->open_uris = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    server->completion_cache = g_hash_table_new_full(g_str_hash,
                                                     g_str_equal,
                                                     g_free,
                                                     completion_cache_value_free);
    server->completion_pending = g_hash_table_new_full(g_direct_hash,
                                                       g_direct_equal,
                                                       NULL,
                                                       lsp_completion_pending_free);
    server->references_pending = g_hash_table_new_full(g_direct_hash,
                                                       g_direct_equal,
                                                       NULL,
                                                       lsp_location_pending_free);
    server->definition_pending = g_hash_table_new_full(g_direct_hash,
                                                       g_direct_equal,
                                                       NULL,
                                                       lsp_location_pending_free);
    server->next_id = 1u;
    server->alive = TRUE;
    lsp_debug(server,
              "started command=%s language_id=%s root=%s stderr=%s",
              syntax->lsp_command ? syntax->lsp_command : "(none)",
              syntax_lsp_language_id(syntax),
              server->root_path ? server->root_path : "(none)",
              server->error_output ? "captured" : "silenced");
    lsp_server_send_initialize(server, win);
    lsp_server_read_next(server);
    lsp_server_read_stderr_next(server);
    return server;
}
