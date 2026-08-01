/**
 * @file src/plugin_api.h
 * @brief Public ABI surface for trusted native Graptoς plugins.
 * @details Native plugins must use this small host object instead of reaching
 *          into EditorWindow, EditorTab, or GTK internals. Keeping the ABI
 *          opaque lets Graptoς change editor implementation details without
 *          breaking plugin source.
 */

#ifndef GRAPTOS_PLUGIN_API_H
#define GRAPTOS_PLUGIN_API_H

#include <glib.h>

/**
 * @brief Graptoς plugin API version.
 */
#define GRAPTOS_PLUGIN_API_VERSION 1u

/**
 * @brief Opaque host passed to native plugins.
 */
typedef struct _GraptosPluginHost GraptosPluginHost;

/**
 * @brief Native plugin registration function.
 * @details Shared libraries export this symbol when they need code-backed
 *          editor automation. Declarative-only plugins do not need a native
 *          entrypoint.
 * @param host The host capability object supplied by Graptoς.
 * @return TRUE when registration succeeds.
 */
typedef gboolean (*GraptosPluginRegisterFunc)(GraptosPluginHost *host);

/**
 * @brief Return the plugin API version implemented by the host.
 * @details Plugins can use this during registration to reject incompatible
 *          Graptoς builds before registering commands.
 * @param host The host capability object supplied by Graptoς.
 * @return The integer API version.
 */
guint graptos_plugin_host_api_version(GraptosPluginHost *host);

/**
 * @brief Return the id of the plugin currently being registered.
 * @details The id is owned by Graptoς and remains valid only while the host is
 *          alive. Plugins should copy it if they need to keep it.
 * @param host The host capability object supplied by Graptoς.
 * @return The plugin id, or NULL when unavailable.
 */
const char *graptos_plugin_host_plugin_id(GraptosPluginHost *host);

#endif /* GRAPTOS_PLUGIN_API_H */
