/**
 * @brief Find an open editor tab for a local file path.
 * @details LSP is useful, but it is not allowed to own the editor. Requests are
 *          asynchronous, answers can be stale, and local YAML/index behavior still needs to
 *          work when a server cannot help.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static EditorTab *lsp_find_tab_for_path(EditorWindow *win, const char *path) {
    if (!win || !win->notebook || !path) return NULL;
    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        EditorTab *tab = child ? g_object_get_data(G_OBJECT(child), "graptos-tab") : NULL;
        if (tab && tab->file_path && strcmp(tab->file_path, path) == 0) return tab;
    }
    return NULL;
}

// static EditorTab *lsp_find_tab_for_path(EditorWindow *win, const char *path) {
    // if (!win || !win->notebook || !path) return NULL;
// 
    // GtkNotebook *notebook = GTK_NOTEBOOK(win->notebook);
    // gint page_count = gtk_notebook_get_n_pages(notebook);
    // gint page_index = 1;
// 
    // while (page_index < page_count) {
        // GtkWidget *page = gtk_notebook_get_nth_page(notebook, page_index);
        // EditorTab *tab = page ? g_object_get_data(G_OBJECT(page), "graptos-tab") : NULL;
// 
        // if (tab && g_strcmp0(tab->file_path, path) == 0) return tab;
        // page_index++;
    // }
// 
    // return NULL;
// }


/**
 * @brief Apply LSP diagnostics to an open tab.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @param uri The document URI used for LSP or protocol routing.
 * @param diagnostics The diagnostics supplied by the caller.
 */
static void lsp_apply_publish_diagnostics(LspServer *server,
                                          const char *uri,
                                          JsonArray *diagnostics) {
    if (!server || !uri || !g_str_has_prefix(uri, "file://")) return;
    g_autofree char *path = g_filename_from_uri(uri, NULL, NULL);
    EditorTab *tab = lsp_find_tab_for_path(server->win, path);
    if (tab && server->win && !server->win->diagnostics_enabled) {
        clear_syntax_diagnostics(tab);
        editor_tab_schedule_lightweight_ui_refresh(tab);
        lsp_debug(server, "publishDiagnostics ignored because diagnostics are disabled uri=%s", uri);
        return;
    }
    if (!tab || !diagnostics) {
        lsp_debug(server,
                  "publishDiagnostics ignored uri=%s path=%s tab_found=%d diagnostics=%d",
                  uri,
                  path ? path : "(none)",
                  tab ? 1 : 0,
                  diagnostics ? 1 : 0);
        return;
    }

    clear_syntax_diagnostics(tab);
    guint length = json_array_get_length(diagnostics);
    guint applied = 0u;
    guint skipped = 0u;
    gboolean server_error_limit_reached = FALSE;
    for (guint i = 0u; i < length; i++) {
        JsonNode *node = json_array_get_element(diagnostics, i);
        if (!node || !JSON_NODE_HOLDS_OBJECT(node)) {
            skipped++;
            continue;
        }
        JsonObject *diagnostic = json_node_get_object(node);
        JsonObject *range = json_object_has_member(diagnostic, "range")
            ? json_object_get_object_member(diagnostic, "range") : NULL;
        JsonObject *start = range && json_object_has_member(range, "start")
            ? json_object_get_object_member(range, "start") : NULL;
        JsonObject *end = range && json_object_has_member(range, "end")
            ? json_object_get_object_member(range, "end") : NULL;
        if (!start || !end) {
            skipped++;
            continue;
        }
        const char *message = json_object_has_member(diagnostic, "message")
            ? json_object_get_string_member(diagnostic, "message")
            : "Language server diagnostic";
        if (message &&
            g_strrstr(message, "too many errors emitted") &&
            g_strrstr(message, "stopping now")) {
            lsp_debug(server, "publishDiagnostics skipped summary diagnostic: %s", message);
            server_error_limit_reached = TRUE;
            skipped++;
            continue;
        }
        const char *source = json_object_has_member(diagnostic, "source")
            ? json_object_get_string_member(diagnostic, "source")
            : "LSP";
        gint severity = json_object_has_member(diagnostic, "severity")
            ? (gint)json_object_get_int_member(diagnostic, "severity")
            : 0;
        const char *severity_label = severity == 1 ? "Error" :
            severity == 2 ? "Warning" :
            severity == 3 ? "Info" :
            severity == 4 ? "Hint" : "Diagnostic";
        g_autofree char *full_message = g_strdup_printf("%s %s: %s",
                                                        source,
                                                        severity_label,
                                                        message);
        gint start_line = json_object_has_member(start, "line")
            ? (gint)json_object_get_int_member(start, "line") : 0;
        gint start_character = json_object_has_member(start, "character")
            ? (gint)json_object_get_int_member(start, "character") : 0;
        gint end_line = json_object_has_member(end, "line")
            ? (gint)json_object_get_int_member(end, "line") : start_line;
        gint end_character = json_object_has_member(end, "character")
            ? (gint)json_object_get_int_member(end, "character") : start_character + 1;
        if (editor_tab_apply_external_diagnostic(tab,
                                                 start_line,
                                                 start_character,
                                                 end_line,
                                                 end_character,
                                                 full_message ? full_message : message)) {
            applied++;
        } else {
            skipped++;
        }
    }
    if (server_error_limit_reached) {
        app_window_set_status(server->win,
                              "LSP stopped diagnostics after too many errors; fix earlier errors to see later diagnostics");
    }
    lsp_debug(server,
              "publishDiagnostics applied path=%s incoming=%u applied=%u skipped=%u server_error_limit=%d",
              path ? path : "(none)",
              length,
              applied,
              skipped,
              server_error_limit_reached ? 1 : 0);
    editor_tab_schedule_lightweight_ui_refresh(tab);
}

/**
 * @brief Emit a debug-mode LSP log message.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @param format The format supplied by the caller.
 */
static void lsp_debug(LspServer *server, const char *format, ...) {
    if (!server || !server->debug || !format) return;

    va_list args;
    va_start(args, format);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
    char *message = g_strdup_vprintf(format, args);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    va_end(args);

    if (message) {
        const char *syntax = server->syntax && server->syntax->name
            ? server->syntax->name
            : "unknown";
        g_message("LSP[%s]: %s", syntax, message);
        g_free(message);
    }
}

/**
 * @brief Emit an LSP debug log when no server object is available.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param format The format supplied by the caller.
 */
static void lsp_client_debug(LspClient *client, const char *format, ...) {
    if (!client || !client->win || !client->win->debug_mode || !format) return;

    va_list args;
    va_start(args, format);
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
    char *message = g_strdup_vprintf(format, args);
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
    va_end(args);

    if (message) {
        g_message("LSP: %s", message);
        g_free(message);
    }
}
/**
 * @brief Free a cached completion array.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param data The callback context passed by the caller.
 */
static void completion_cache_value_free(gpointer data) {
    GPtrArray *array = data;
    if (array) g_ptr_array_free(array, TRUE);
}

/**
 * @brief Clear cached and pending completion state for one LSP server.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 */
static void lsp_server_clear_completion_state(LspServer *server) {
    if (!server) return;
    if (server->completion_cache) g_hash_table_remove_all(server->completion_cache);
    if (server->completion_pending) g_hash_table_remove_all(server->completion_pending);
}

/**
 * @brief Clear delayed LSP completion retry state for a tab.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
static void lsp_tab_clear_completion_retry(EditorTab *tab) {
    if (!tab) return;
    graptos_source_cancel(&tab->lsp_completion_retry_timeout);
    tab->lsp_completion_retry_count = 0u;
    g_clear_pointer(&tab->lsp_completion_retry_key, g_free);
}

/**
 * @brief Free a pending LSP completion request.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param data The callback context passed by the caller.
 */
static void lsp_completion_pending_free(gpointer data) {
    LspCompletionPending *pending = data;
    if (!pending) return;
    g_free(pending->uri);
    g_free(pending->cache_key);
    g_free(pending->prefix);
    g_free(pending);
}

/**
 * @brief Free delayed LSP completion retry state.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param data The callback context passed by the caller.
 */
static void lsp_completion_retry_free(gpointer data) {
    LspCompletionRetry *retry = data;
    if (!retry) return;
    g_free(retry->cache_key);
    g_free(retry);
}

/**
 * @brief Run one delayed LSP completion retry.
 * @details Some servers answer completion before they have digested the latest
 *          didChange. We retry only while the buffer version and cache key still
 *          match; otherwise an old empty answer would keep resurrecting itself.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean lsp_completion_retry_timeout_cb(gpointer user_data) {
    LspCompletionRetry *retry = user_data;
    EditorTab *tab = retry ? retry->tab : NULL;
    if (!tab) return G_SOURCE_REMOVE;

    tab->lsp_completion_retry_timeout = 0u;
    if (!tab->autocomplete_enabled ||
        tab->lsp_version != retry->version ||
        g_strcmp0(tab->lsp_completion_retry_key, retry->cache_key) != 0) {
        if (tab->win && tab->win->debug_mode) {
            g_message("LSP completion retry cancelled: autocomplete=%d version=%u expected=%u key_match=%d",
                      tab->autocomplete_enabled ? 1 : 0,
                      tab->lsp_version,
                      retry->version,
                      g_strcmp0(tab->lsp_completion_retry_key, retry->cache_key) == 0 ? 1 : 0);
        }
        tab->lsp_completion_retry_count = 0u;
        g_clear_pointer(&tab->lsp_completion_retry_key, g_free);
        return G_SOURCE_REMOVE;
    }

    if (tab->win && tab->win->debug_mode) {
        g_message("LSP completion retry fired key=%s count=%u",
                  retry->cache_key ? retry->cache_key : "",
                  tab->lsp_completion_retry_count);
    }
    editor_tab_show_completion(tab, retry->manual);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Schedule a bounded retry for an empty LSP completion response.
 * @details Empty completion is sometimes a timing problem, not a real answer.
 *          The retry count is capped because a language server that keeps
 *          returning nothing should not turn autocomplete into a loop.
 * @param server The server supplied by the caller.
 * @param pending The pending supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean lsp_schedule_empty_completion_retry(LspServer *server,
                                                   LspCompletionPending *pending) {
    if (!server || !pending || !pending->tab || !pending->cache_key) return FALSE;
    EditorTab *tab = pending->tab;
    guint max_retries = server->win
        ? server->win->lsp_completion_max_retries
        : GRAPTOS_LSP_COMPLETION_MAX_RETRIES;
    guint retry_delay = server->win && server->win->lsp_completion_retry_delay_ms > 0u
        ? server->win->lsp_completion_retry_delay_ms
        : GRAPTOS_LSP_COMPLETION_RETRY_DELAY_MS;
    if (!tab->autocomplete_enabled ||
        pending->retry_count >= max_retries) {
        return FALSE;
    }

    LspCompletionRetry *retry = g_new0(LspCompletionRetry, 1);
    if (!retry) return FALSE;
    retry->tab = tab;
    retry->cache_key = g_strdup(pending->cache_key);
    retry->version = pending->version;
    retry->manual = pending->manual;

    graptos_source_cancel(&tab->lsp_completion_retry_timeout);
    g_free(tab->lsp_completion_retry_key);
    tab->lsp_completion_retry_key = g_strdup(pending->cache_key);
    tab->lsp_completion_retry_count = pending->retry_count + 1u;
    tab->lsp_completion_retry_timeout =
        g_timeout_add_full(G_PRIORITY_LOW,
                           retry_delay,
                           lsp_completion_retry_timeout_cb,
                           retry,
                           lsp_completion_retry_free);
    lsp_debug(server,
              "completion empty; retry %u/%u scheduled key=%s",
              tab->lsp_completion_retry_count,
              max_retries,
              pending->cache_key);
    return TRUE;
}

/**
 * @brief Free a pending LSP location request.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param data The callback context passed by the caller.
 */
static void lsp_location_pending_free(gpointer data) {
    LspLocationPending *pending = data;
    if (!pending) return;
    g_free(pending->kind);
    g_free(pending);
}

/**
 * @brief Free one LSP server.
 * @details Shutdown can happen while requests, retries, and stdio reads are
 *          still pending. We cancel first, force the process down, then release
 *          caches and pending tables so callbacks do not keep half-dead state.
 * @param data The callback context passed by the caller.
 */
static void lsp_server_free(gpointer data) {
    LspServer *server = data;
    if (!server) return;
    if (server->cancellable) g_cancellable_cancel(server->cancellable);
    if (server->process) {
        g_subprocess_force_exit(server->process);
        g_object_unref(server->process);
    }
    if (server->output) g_object_unref(server->output);
    if (server->error_output) g_object_unref(server->error_output);
    if (server->input) g_object_unref(server->input);
    if (server->cancellable) g_object_unref(server->cancellable);
    if (server->open_uris) g_hash_table_destroy(server->open_uris);
    if (server->completion_cache) g_hash_table_destroy(server->completion_cache);
    if (server->completion_pending) g_hash_table_destroy(server->completion_pending);
    if (server->references_pending) g_hash_table_destroy(server->references_pending);
    if (server->definition_pending) g_hash_table_destroy(server->definition_pending);
    g_free(server->root_path);
    g_free(server);
}
