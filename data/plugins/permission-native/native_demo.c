/**
 * @file native_demo.c
 * @brief Example native Graptoς plugin.
 * @details The manifest owns identity and permissions. This shared library
 *          owns the command label and behavior.
 */

#include "plugin_api.h"

/**
 * @brief Show text for the active editor line.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void native_demo_line_info(GraptosPluginCommandContext *context,
                                  gpointer user_data) {
    (void)user_data;
    guint line = graptos_plugin_context_line(context);
    char *text = graptos_plugin_context_line_text(context, line);
    char *body = g_strdup_printf("Line: %u\n\n%s", line, text ? text : "");
    graptos_plugin_context_show_output(context,
                                       "Native Demo",
                                       "Native Line Info",
                                       body);
    g_free(body);
    g_free(text);
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
                                                           "native-demo-line-info",
                                                           "Native Demo Line Info",
                                                           native_demo_line_info,
                                                           NULL,
                                                           NULL);
}
