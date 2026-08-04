/**
 * @file full_demo.c
 * @brief Native command for the full demo plugin.
 * @details The full demo manifest declares every current permission. This
 *          shared library keeps the native part real so startup does not emit a
 *          missing-library warning and plugin authors can see the full shape.
 */

#include "plugin_api.h"

gboolean graptos_plugin_register(GraptosPluginHost *host);

/**
 * @brief Show a compact native plugin status report.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void full_demo_native_command(GraptosPluginCommandContext *context,
                                     gpointer user_data) {
    (void)user_data;
    char *path = graptos_plugin_context_file_path(context);
    const char *syntax = graptos_plugin_context_syntax_name(context);
    char *body = g_strdup_printf("Plugin: %s\nCommand: %s\nLine: %u\nSyntax: %s\nModified: %s\nTabs: %u\nFile: %s",
                                 graptos_plugin_context_plugin_id(context),
                                 graptos_plugin_context_command_id(context),
                                 graptos_plugin_context_line(context),
                                 syntax ? syntax : "(none)",
                                 graptos_plugin_context_is_modified(context) ? "yes" : "no",
                                 graptos_plugin_context_tab_count(context),
                                 path ? path : "(unsaved)");
    graptos_plugin_context_show_output(context,
                                       "Full Demo Native",
                                       "Native Plugin Info",
                                       body);
    g_free(body);
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
                                                           "native-demo-command",
                                                           "Full Demo Native",
                                                           full_demo_native_command,
                                                           NULL,
                                                           NULL);
}
