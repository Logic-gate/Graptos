/**
 * @file beancount_tools.c
 * @brief Native plugin entry point for Beancount ledger editing.
 * @details Feature modules are included into one translation unit so plugin
 *          builds can select modules with compile-time flags while still
 *          producing one shared object.
 */

#include "beancount_tools_config.h"

#include "beancount_tools_common.c"

#if BEANCOUNT_TOOLS_ENABLE_COMPLETION
#include "beancount_tools_completion.c"
#endif

#if BEANCOUNT_TOOLS_ENABLE_HOVER
#include "beancount_tools_hover.c"
#endif

#if BEANCOUNT_TOOLS_ENABLE_COMMANDS
#include "beancount_tools_commands.c"
#endif

#if BEANCOUNT_TOOLS_ENABLE_REPORT
#include "beancount_tools_report.c"
#endif

/**
 * @brief Register this plugin with Graptoς.
 * @param host Host capability object supplied by Graptoς.
 * @return TRUE when registration succeeds.
 */
gboolean graptos_plugin_register(GraptosPluginHost *host) {
    if (graptos_plugin_host_api_version(host) != GRAPTOS_PLUGIN_API_VERSION) return FALSE;
    gboolean t_done = TRUE;
#if BEANCOUNT_TOOLS_ENABLE_COMPLETION
    t_done = graptos_plugin_host_register_completion_provider(host,
                                                          "beancount-completion",
                                                          "Beancount",
                                                          beancount_completion_candidates,
                                                          NULL,
                                                          NULL) && t_done;
    t_done = graptos_plugin_host_register_editor_line_command_with_shortcut(host,
                                                                        "beancount-complete",
                                                                        "Beancount Complete",
                                                                        "Ctrl+Shift+X",
                                                                        show_beancount_completion,
                                                                        NULL,
                                                                        NULL) && t_done;
#endif
#if BEANCOUNT_TOOLS_ENABLE_HOVER
    t_done = graptos_plugin_host_register_hover_provider(host,
                                                     "beancount-account-balance",
                                                     "Beancount balance",
                                                     account_balance_hover,
                                                     NULL,
                                                     NULL) && t_done;
#endif
#if BEANCOUNT_TOOLS_ENABLE_COMMANDS
    t_done = graptos_plugin_host_register_editor_line_command(host, "beancount-validate", "Validate Ledger", validate_ledger, NULL, NULL) && t_done;
    t_done = graptos_plugin_host_register_editor_line_command(host, "beancount-query", "Query Selection", query_selection, NULL, NULL) && t_done;
    t_done = graptos_plugin_host_register_editor_line_command(host, "beancount-summary", "Ledger Summary", ledger_summary, NULL, NULL) && t_done;
    t_done = graptos_plugin_host_register_editor_line_command(host, "beancount-format", "Format Ledger", format_ledger, NULL, NULL) && t_done;
    t_done = graptos_plugin_host_register_editor_line_command(host, "beancount-context", "Account Context", account_context, NULL, NULL) && t_done;
#endif
#if BEANCOUNT_TOOLS_ENABLE_REPORT
    t_done = graptos_plugin_host_register_editor_line_command(host, "beancount-balance-preview", "Balance Sheet Preview", balance_sheet_preview, NULL, NULL) && t_done;
    t_done = graptos_plugin_host_register_editor_line_command(host, "beancount-balance-export", "Export Balance Sheet PDF", export_balance_sheet_pdf, NULL, NULL) && t_done;
#endif
    return t_done;
}
