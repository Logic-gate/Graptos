/**
 * @brief Return whether a server command is configured.
 * @details LSP is useful, but it is not allowed to own the editor. Requests are
 *          asynchronous, answers can be stale, and local YAML/index behavior still needs to
 *          work when a server cannot help.
 * @param syntax The syntax definition used by the editor path.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean syntax_has_lsp(SyntaxDef *syntax) {
    return syntax && syntax->lsp_command && syntax->lsp_command[0] != '\0';
}

/**
 * @brief Return the language id for a syntax.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param syntax The syntax definition used by the editor path.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *syntax_lsp_language_id(SyntaxDef *syntax) {
    if (!syntax) return "plaintext";
    if (syntax->lsp_language_id && syntax->lsp_language_id[0] != '\0') {
        return syntax->lsp_language_id;
    }
    return syntax->name ? syntax->name : "plaintext";
}

/**
 * @brief Return whether the configured server is clangd.
 * @details Only clangd understands the fallbackFlags initialization option.
 *          Keeping that decision next to protocol construction prevents C/C++
 *          include fallback behavior from leaking into other language servers.
 * @param syntax The syntax definition used by the editor path.
 * @return TRUE when the configured LSP command resolves to clangd.
 */
static gboolean syntax_lsp_is_clangd(SyntaxDef *syntax) {
    if (!syntax || !syntax->lsp_command) return FALSE;
    g_autofree char *basename = g_path_get_basename(syntax->lsp_command);
    return g_strcmp0(basename, "clangd") == 0;
}

/**
 * @brief Add one include root to clangd fallback flags.
 * @details import_roots are editor-facing roots, so relative paths need to be
 *          made stable against the LSP workspace root before clangd sees them.
 *          clangd receives compiler-like flags, not Graptoς syntax keys.
 * @param flags Output array of owned flag strings.
 * @param root_path Workspace root used for relative roots.
 * @param root Include root from syntax YAML or an environment variable.
 */
static void lsp_add_include_flag(GPtrArray *flags,
                                 const char *root_path,
                                 const char *root) {
    if (!flags || !root || root[0] == '\0') return;

    g_autofree char *expanded = NULL;
    if (root[0] == '~') {
        const char *home = g_get_home_dir();
        if (home && root[1] == G_DIR_SEPARATOR) {
            expanded = g_build_filename(home, root + 2, NULL);
        } else if (home && root[1] == '\0') {
            expanded = g_strdup(home);
        }
    } else if (g_path_is_absolute(root)) {
        expanded = g_strdup(root);
    } else {
        g_autofree char *current_dir = NULL;
        const char *base = root_path && root_path[0] != '\0' ? root_path : NULL;
        if (!base) {
            current_dir = g_get_current_dir();
            base = current_dir;
        }
        expanded = g_build_filename(base, root, NULL);
    }
    if (!expanded || expanded[0] == '\0') return;

    g_autofree char *canonical = g_canonicalize_filename(expanded, NULL);
    if (!canonical || canonical[0] == '\0') return;
    if (!g_file_test(canonical, G_FILE_TEST_IS_DIR)) return;

    for (guint i = 0u; i < flags->len; i++) {
        const char *existing = g_ptr_array_index(flags, i);
        if (existing && g_str_has_prefix(existing, "-I") &&
            g_strcmp0(existing + 2, canonical) == 0) {
            return;
        }
    }

    g_ptr_array_add(flags, g_strdup_printf("-I%s", canonical));
}

/**
 * @brief Add include roots from one search-path environment variable.
 * @details Syntax YAML can name variables such as CPATH. Expanding them here
 *          keeps clangd fallback flags aligned with the same import roots that
 *          Graptoς uses for local import completion.
 * @param flags Output array of owned flag strings.
 * @param variable Environment variable name.
 */
static void lsp_add_include_env_flags(GPtrArray *flags, const char *variable) {
    if (!flags || !variable || variable[0] == '\0') return;
    const char *value = g_getenv(variable);
    if (!value || value[0] == '\0') return;

    char **parts = g_strsplit(value, G_SEARCHPATH_SEPARATOR_S, -1);
    for (guint i = 0u; parts && parts[i]; i++) {
        lsp_add_include_flag(flags, NULL, parts[i]);
    }
    g_strfreev(parts);
}

/**
 * @brief Build clangd fallback compiler flags from syntax import metadata.
 * @details import_roots still belong to Graptoς import completion. This bridge
 *          only mirrors C/C++ include roots into clangd's fallbackFlags so
 *          diagnostics work when a project has not supplied compile_commands.json.
 * @param syntax The syntax definition used by the editor path.
 * @param root_path Workspace root used for relative include roots.
 * @return Owned array of char* flags, or NULL when no fallback flags are needed.
 */
static GPtrArray *lsp_clangd_fallback_flags(SyntaxDef *syntax,
                                            const char *root_path) {
    if (!syntax_lsp_is_clangd(syntax)) return NULL;
    if (syntax->import_style &&
        g_ascii_strcasecmp(syntax->import_style, "c_include") != 0) {
        return NULL;
    }

    GPtrArray *flags = g_ptr_array_new_with_free_func(g_free);
    if (!flags) return NULL;

    for (guint i = 0u; syntax->import_roots && i < syntax->import_roots->len; i++) {
        const char *root = g_ptr_array_index(syntax->import_roots, i);
        if (!root || root[0] == '\0') continue;
        if (root[0] == '$') {
            lsp_add_include_env_flags(flags, root + 1);
        } else {
            lsp_add_include_flag(flags, root_path, root);
        }
    }

    for (guint i = 0u; syntax->import_env && i < syntax->import_env->len; i++) {
        lsp_add_include_env_flags(flags, g_ptr_array_index(syntax->import_env, i));
    }

    if (flags->len == 0u) {
        g_ptr_array_free(flags, TRUE);
        return NULL;
    }
    return flags;
}

/**
 * @brief Build argv for a configured LSP command.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param syntax The syntax definition used by the editor path.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char **lsp_server_argv(SyntaxDef *syntax) {
    if (!syntax_has_lsp(syntax)) return NULL;
    guint argc = 1u + (syntax->lsp_args ? syntax->lsp_args->len : 0u);
    char **argv = g_new0(char *, argc + 1u);
    if (!argv) return NULL;
    argv[0] = g_strdup(syntax->lsp_command);
    for (guint i = 0u; syntax->lsp_args && i < syntax->lsp_args->len; i++) {
        argv[i + 1u] = g_strdup(g_ptr_array_index(syntax->lsp_args, i));
    }
    return argv;
}

/**
 * @brief Write a framed JSON-RPC message to a server.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @param node The node supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean lsp_server_write_json(LspServer *server, JsonNode *node) {
    if (!server || !server->input || !node) return FALSE;
    g_autoptr(JsonGenerator) generator = json_generator_new();
    json_generator_set_root(generator, node);
    g_autofree char *json = json_generator_to_data(generator, NULL);
    if (!json) return FALSE;
    g_autofree char *header = g_strdup_printf("Content-Length: %zu\r\n\r\n",
                                              strlen(json));
    g_autoptr(GError) error = NULL;
    gboolean ok = g_output_stream_write_all(server->input,
                                            header,
                                            strlen(header),
                                            NULL,
                                            server->cancellable,
                                            &error) &&
        g_output_stream_write_all(server->input,
                                  json,
                                  strlen(json),
                                  NULL,
                                  server->cancellable,
                                  &error);
    if (!ok) {
        lsp_debug(server,
                  "write failed: %s",
                  error ? error->message : "unknown error");
        server->alive = FALSE;
    }
    return ok;
}

/**
 * @brief Create a JSON-RPC base object.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param method The method supplied by the caller.
 * @param id The id supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static JsonBuilder *lsp_builder_base(const char *method, guint64 id) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "jsonrpc");
    json_builder_add_string_value(builder, "2.0");
    if (id != 0u) {
        json_builder_set_member_name(builder, "id");
        json_builder_add_int_value(builder, (gint64)id);
    }
    json_builder_set_member_name(builder, "method");
    json_builder_add_string_value(builder, method);
    return builder;
}

/**
 * @brief Create a JSON-RPC response base object.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param id_node The id node supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static JsonBuilder *lsp_response_builder(JsonNode *id_node) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "jsonrpc");
    json_builder_add_string_value(builder, "2.0");
    json_builder_set_member_name(builder, "id");
    json_builder_add_value(builder, id_node ? json_node_copy(id_node) : json_node_new(JSON_NODE_NULL));
    return builder;
}

/**
 * @brief Reply to a server request that expects a minimal client response.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @param id_node The id node supplied by the caller.
 * @param method The method supplied by the caller.
 * @param params The params supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean lsp_server_reply_to_request(LspServer *server,
                                            JsonNode *id_node,
                                            const char *method,
                                            JsonObject *params) {
    if (!server || !id_node || !method) return FALSE;
    g_autoptr(JsonBuilder) builder = lsp_response_builder(id_node);
    json_builder_set_member_name(builder, "result");
    if (strcmp(method, "workspace/configuration") == 0) {
        JsonArray *items = params && json_object_has_member(params, "items")
            ? json_object_get_array_member(params, "items") : NULL;
        guint count = items ? json_array_get_length(items) : 0u;
        json_builder_begin_array(builder);
        for (guint i = 0u; i < count; i++) {
            json_builder_add_null_value(builder);
        }
        json_builder_end_array(builder);
    } else {
        json_builder_add_null_value(builder);
    }
    json_builder_end_object(builder);
    g_autoptr(JsonNode) node = json_builder_get_root(builder);
    lsp_debug(server, "reply request method=%s", method);
    return lsp_server_write_json(server, node);
}

/**
 * @brief Send a no-param notification.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 */
static void lsp_server_send_initialized(LspServer *server) {
    lsp_debug(server, "send initialized notification");
    g_autoptr(JsonBuilder) builder = lsp_builder_base("initialized", 0u);
    json_builder_set_member_name(builder, "params");
    json_builder_begin_object(builder);
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    g_autoptr(JsonNode) node = json_builder_get_root(builder);
    (void)lsp_server_write_json(server, node);
}

/**
 * @brief Send initialize request.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @param win The win supplied by the caller.
 */
static void lsp_server_send_initialize(LspServer *server, EditorWindow *win) {
    (void)win;
    if (!server || server->initialized) return;
    server->initialized = TRUE;
    guint64 id = server->next_id++;
    server->initialize_id = id;
    lsp_debug(server,
              "send initialize id=%" G_GUINT64_FORMAT " root=%s command=%s",
              id,
              server->root_path ? server->root_path : g_get_home_dir(),
              server->syntax && server->syntax->lsp_command
                  ? server->syntax->lsp_command : "(none)");
    g_autoptr(JsonBuilder) builder = lsp_builder_base("initialize", id);
    json_builder_set_member_name(builder, "params");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "processId");
    json_builder_add_int_value(builder, (gint64)getpid());
    json_builder_set_member_name(builder, "clientInfo");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "name");
    json_builder_add_string_value(builder, "Graptoς");
    json_builder_end_object(builder);
    g_autofree char *root_uri = lsp_path_to_uri(server->root_path ? server->root_path : g_get_home_dir());
    json_builder_set_member_name(builder, "rootPath");
    json_builder_add_string_value(builder, server->root_path ? server->root_path : g_get_home_dir());
    json_builder_set_member_name(builder, "rootUri");
    json_builder_add_string_value(builder, root_uri ? root_uri : "");
    json_builder_set_member_name(builder, "workspaceFolders");
    json_builder_begin_array(builder);
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "uri");
    json_builder_add_string_value(builder, root_uri ? root_uri : "");
    json_builder_set_member_name(builder, "name");
    g_autofree char *workspace_name = server->root_path ? g_path_get_basename(server->root_path) : NULL;
    json_builder_add_string_value(builder, workspace_name ? workspace_name : "workspace");
    json_builder_end_object(builder);
    json_builder_end_array(builder);
    g_autoptr(GPtrArray) fallback_flags =
        lsp_clangd_fallback_flags(server->syntax, server->root_path);
    if (fallback_flags && fallback_flags->len > 0u) {
        lsp_debug(server,
                  "clangd fallbackFlags from import_roots count=%u",
                  fallback_flags->len);
        json_builder_set_member_name(builder, "initializationOptions");
        json_builder_begin_object(builder);
        json_builder_set_member_name(builder, "fallbackFlags");
        json_builder_begin_array(builder);
        for (guint i = 0u; i < fallback_flags->len; i++) {
            json_builder_add_string_value(builder, g_ptr_array_index(fallback_flags, i));
        }
        json_builder_end_array(builder);
        json_builder_end_object(builder);
    }
    json_builder_set_member_name(builder, "capabilities");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "workspace");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "configuration");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "textDocument");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "publishDiagnostics");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "relatedInformation");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_end_object(builder);
    json_builder_set_member_name(builder, "completion");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "contextSupport");
    json_builder_add_boolean_value(builder, TRUE);
    json_builder_set_member_name(builder, "completionItem");
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "snippetSupport");
    json_builder_add_boolean_value(builder, FALSE);
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    json_builder_end_object(builder);
    g_autoptr(JsonNode) node = json_builder_get_root(builder);
    (void)lsp_server_write_json(server, node);
}

/**
 * @brief Start reading server messages.
 */
