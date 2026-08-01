/**
 * @file src/plugin.h
 * @brief Graptoς plugin manifest, registry, and contribution API.
 * @details The plugin layer is the stable boundary between Graptoς internals
 *          and extensions. Plugins contribute data through manifests and may
 *          later register native behavior through the opaque host ABI.
 */

#ifndef GRAPTOS_PLUGIN_H
#define GRAPTOS_PLUGIN_H

#include <gtk/gtk.h>

#include "plugin_api.h"

typedef struct _EditorTab EditorTab;

/**
 * @brief Plugin error domain macro.
 */
#define GRAPTOS_PLUGIN_ERROR (graptos_plugin_error_quark())

/**
 * @brief Plugin loading failure codes.
 */
typedef enum {
    GRAPTOS_PLUGIN_ERROR_INVALID_SCHEMA,
    GRAPTOS_PLUGIN_ERROR_INVALID_ID,
    GRAPTOS_PLUGIN_ERROR_UNSUPPORTED_API,
    GRAPTOS_PLUGIN_ERROR_IO,
    GRAPTOS_PLUGIN_ERROR_NATIVE
} GraptosPluginError;

/**
 * @brief Plugin contribution kind.
 */
typedef enum {
    GRAPTOS_PLUGIN_CONTRIBUTION_SYNTAX,
    GRAPTOS_PLUGIN_CONTRIBUTION_THEME,
    GRAPTOS_PLUGIN_CONTRIBUTION_TEMPLATE,
    GRAPTOS_PLUGIN_CONTRIBUTION_SNIPPET,
    GRAPTOS_PLUGIN_CONTRIBUTION_COMMAND,
    GRAPTOS_PLUGIN_CONTRIBUTION_MENU
} GraptosPluginContributionKind;

/**
 * @brief Parsed plugin manifest.
 */
typedef struct {
    char *id; /**< Stable plugin id. */
    char *name; /**< Human-facing plugin name. */
    char *version; /**< Plugin version string. */
    char *description; /**< Human-facing summary. */
    char *base_path; /**< Canonical plugin root directory. */
    char *native_library; /**< Optional native library path relative to base. */
    guint graptos_api_version; /**< Required plugin API version. */
    gboolean enabled; /**< FALSE when manifest disables the plugin. */
    GPtrArray *permissions; /**< char*. */
    GPtrArray *syntax_dirs; /**< char*. */
    GPtrArray *theme_dirs; /**< char*. */
    GPtrArray *template_dirs; /**< char*. */
    GPtrArray *snippet_dirs; /**< char*. */
    GPtrArray *commands; /**< char*. */
    GPtrArray *menus; /**< char*. */
    gpointer native_handle; /**< Private GModule handle. */
} GraptosPlugin;

/**
 * @brief Plugin registry.
 */
typedef struct GraptosPluginRegistry {
    GPtrArray *plugins; /**< GraptosPlugin*. */
} GraptosPluginRegistry;

/**
 * @brief Return the plugin error quark.
 * @details Callers use the domain to distinguish manifest, API-version, IO,
 *          and native loader failures from generic GLib errors.
 * @return The plugin error domain.
 */
GQuark graptos_plugin_error_quark(void);
/**
 * @brief Load one plugin manifest.
 * @details The loader parses `plugin.yaml` from a plugin root and validates the
 *          stable id and API version before exposing any contribution paths.
 * @param plugin_dir Directory containing `plugin.yaml`.
 * @param error Return location for parser or validation errors.
 * @return A parsed plugin manifest, or NULL on failure.
 */
GraptosPlugin *graptos_plugin_load_manifest(const char *plugin_dir,
                                            GError **error);
/**
 * @brief Free a plugin manifest.
 * @details This also closes a native module if one was explicitly loaded.
 * @param plugin Plugin manifest to free.
 */
void graptos_plugin_free(GraptosPlugin *plugin);
/**
 * @brief Create an empty plugin registry.
 * @details The registry owns loaded plugin manifests and is the handoff point
 *          between discovery and Graptoς subsystems that consume contributions.
 * @return New registry, or NULL on allocation failure.
 */
GraptosPluginRegistry *graptos_plugin_registry_new(void);
/**
 * @brief Free a plugin registry.
 * @details All plugin manifests owned by the registry are released.
 * @param registry Registry to free.
 */
void graptos_plugin_registry_free(GraptosPluginRegistry *registry);
/**
 * @brief Discover installed plugins.
 * @details Discovery scans user, system, and development plugin roots. User
 *          plugins win on duplicate ids because they are scanned first.
 * @param registry Registry receiving discovered plugins.
 * @param error Reserved for fatal discovery errors.
 * @return TRUE when discovery completed.
 */
gboolean graptos_plugin_registry_discover(GraptosPluginRegistry *registry,
                                          GError **error);
/**
 * @brief Return contribution directories of one kind.
 * @details Relative contribution paths are resolved against each plugin root
 *          and only existing directories are returned.
 * @param registry Registry to inspect.
 * @param kind Contribution kind.
 * @return Owned string array of canonical directories.
 */
GPtrArray *graptos_plugin_registry_contribution_dirs(GraptosPluginRegistry *registry,
                                                    GraptosPluginContributionKind kind);
/**
 * @brief Load native plugin libraries.
 * @details Native code is not loaded by normal discovery. Callers must invoke
 *          this explicitly after permission policy has approved native loading.
 * @param registry Registry containing parsed plugin manifests.
 * @param error Return location for native loader errors.
 * @return TRUE when all requested native plugins registered successfully.
 */
gboolean graptos_plugin_registry_load_native(GraptosPluginRegistry *registry,
                                             GError **error);
/**
 * @brief Append plugin editor context menu items.
 * @details Plugin menus are declarative rows that map a label to a registered
 *          command id. The editor passes the current line so commands can act
 *          on the right-click location without direct access to private state.
 * @param registry Plugin registry to inspect.
 * @param tab Active editor tab.
 * @param menu_box Context menu box receiving button widgets.
 * @param popover Popover to close before running the command.
 * @param line One-based line number under the pointer.
 */
void graptos_plugin_append_editor_context_items(GraptosPluginRegistry *registry,
                                                EditorTab *tab,
                                                GtkWidget *menu_box,
                                                GtkWidget *popover,
                                                guint line);

#endif /* GRAPTOS_PLUGIN_H */
