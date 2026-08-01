/**
 * @file run_active_file.c
 * @brief Native plugin that runs the active file.
 * @details The manifest owns trust and permissions. This shared library owns
 *          the command label, runner selection, process execution, and output.
 */

#include "plugin_api.h"

#include <glib/gstdio.h>
#include <string.h>

/**
 * @brief Runner command for one file type.
 */
typedef struct {
    const char *extension; /**< File extension including the dot. */
    const char *program; /**< Program used to run the file. */
} RunFileRunner;

/**
 * @brief Known extension runners.
 */
static const RunFileRunner RUNNERS[] = {
    { ".py", "python3" },
    { ".js", "node" },
    { ".mjs", "node" },
    { ".sh", "bash" },
    { ".bash", "bash" },
    { ".rb", "ruby" },
    { ".pl", "perl" },
    { ".lua", "lua" },
    { ".php", "php" },
};

/**
 * @brief Return a runner for a path extension.
 * @param path Active file path.
 * @return Program name, or NULL when no extension runner is known.
 */
static const char *runner_for_path(const char *path) {
    if (!path) return NULL;
    for (guint i = 0u; i < G_N_ELEMENTS(RUNNERS); i++) {
        if (g_str_has_suffix(path, RUNNERS[i].extension)) {
            return RUNNERS[i].program;
        }
    }
    return NULL;
}

/**
 * @brief Format process output for the result dialog.
 * @param path File path that was run.
 * @param program Program used to run it.
 * @param status Process wait status.
 * @param stdout_text Captured stdout.
 * @param stderr_text Captured stderr.
 * @return Newly allocated dialog body.
 */
static char *format_output(const char *path,
                           const char *program,
                           gint status,
                           const char *stdout_text,
                           const char *stderr_text) {
    return g_strdup_printf("Command: %s %s\nExit status: %d\n\nstdout:\n%s\n\nstderr:\n%s",
                           program ? program : "",
                           path ? path : "",
                           status,
                           stdout_text && stdout_text[0] ? stdout_text : "(empty)",
                           stderr_text && stderr_text[0] ? stderr_text : "(empty)");
}

/**
 * @brief Run the active file and show captured output.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void run_active_file(GraptosPluginCommandContext *context,
                            gpointer user_data) {
    (void)user_data;
    char *path = graptos_plugin_context_file_path(context);
    if (!path || !path[0]) {
        graptos_plugin_context_show_output(context,
                                           "Run Active File",
                                           "No Saved File",
                                           "Save the active tab before running it.");
        g_free(path);
        return;
    }

    const char *program = runner_for_path(path);
    gboolean executable = g_file_test(path, G_FILE_TEST_IS_EXECUTABLE);
    if (!program && !executable) {
        char *body = g_strdup_printf("No runner is configured for this file:\n%s\n\nSupported extensions: .py, .js, .mjs, .sh, .bash, .rb, .pl, .lua, .php\nExecutable files are run directly.",
                                     path);
        graptos_plugin_context_show_output(context,
                                           "Run Active File",
                                           "Unsupported File Type",
                                           body);
        g_free(body);
        g_free(path);
        return;
    }

    char *argv_with_runner[] = { (char *)program, path, NULL };
    char *argv_executable[] = { path, NULL };
    char **argv = program ? argv_with_runner : argv_executable;
    char *cwd = g_path_get_dirname(path);
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    GError *error = NULL;
    gint status = 0;
    gboolean ok = g_spawn_sync(cwd,
                               argv,
                               NULL,
                               G_SPAWN_SEARCH_PATH,
                               NULL,
                               NULL,
                               &stdout_text,
                               &stderr_text,
                               &status,
                               &error);
    if (!ok) {
        char *body = g_strdup_printf("Failed to run:\n%s\n\n%s",
                                     path,
                                     error ? error->message : "Unknown error");
        graptos_plugin_context_show_output(context,
                                           "Run Active File",
                                           "Run Failed",
                                           body);
        g_free(body);
        g_clear_error(&error);
    } else {
        char *body = format_output(path,
                                   program ? program : path,
                                   status,
                                   stdout_text,
                                   stderr_text);
        graptos_plugin_context_show_output(context,
                                           "Run Active File",
                                           "Run Output",
                                           body);
        g_free(body);
    }

    g_free(stdout_text);
    g_free(stderr_text);
    g_free(cwd);
    g_free(path);
}

/**
 * @brief Register this plugin with Graptoς.
 * @param host Host capability object supplied by Graptoς.
 * @return TRUE when registration succeeds.
 */
gboolean graptos_plugin_register(GraptosPluginHost *host) {
    if (graptos_plugin_host_api_version(host) != GRAPTOS_PLUGIN_API_VERSION) {
        return FALSE;
    }
    return graptos_plugin_host_register_editor_line_command(host,
                                                           "run-active-file",
                                                           "Run Active File",
                                                           run_active_file,
                                                           NULL,
                                                           NULL);
}
