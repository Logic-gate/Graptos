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
    gboolean default_enabled; /**< Manifest enable value before user overrides. */
    GPtrArray *permissions; /**< char*. */
    GPtrArray *syntax_dirs; /**< char*. */
    GPtrArray *theme_dirs; /**< char*. */
    GPtrArray *template_dirs; /**< char*. */
    GPtrArray *snippet_dirs; /**< char*. */
    GPtrArray *commands; /**< char*. */
    GPtrArray *menus; /**< char*. */
    gpointer native_handle; /**< Private GModule handle. */
    GHashTable *native_commands; /**< char* to native command handler. */
    GPtrArray *native_completion_providers; /**< Native completion providers. */
    GPtrArray *native_hover_providers; /**< Native hover providers. */
    GPtrArray *native_hubs; /**< Native hub providers. */
} GraptosPlugin;

/**
 * @brief A lightweight plugin hub entry for UI callers.
 */
typedef struct {
    char *plugin_id; /**< Plugin id that owns the hub. */
    char *hub_id; /**< Hub id inside the plugin. */
    char *label; /**< Human-facing hub label. */
} GraptosPluginHubView;

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
 * @brief Apply persisted user enable and disable choices.
 * @details Manifest `enabled:` remains the plugin default. User choices are
 *          stored outside plugin folders so package updates do not overwrite
 *          local decisions.
 * @param registry Registry receiving persisted state.
 */
void graptos_plugin_registry_apply_user_state(GraptosPluginRegistry *registry);
/**
 * @brief Save current plugin enable state.
 * @details Only disabled plugin ids are persisted. Plugins default to enabled
 *          when their manifest enables them and no user override exists.
 * @param registry Registry whose state should be saved.
 * @return TRUE when the state reached disk.
 */
gboolean graptos_plugin_registry_save_user_state(GraptosPluginRegistry *registry);
/**
 * @brief Set one plugin enabled state.
 * @param registry Registry to update.
 * @param plugin_id Stable plugin id.
 * @param enabled TRUE to enable the plugin.
 * @return TRUE when a plugin was found and updated.
 */
gboolean graptos_plugin_registry_set_enabled(GraptosPluginRegistry *registry,
                                             const char *plugin_id,
                                             gboolean enabled);
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
/**
 * @brief Append plugin editor tool panel items.
 * @details Tool panel rows reuse declarative editor-line menu entries, but run
 *          them against the active cursor line instead of a right-click line.
 * @param registry Plugin registry to inspect.
 * @param tab Active editor tab.
 * @param box Tool panel box receiving button widgets.
 * @param line One-based active editor line.
 * @return Number of plugin command rows appended.
 */
guint graptos_plugin_append_editor_tool_items(GraptosPluginRegistry *registry,
                                              EditorTab *tab,
                                              GtkWidget *box,
                                              guint line);
/**
 * @brief Append plugin editor items to a transient tool popover.
 * @details This keeps the bottom plugin surface compact while reusing the
 *          same command path as editor context menus and the plugin tool panel.
 *          The popover is closed before a selected command runs.
 * @param registry Plugin registry to inspect.
 * @param tab Active editor tab.
 * @param box Popover box receiving button widgets.
 * @param popover Popover to close before running the command.
 * @param line One-based active editor line.
 * @return Number of plugin command rows appended.
 */
guint graptos_plugin_append_editor_tool_menu_items(GraptosPluginRegistry *registry,
                                                   EditorTab *tab,
                                                   GtkWidget *box,
                                                   GtkWidget *popover,
                                                   guint line);
/**
 * @brief Run a plugin shortcut command.
 * @details Keyboard handling passes stable shortcut ids here so plugins can
 *          own behavior without the editor knowing command ids.
 * @param registry Plugin registry to inspect.
 * @param tab Active editor tab.
 * @param shortcut Stable shortcut id, such as `Ctrl+Alt+P`.
 * @param line One-based active editor line.
 * @return TRUE when a plugin handled the shortcut.
 */
gboolean graptos_plugin_registry_run_shortcut(GraptosPluginRegistry *registry,
                                              EditorTab *tab,
                                              const char *shortcut,
                                              guint line);
/**
 * @brief Collect native plugin completion candidates.
 * @details The first provider that returns candidates owns the replacement
 *          prefix and visible source label for this completion popup.
 * @param registry Plugin registry to inspect.
 * @param tab Active editor tab.
 * @param replace_prefix_out Owned prefix text to replace on accept.
 * @param source_label_out Owned source label for the popup header.
 * @return GPtrArray of owned char* candidates, or NULL.
 */
GPtrArray *graptos_plugin_registry_completion_candidates(GraptosPluginRegistry *registry,
                                                        EditorTab *tab,
                                                        char **replace_prefix_out,
                                                        char **source_label_out);

/**
 * @brief Return native plugin hover text for a token.
 * @details The first enabled provider returning text owns the label and body.
 * @param registry Plugin registry to inspect.
 * @param tab Active editor tab.
 * @param word Token under the pointer.
 * @param source_label_out Owned source label for the hover heading.
 * @return Owned hover body, or NULL.
 */
char *graptos_plugin_registry_hover_text(GraptosPluginRegistry *registry,
                                         EditorTab *tab,
                                         const char *word,
                                         char **source_label_out);

/**
 * @brief Free one hub view.
 * @param data GraptosPluginHubView.
 */
void graptos_plugin_hub_view_free(gpointer data);
/**
 * @brief Return enabled native plugin hubs.
 * @param registry Plugin registry to inspect.
 * @return GPtrArray of GraptosPluginHubView*.
 */
GPtrArray *graptos_plugin_registry_hub_views(GraptosPluginRegistry *registry);
/**
 * @brief Render a native plugin hub.
 * @param registry Plugin registry to inspect.
 * @param tab Active editor tab.
 * @param plugin_id Plugin id.
 * @param hub_id Hub id.
 * @return Owned hub body, or NULL.
 */
char *graptos_plugin_registry_render_hub(GraptosPluginRegistry *registry,
                                         EditorTab *tab,
                                         const char *plugin_id,
                                         const char *hub_id);
/**
 * @brief Run a native plugin hub action.
 * @param registry Plugin registry to inspect.
 * @param tab Active editor tab.
 * @param plugin_id Plugin id.
 * @param hub_id Hub id.
 * @param action_id Action id.
 * @param input Prompt input.
 * @return TRUE when a hub handled the action.
 */
gboolean graptos_plugin_registry_run_hub_action(GraptosPluginRegistry *registry,
                                                EditorTab *tab,
                                                const char *plugin_id,
                                                const char *hub_id,
                                                const char *action_id,
                                                const char *input);

#endif /* GRAPTOS_PLUGIN_H */
