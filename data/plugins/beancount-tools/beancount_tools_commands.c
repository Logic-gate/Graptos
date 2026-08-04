/**
 * @brief Publish bean-check diagnostics from tool output.
 * @param context Command context supplied by Graptoς.
 * @param output Combined bean-check output.
 */
static void publish_check_diagnostics(GraptosPluginCommandContext *context,
                                      const char *output) {
    graptos_plugin_context_clear_diagnostics(context);
    char **lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        const char *p = lines[i];
        while ((p = strchr(p, ':')) != NULL) {
            p++;
            if (!g_ascii_isdigit(*p)) continue;
            guint line = 0u;
            while (g_ascii_isdigit(*p)) {
                line = line * 10u + (guint)(*p - '0');
                p++;
            }
            if (*p == ':' && line > 0u) {
                graptos_plugin_context_add_line_diagnostic(context, line, lines[i]);
                break;
            }
        }
    }
    g_strfreev(lines);
}

/**
 * @brief Run bean-check and publish line diagnostics.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void validate_ledger(GraptosPluginCommandContext *context, gpointer user_data) {
    (void)user_data;
    char *path = saved_beancount_path(context, "Validate Ledger");
    if (!path) return;
    if (!g_find_program_in_path("bean-check")) {
        graptos_plugin_context_show_output(context, "Validate Ledger", "bean-check Missing", "Install Beancount to use bean-check.");
        g_free(path);
        return;
    }
    char *cwd = g_path_get_dirname(path);
    char *argv[] = { "bean-check", path, NULL };
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    gint status = 0;
    (void)run_tool(cwd, argv, &stdout_text, &stderr_text, &status);
    char *combined = g_strconcat(stdout_text ? stdout_text : "", stderr_text ? stderr_text : "", NULL);
    publish_check_diagnostics(context, combined);
    char *body = status == 0 && (!combined || !combined[0])
        ? g_strdup("bean-check passed.")
        : format_tool_output("bean-check", status, stdout_text, stderr_text);
    graptos_plugin_context_show_output(context, "Validate Ledger", "Bean Check", body);
    g_free(body);
    g_free(combined);
    g_free(stdout_text);
    g_free(stderr_text);
    g_free(cwd);
    g_free(path);
}

/**
 * @brief Run a bean-query command.
 * @param context Command context supplied by Graptoς.
 * @param query Beancount query text.
 * @param title Dialog title.
 */
static void run_query(GraptosPluginCommandContext *context,
                      const char *query,
                      const char *title) {
    char *path = saved_beancount_path(context, title);
    if (!path) return;
    if (!g_find_program_in_path("bean-query")) {
        graptos_plugin_context_show_output(context, title, "bean-query Missing", "Install Beancount to use bean-query.");
        g_free(path);
        return;
    }
    char *cwd = g_path_get_dirname(path);
    char *argv[] = { "bean-query", path, (char *)query, NULL };
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    gint status = 0;
    (void)run_tool(cwd, argv, &stdout_text, &stderr_text, &status);
    char *body = format_tool_output(query, status, stdout_text, stderr_text);
    graptos_plugin_context_show_output(context, title, "Bean Query", body);
    g_free(body);
    g_free(stdout_text);
    g_free(stderr_text);
    g_free(cwd);
    g_free(path);
}

/**
 * @brief Query the selection or fall back to balances.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void query_selection(GraptosPluginCommandContext *context, gpointer user_data) {
    (void)user_data;
    char *selection = graptos_plugin_context_selection(context);
    g_strstrip(selection);
    run_query(context, selection && selection[0] ? selection : "BALANCES", "Query Selection");
    g_free(selection);
}

/**
 * @brief Show a compact ledger balance summary.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void ledger_summary(GraptosPluginCommandContext *context, gpointer user_data) {
    (void)user_data;
    run_query(context, "BALANCES", "Ledger Summary");
}

/**
 * @brief Replace the editor text with bean-format output.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void format_ledger(GraptosPluginCommandContext *context, gpointer user_data) {
    (void)user_data;
    char *path = saved_beancount_path(context, "Format Ledger");
    if (!path) return;
    if (!g_find_program_in_path("bean-format")) {
        graptos_plugin_context_show_output(context, "Format Ledger", "bean-format Missing", "Install Beancount to use bean-format.");
        g_free(path);
        return;
    }
    char *cwd = g_path_get_dirname(path);
    char *argv[] = { "bean-format", path, NULL };
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    gint status = 0;
    (void)run_tool(cwd, argv, &stdout_text, &stderr_text, &status);
    if (status == 0 && stdout_text && stdout_text[0]) {
        if (graptos_plugin_context_replace_text(context, stdout_text)) {
            graptos_plugin_context_set_status(context, "Beancount formatted");
        }
    } else {
        char *body = format_tool_output("bean-format", status, stdout_text, stderr_text);
        graptos_plugin_context_show_output(context, "Format Ledger", "Bean Format", body);
        g_free(body);
    }
    g_free(stdout_text);
    g_free(stderr_text);
    g_free(cwd);
    g_free(path);
}

/**
 * @brief Show bean-doctor context for the active line.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void account_context(GraptosPluginCommandContext *context, gpointer user_data) {
    (void)user_data;
    char *path = saved_beancount_path(context, "Account Context");
    if (!path) return;
    if (!g_find_program_in_path("bean-doctor")) {
        graptos_plugin_context_show_output(context, "Account Context", "bean-doctor Missing", "Install Beancount to use bean-doctor.");
        g_free(path);
        return;
    }
    char *line = g_strdup_printf("%u", graptos_plugin_context_line(context));
    char *cwd = g_path_get_dirname(path);
    char *argv[] = { "bean-doctor", "context", path, line, NULL };
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    gint status = 0;
    (void)run_tool(cwd, argv, &stdout_text, &stderr_text, &status);
    char *body = format_tool_output("bean-doctor context", status, stdout_text, stderr_text);
    graptos_plugin_context_show_output(context, "Account Context", "Bean Doctor", body);
    g_free(body);
    g_free(stdout_text);
    g_free(stderr_text);
    g_free(cwd);
    g_free(line);
    g_free(path);
}
