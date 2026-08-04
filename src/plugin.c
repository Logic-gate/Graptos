/**
 * @file src/plugin.c
 * @brief Graptoς plugin manifest and registry implementation.
 * @details Plugin manifests are intentionally declarative and small. The parser
 *          accepts a bounded YAML-like shape matching the rest of Graptoς data
 *          files, then stores contribution directories for syntax, theme,
 *          template, and future command loading.
 */

#include "plugin.h"

#ifndef GRAPTOS_PLUGIN_NO_UI
#include "app.h"
#include "config.h"
#include "dialogs.h"
#include "editor_tab.h"
#include "editor_tab_private.h"
#include "git.h"
#include "ui.h"
#endif

#include <gmodule.h>
#include <string.h>

#ifndef DATADIR
/**
 * @brief Datadir macro.
 */
#define DATADIR "/usr/local/share/graptos"
#endif

/**
 * @brief Plugin host backing struct.
 */
struct _GraptosPluginHost {
    GraptosPlugin *plugin; /**< Plugin currently registering native code. */
};

/**
 * @brief Plugin command execution context.
 */
struct _GraptosPluginCommandContext {
    GraptosPlugin *plugin; /**< Plugin that owns the command. */
    EditorTab *tab; /**< Editor tab receiving the command. */
    const char *command; /**< Command id being executed. */
    guint line; /**< One-based target line. */
};

/**
 * @brief Registered native command handler.
 */
typedef struct {
    GraptosPlugin *plugin; /**< Plugin that owns the handler. */
    char *command; /**< Command id. */
    char *label; /**< Optional visible command label. */
    char *shortcut; /**< Optional stable shortcut id. */
    GraptosPluginCommandFunc callback; /**< Native command callback. */
    gpointer user_data; /**< Plugin-owned callback data. */
    GraptosPluginDestroyFunc destroy; /**< Optional user data destroy hook. */
    gboolean editor_line; /**< TRUE when shown in editor line command surfaces. */
} GraptosNativeCommand;

/**
 * @brief Free a native command handler.
 * @param data Native command handler.
 */
static void native_command_free(gpointer data) {
    GraptosNativeCommand *command = data;
    if (!command) return;
    if (command->destroy) command->destroy(command->user_data);
    g_free(command->command);
    g_free(command->label);
    g_free(command->shortcut);
    g_free(command);
}

/**
 * @brief Registered native completion provider.
 */
typedef struct {
    GraptosPlugin *plugin; /**< Plugin that owns the provider. */
    char *provider_id; /**< Provider id. */
    char *label; /**< Visible source label. */
    GraptosPluginCompletionFunc callback; /**< Completion callback. */
    gpointer user_data; /**< Plugin-owned callback data. */
    GraptosPluginDestroyFunc destroy; /**< Optional user data destroy hook. */
} GraptosNativeCompletionProvider;

/**
 * @brief Free a native completion provider.
 * @param data Native completion provider.
 */
static void native_completion_provider_free(gpointer data) {
    GraptosNativeCompletionProvider *provider = data;
    if (!provider) return;
    if (provider->destroy) provider->destroy(provider->user_data);
    g_free(provider->provider_id);
    g_free(provider->label);
    g_free(provider);
}

/**
 * @brief Registered native plugin hub.
 */
typedef struct {
    GraptosPlugin *plugin; /**< Plugin that owns the hub. */
    char *hub_id; /**< Stable hub id. */
    char *label; /**< Visible hub label. */
    GraptosPluginHubRenderFunc render; /**< Hub render callback. */
    GraptosPluginHubActionFunc action; /**< Hub action callback. */
    gpointer user_data; /**< Plugin-owned callback data. */
    GraptosPluginDestroyFunc destroy; /**< Optional user data destroy hook. */
} GraptosNativeHub;

/**
 * @brief Free a native plugin hub.
 * @param data Native hub.
 */
static void native_hub_free(gpointer data) {
    GraptosNativeHub *hub = data;
    if (!hub) return;
    if (hub->destroy) hub->destroy(hub->user_data);
    g_free(hub->hub_id);
    g_free(hub->label);
    g_free(hub);
}

/**
 * @brief Registered native hover provider.
 */
typedef struct {
    GraptosPlugin *plugin; /**< Plugin that owns the provider. */
    char *provider_id; /**< Provider id. */
    char *label; /**< Visible hover heading. */
    GraptosPluginHoverFunc callback; /**< Hover callback. */
    gpointer user_data; /**< Plugin-owned callback data. */
    GraptosPluginDestroyFunc destroy; /**< Optional user data destroy hook. */
} GraptosNativeHoverProvider;

/**
 * @brief Free a native hover provider.
 * @param data Native hover provider.
 */
static void native_hover_provider_free(gpointer data) {
    GraptosNativeHoverProvider *provider = data;
    if (!provider) return;
    if (provider->destroy) provider->destroy(provider->user_data);
    g_free(provider->provider_id);
    g_free(provider->label);
    g_free(provider);
}

GQuark graptos_plugin_error_quark(void) {
    return g_quark_from_static_string("graptos-plugin-error-quark");
}

guint graptos_plugin_host_api_version(GraptosPluginHost *host) {
    (void)host;
    return GRAPTOS_PLUGIN_API_VERSION;
}

const char *graptos_plugin_host_plugin_id(GraptosPluginHost *host) {
    return host && host->plugin ? host->plugin->id : NULL;
}

/**
 * @brief Register a native command implementation.
 * @details Native plugins own their command ids. YAML still owns the trust
 *          boundary by deciding which plugins can load native code.
 * @param host The host capability object supplied by Graptoς.
 * @param command_id Native command id.
 * @param label Optional visible label.
 * @param editor_line TRUE when shown in editor line command surfaces.
 * @param callback Native command callback.
 * @param user_data Plugin data passed to callback.
 * @param destroy Optional destroy callback.
 * @return TRUE when registration succeeds.
 */
static gboolean plugin_register_native_command(GraptosPluginHost *host,
                                               const char *command_id,
                                               const char *label,
                                               const char *shortcut,
                                               gboolean editor_line,
                                               GraptosPluginCommandFunc callback,
                                               gpointer user_data,
                                               GraptosPluginDestroyFunc destroy) {
    if (!host || !host->plugin || !command_id || !command_id[0] ||
        !callback) {
        return FALSE;
    }
    GraptosPlugin *plugin = host->plugin;
    if (!plugin->native_commands) {
        plugin->native_commands = g_hash_table_new_full(g_str_hash,
                                                        g_str_equal,
                                                        g_free,
                                                        native_command_free);
    }
    GraptosNativeCommand *command = g_new0(GraptosNativeCommand, 1);
    if (!command) return FALSE;
    command->plugin = plugin;
    command->command = g_strdup(command_id);
    command->label = label && label[0] ? g_strdup(label) : NULL;
    command->shortcut = shortcut && shortcut[0] ? g_strdup(shortcut) : NULL;
    command->callback = callback;
    command->user_data = user_data;
    command->destroy = destroy;
    command->editor_line = editor_line;
    g_hash_table_replace(plugin->native_commands,
                         g_strdup(command_id),
                         command);
    return TRUE;
}

gboolean graptos_plugin_host_register_command(GraptosPluginHost *host,
                                              const char *command_id,
                                              GraptosPluginCommandFunc callback,
                                              gpointer user_data,
                                              GraptosPluginDestroyFunc destroy) {
    return plugin_register_native_command(host,
                                          command_id,
                                          NULL,
                                          NULL,
                                          FALSE,
                                          callback,
                                          user_data,
                                          destroy);
}

gboolean graptos_plugin_host_register_editor_line_command(GraptosPluginHost *host,
                                                          const char *command_id,
                                                          const char *label,
                                                          GraptosPluginCommandFunc callback,
                                                          gpointer user_data,
                                                          GraptosPluginDestroyFunc destroy) {
    return plugin_register_native_command(host,
                                          command_id,
                                          label,
                                          NULL,
                                          TRUE,
                                          callback,
                                          user_data,
                                          destroy);
}

gboolean graptos_plugin_host_register_editor_line_command_with_shortcut(GraptosPluginHost *host,
                                                                        const char *command_id,
                                                                        const char *label,
                                                                        const char *shortcut,
                                                                        GraptosPluginCommandFunc callback,
                                                                        gpointer user_data,
                                                                        GraptosPluginDestroyFunc destroy) {
    return plugin_register_native_command(host,
                                          command_id,
                                          label,
                                          shortcut,
                                          TRUE,
                                          callback,
                                          user_data,
                                          destroy);
}

gboolean graptos_plugin_host_register_completion_provider(GraptosPluginHost *host,
                                                          const char *provider_id,
                                                          const char *label,
                                                          GraptosPluginCompletionFunc callback,
                                                          gpointer user_data,
                                                          GraptosPluginDestroyFunc destroy) {
    if (!host || !host->plugin || !provider_id || !provider_id[0] ||
        !callback) {
        return FALSE;
    }
    GraptosPlugin *plugin = host->plugin;
    if (!plugin->native_completion_providers) {
        plugin->native_completion_providers =
            g_ptr_array_new_with_free_func(native_completion_provider_free);
    }
    GraptosNativeCompletionProvider *provider = g_new0(GraptosNativeCompletionProvider, 1);
    if (!provider) return FALSE;
    provider->plugin = plugin;
    provider->provider_id = g_strdup(provider_id);
    provider->label = g_strdup(label && label[0] ? label : "Plugin");
    provider->callback = callback;
    provider->user_data = user_data;
    provider->destroy = destroy;
    g_ptr_array_add(plugin->native_completion_providers, provider);
    return TRUE;
}

gboolean graptos_plugin_host_register_hover_provider(GraptosPluginHost *host,
                                                     const char *provider_id,
                                                     const char *label,
                                                     GraptosPluginHoverFunc callback,
                                                     gpointer user_data,
                                                     GraptosPluginDestroyFunc destroy) {
    if (!host || !host->plugin || !provider_id || !provider_id[0] ||
        !callback) {
        return FALSE;
    }
    GraptosPlugin *plugin = host->plugin;
    if (!plugin->native_hover_providers) {
        plugin->native_hover_providers =
            g_ptr_array_new_with_free_func(native_hover_provider_free);
    }
    GraptosNativeHoverProvider *provider = g_new0(GraptosNativeHoverProvider, 1);
    if (!provider) return FALSE;
    provider->plugin = plugin;
    provider->provider_id = g_strdup(provider_id);
    provider->label = g_strdup(label && label[0] ? label : "Plugin");
    provider->callback = callback;
    provider->user_data = user_data;
    provider->destroy = destroy;
    g_ptr_array_add(plugin->native_hover_providers, provider);
    return TRUE;
}

gboolean graptos_plugin_host_register_hub(GraptosPluginHost *host,
                                          const char *hub_id,
                                          const char *label,
                                          GraptosPluginHubRenderFunc render,
                                          GraptosPluginHubActionFunc action,
                                          gpointer user_data,
                                          GraptosPluginDestroyFunc destroy) {
    if (!host || !host->plugin || !hub_id || !hub_id[0] || !render || !action) {
        return FALSE;
    }
    GraptosPlugin *plugin = host->plugin;
    if (!plugin->native_hubs) {
        plugin->native_hubs = g_ptr_array_new_with_free_func(native_hub_free);
    }
    GraptosNativeHub *hub = g_new0(GraptosNativeHub, 1);
    if (!hub) return FALSE;
    hub->plugin = plugin;
    hub->hub_id = g_strdup(hub_id);
    hub->label = g_strdup(label && label[0] ? label : "Plugin Hub");
    hub->render = render;
    hub->action = action;
    hub->user_data = user_data;
    hub->destroy = destroy;
    g_ptr_array_add(plugin->native_hubs, hub);
    return TRUE;
}

/**
 * @brief Strip a line comment.
 * @details Plugin manifests do not support quoted hashes yet. Matching the
 *          existing project-template parser keeps the v1 format easy to audit.
 * @param line Mutable line text.
 */
static void strip_comment(char *line) {
    if (!line) return;
    char *hash = strchr(line, '#');
    if (hash) *hash = '\0';
}

/**
 * @brief Parse one key-value line.
 * @details The manifest parser only accepts simple scalar keys and explicit
 *          list sections so unsupported shapes fail predictably.
 * @param line The stripped input line.
 * @param key_out Output key.
 * @param value_out Output value.
 * @return TRUE when parsing succeeds.
 */
static gboolean parse_key_value(const char *line, char **key_out, char **value_out) {
    const char *colon = strchr(line, ':');
    if (!colon) return FALSE;
    char *key = g_strndup(line, (gsize)(colon - line));
    char *value = g_strdup(colon + 1);
    if (!key || !value) {
        g_free(key);
        g_free(value);
        return FALSE;
    }
    g_strstrip(key);
    g_strstrip(value);
    *key_out = key;
    *value_out = value;
    return TRUE;
}

/**
 * @brief Duplicate a scalar manifest value.
 * @details Simple quote stripping is enough for Graptoς manifests because paths
 *          and ids are line-local scalar values.
 * @param value Raw scalar text.
 * @return Newly allocated unquoted value.
 */
static char *scalar_dup(const char *value) {
    char *copy = g_strdup(value ? value : "");
    if (!copy) return NULL;
    g_strstrip(copy);
    gsize len = strlen(copy);
    if (len >= 2u &&
        ((copy[0] == '"' && copy[len - 1u] == '"') ||
         (copy[0] == '\'' && copy[len - 1u] == '\''))) {
        copy[len - 1u] = '\0';
        char *out = g_strdup(copy + 1);
        g_free(copy);
        return out;
    }
    return copy;
}

/**
 * @brief Validate a stable plugin identifier.
 * @details Plugin ids become config keys and directory names, so keep them to
 *          lowercase identifiers plus dot, dash, and underscore separators.
 * @param id Candidate id.
 * @return TRUE when the id is valid.
 */
static gboolean valid_plugin_id(const char *id) {
    if (!id || !id[0]) return FALSE;
    for (const char *p = id; *p; p++) {
        if (g_ascii_islower(*p) || g_ascii_isdigit(*p) ||
            *p == '.' || *p == '-' || *p == '_') {
            continue;
        }
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Add a manifest list value.
 * @details Values are kept relative until contribution lookup so plugin
 *          manifests remain movable between system and user directories.
 * @param array Destination string array.
 * @param value Raw scalar value.
 */
static void add_list_value(GPtrArray *array, const char *value) {
    if (!array || !value) return;
    g_autofree char *item = scalar_dup(value);
    if (item && item[0] != '\0') g_ptr_array_add(array, g_strdup(item));
}

/**
 * @brief Allocate an empty plugin manifest.
 * @details Arrays are owned by the manifest and always available to callers so
 *          contribution loops do not need null checks.
 * @return New manifest object.
 */
static GraptosPlugin *plugin_new(void) {
    GraptosPlugin *plugin = g_new0(GraptosPlugin, 1);
    if (!plugin) return NULL;
    plugin->enabled = TRUE;
    plugin->default_enabled = TRUE;
    plugin->graptos_api_version = GRAPTOS_PLUGIN_API_VERSION;
    plugin->permissions = g_ptr_array_new_with_free_func(g_free);
    plugin->syntax_dirs = g_ptr_array_new_with_free_func(g_free);
    plugin->theme_dirs = g_ptr_array_new_with_free_func(g_free);
    plugin->template_dirs = g_ptr_array_new_with_free_func(g_free);
    plugin->snippet_dirs = g_ptr_array_new_with_free_func(g_free);
    plugin->commands = g_ptr_array_new_with_free_func(g_free);
    plugin->menus = g_ptr_array_new_with_free_func(g_free);
    plugin->native_commands = g_hash_table_new_full(g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    native_command_free);
    plugin->native_completion_providers =
        g_ptr_array_new_with_free_func(native_completion_provider_free);
    plugin->native_hover_providers =
        g_ptr_array_new_with_free_func(native_hover_provider_free);
    plugin->native_hubs =
        g_ptr_array_new_with_free_func(native_hub_free);
    return plugin;
}

void graptos_plugin_free(GraptosPlugin *plugin) {
    if (!plugin) return;
    if (plugin->native_handle) g_module_close((GModule *)plugin->native_handle);
    g_free(plugin->id);
    g_free(plugin->name);
    g_free(plugin->version);
    g_free(plugin->description);
    g_free(plugin->base_path);
    g_free(plugin->native_library);
    if (plugin->permissions) g_ptr_array_free(plugin->permissions, TRUE);
    if (plugin->syntax_dirs) g_ptr_array_free(plugin->syntax_dirs, TRUE);
    if (plugin->theme_dirs) g_ptr_array_free(plugin->theme_dirs, TRUE);
    if (plugin->template_dirs) g_ptr_array_free(plugin->template_dirs, TRUE);
    if (plugin->snippet_dirs) g_ptr_array_free(plugin->snippet_dirs, TRUE);
    if (plugin->commands) g_ptr_array_free(plugin->commands, TRUE);
    if (plugin->menus) g_ptr_array_free(plugin->menus, TRUE);
    if (plugin->native_commands) g_hash_table_destroy(plugin->native_commands);
    if (plugin->native_completion_providers) {
        g_ptr_array_free(plugin->native_completion_providers, TRUE);
    }
    if (plugin->native_hover_providers) {
        g_ptr_array_free(plugin->native_hover_providers, TRUE);
    }
    if (plugin->native_hubs) {
        g_ptr_array_free(plugin->native_hubs, TRUE);
    }
    g_free(plugin);
}

/**
 * @brief Parse a plugin manifest.
 * @details The supported v1 shape has top-level scalar metadata, permissions as
 *          a list, and contribution lists below contributes.<kind>.
 * @param plugin_dir Directory containing plugin.yaml.
 * @param error Return location for parse errors.
 * @return Parsed plugin, or NULL.
 */
GraptosPlugin *graptos_plugin_load_manifest(const char *plugin_dir, GError **error) {
    g_autofree char *manifest = g_build_filename(plugin_dir, "plugin.yaml", NULL);
    g_autofree char *text = NULL;
    gsize len = 0u;
    if (!g_file_get_contents(manifest, &text, &len, error)) return NULL;

    GraptosPlugin *plugin = plugin_new();
    if (!plugin) return NULL;
    plugin->base_path = g_canonicalize_filename(plugin_dir, NULL);

    enum {
        SECTION_TOP,
        SECTION_PERMISSIONS,
        SECTION_CONTRIBUTES,
        SECTION_SYNTAX,
        SECTION_THEMES,
        SECTION_TEMPLATES,
        SECTION_SNIPPETS,
        SECTION_COMMANDS,
        SECTION_MENUS
    } section = SECTION_TOP;

    char **lines = g_strsplit(text, "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        char *line = lines[i];
        strip_comment(line);
        g_autofree char *trim_copy = g_strdup(line);
        char *trim = g_strstrip(trim_copy);
        if (trim[0] == '\0') continue;
        guint indent = 0u;
        while (line[indent] == ' ') indent++;

        if (indent == 0u) {
            if (g_strcmp0(trim, "permissions:") == 0) {
                section = SECTION_PERMISSIONS;
                continue;
            }
            if (g_strcmp0(trim, "contributes:") == 0) {
                section = SECTION_CONTRIBUTES;
                continue;
            }
            section = SECTION_TOP;
        } else if ((section == SECTION_CONTRIBUTES ||
                    section == SECTION_SYNTAX ||
                    section == SECTION_THEMES ||
                    section == SECTION_TEMPLATES ||
                    section == SECTION_SNIPPETS ||
                    section == SECTION_COMMANDS ||
                    section == SECTION_MENUS) && indent == 2u) {
            if (g_strcmp0(trim, "syntaxes:") == 0) section = SECTION_SYNTAX;
            else if (g_strcmp0(trim, "themes:") == 0) section = SECTION_THEMES;
            else if (g_strcmp0(trim, "templates:") == 0) section = SECTION_TEMPLATES;
            else if (g_strcmp0(trim, "snippets:") == 0) section = SECTION_SNIPPETS;
            else if (g_strcmp0(trim, "commands:") == 0) section = SECTION_COMMANDS;
            else if (g_strcmp0(trim, "menus:") == 0) section = SECTION_MENUS;
            else {
                g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                            GRAPTOS_PLUGIN_ERROR_INVALID_SCHEMA,
                            "Plugin error in %s: unsupported contribution line %u: %s",
                            manifest, i + 1u, trim);
                goto fail;
            }
            continue;
        }

        if (g_str_has_prefix(trim, "-")) {
            char *value = g_strstrip(trim + 1);
            if (section == SECTION_PERMISSIONS) add_list_value(plugin->permissions, value);
            else if (section == SECTION_SYNTAX) add_list_value(plugin->syntax_dirs, value);
            else if (section == SECTION_THEMES) add_list_value(plugin->theme_dirs, value);
            else if (section == SECTION_TEMPLATES) add_list_value(plugin->template_dirs, value);
            else if (section == SECTION_SNIPPETS) add_list_value(plugin->snippet_dirs, value);
            else if (section == SECTION_COMMANDS) add_list_value(plugin->commands, value);
            else if (section == SECTION_MENUS) add_list_value(plugin->menus, value);
            else {
                g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                            GRAPTOS_PLUGIN_ERROR_INVALID_SCHEMA,
                            "Plugin error in %s: misplaced list item on line %u",
                            manifest, i + 1u);
                goto fail;
            }
            continue;
        }

        g_autofree char *key = NULL;
        g_autofree char *value = NULL;
        if (!parse_key_value(trim, &key, &value)) {
            g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                        GRAPTOS_PLUGIN_ERROR_INVALID_SCHEMA,
                        "Plugin error in %s: malformed line %u: %s",
                        manifest, i + 1u, trim);
            goto fail;
        }

        if (section != SECTION_TOP) {
            g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                        GRAPTOS_PLUGIN_ERROR_INVALID_SCHEMA,
                        "Plugin error in %s: unsupported line %u: %s",
                        manifest, i + 1u, trim);
            goto fail;
        }
        if (g_strcmp0(key, "id") == 0) {
            g_free(plugin->id);
            plugin->id = scalar_dup(value);
        } else if (g_strcmp0(key, "name") == 0) {
            g_free(plugin->name);
            plugin->name = scalar_dup(value);
        } else if (g_strcmp0(key, "version") == 0) {
            g_free(plugin->version);
            plugin->version = scalar_dup(value);
        } else if (g_strcmp0(key, "description") == 0) {
            g_free(plugin->description);
            plugin->description = scalar_dup(value);
        } else if (g_strcmp0(key, "graptos_api_version") == 0) {
            plugin->graptos_api_version = (guint)g_ascii_strtoull(value, NULL, 10);
        } else if (g_strcmp0(key, "native") == 0) {
            g_free(plugin->native_library);
            plugin->native_library = scalar_dup(value);
        } else if (g_strcmp0(key, "enabled") == 0) {
            plugin->enabled = g_ascii_strcasecmp(value, "false") != 0 &&
                              g_ascii_strcasecmp(value, "no") != 0 &&
                              strcmp(value, "0") != 0;
        } else {
            g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                        GRAPTOS_PLUGIN_ERROR_INVALID_SCHEMA,
                        "Plugin error in %s: unsupported key %s", manifest, key);
            goto fail;
        }
    }
    g_strfreev(lines);
    lines = NULL;

    if (!plugin->id || !plugin->name || !plugin->version) {
        g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                    GRAPTOS_PLUGIN_ERROR_INVALID_SCHEMA,
                    "Plugin error in %s: id, name, and version are required",
                    manifest);
        goto fail;
    }
    if (!valid_plugin_id(plugin->id)) {
        g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                    GRAPTOS_PLUGIN_ERROR_INVALID_ID,
                    "Plugin error in %s: invalid plugin id %s",
                    manifest, plugin->id);
        goto fail;
    }
    if (plugin->graptos_api_version != GRAPTOS_PLUGIN_API_VERSION) {
        g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                    GRAPTOS_PLUGIN_ERROR_UNSUPPORTED_API,
                    "Plugin %s requires API %u, Graptoς supports API %u",
                    plugin->id,
                    plugin->graptos_api_version,
                    GRAPTOS_PLUGIN_API_VERSION);
        goto fail;
    }
    plugin->default_enabled = plugin->enabled;
    return plugin;

fail:
    if (lines) g_strfreev(lines);
    graptos_plugin_free(plugin);
    return NULL;
}

GraptosPluginRegistry *graptos_plugin_registry_new(void) {
    GraptosPluginRegistry *registry = g_new0(GraptosPluginRegistry, 1);
    if (!registry) return NULL;
    registry->plugins = g_ptr_array_new_with_free_func((GDestroyNotify)graptos_plugin_free);
    return registry;
}

void graptos_plugin_registry_free(GraptosPluginRegistry *registry) {
    if (!registry) return;
    if (registry->plugins) g_ptr_array_free(registry->plugins, TRUE);
    g_free(registry);
}

/**
 * @brief Discover plugin manifests below one root.
 * @details Directory names are not trusted as plugin ids. Only parsed manifests
 *          determine plugin identity.
 * @param registry Plugin registry receiving valid plugins.
 * @param by_id Deduplication table.
 * @param root Root directory to scan.
 */
static void discover_root(GraptosPluginRegistry *registry,
                          GHashTable *by_id,
                          const char *root) {
    if (!registry || !by_id || !root) return;
    GDir *dir = g_dir_open(root, 0, NULL);
    if (!dir) return;
    const char *name = NULL;
    while ((name = g_dir_read_name(dir))) {
        g_autofree char *child = g_build_filename(root, name, NULL);
        if (!g_file_test(child, G_FILE_TEST_IS_DIR)) continue;
        g_autoptr(GError) error = NULL;
        GraptosPlugin *plugin = graptos_plugin_load_manifest(child, &error);
        if (!plugin) continue;
        if (g_hash_table_contains(by_id, plugin->id)) {
            graptos_plugin_free(plugin);
            continue;
        }
        g_hash_table_add(by_id, g_strdup(plugin->id));
        g_ptr_array_add(registry->plugins, plugin);
    }
    g_dir_close(dir);
}

gboolean graptos_plugin_registry_discover(GraptosPluginRegistry *registry,
                                          GError **error) {
    (void)error;
    if (!registry) return FALSE;
    if (registry->plugins) g_ptr_array_set_size(registry->plugins, 0u);
    if (g_getenv("GRAPTOS_DISABLE_PLUGINS")) return TRUE;
    g_autoptr(GHashTable) by_id = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    g_autofree char *system = g_build_filename(DATADIR, "plugins", NULL);
    g_autofree char *user = g_build_filename(g_get_user_data_dir(), "graptos", "plugins", NULL);
    discover_root(registry, by_id, user);
    discover_root(registry, by_id, system);
    discover_root(registry, by_id, "data/plugins");
    return TRUE;
}

/**
 * @brief Return the user plugin state path.
 * @details Plugin enable choices are editor preferences, not plugin package
 *          data, so they live beside the main Graptoς config file.
 * @return Owned plugin state path.
 */
static char *plugin_state_path(void) {
    const char *base = g_get_user_config_dir();
    if (!base || base[0] == '\0') return NULL;
    return g_build_filename(base, "graptos", "plugins.ini", NULL);
}

/**
 * @brief Return whether a string list contains a plugin id.
 * @param values String list returned by GKeyFile.
 * @param id Plugin id to find.
 * @return TRUE when the id exists in the list.
 */
static gboolean plugin_id_list_contains(char **values, const char *id) {
    if (!values || !id) return FALSE;
    for (guint i = 0u; values[i]; i++) {
        if (g_strcmp0(values[i], id) == 0) return TRUE;
    }
    return FALSE;
}

void graptos_plugin_registry_apply_user_state(GraptosPluginRegistry *registry) {
    if (!registry || !registry->plugins) return;
    g_autofree char *path = plugin_state_path();
    if (!path || !g_file_test(path, G_FILE_TEST_EXISTS)) return;
    g_autoptr(GKeyFile) key_file = g_key_file_new();
    if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL)) return;

    gsize disabled_len = 0u;
    g_auto(GStrv) disabled =
        g_key_file_get_string_list(key_file, "Plugins", "disabled", &disabled_len, NULL);
    gsize enabled_len = 0u;
    g_auto(GStrv) enabled =
        g_key_file_get_string_list(key_file, "Plugins", "enabled", &enabled_len, NULL);
    (void)disabled_len;
    (void)enabled_len;
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->id) continue;
        if (plugin_id_list_contains(enabled, plugin->id)) {
            plugin->enabled = TRUE;
        } else if (plugin_id_list_contains(disabled, plugin->id)) {
            plugin->enabled = FALSE;
        }
    }
}

gboolean graptos_plugin_registry_save_user_state(GraptosPluginRegistry *registry) {
    if (!registry || !registry->plugins) return FALSE;
    g_autofree char *path = plugin_state_path();
    if (!path) return FALSE;

    GPtrArray *enabled = g_ptr_array_new();
    GPtrArray *disabled = g_ptr_array_new();
    if (!enabled || !disabled) {
        if (enabled) g_ptr_array_free(enabled, TRUE);
        if (disabled) g_ptr_array_free(disabled, TRUE);
        return FALSE;
    }
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->id) continue;
        if (!plugin->default_enabled && plugin->enabled) {
            g_ptr_array_add(enabled, plugin->id);
        } else if (plugin->default_enabled && !plugin->enabled) {
            g_ptr_array_add(disabled, plugin->id);
        }
    }

    g_autoptr(GKeyFile) key_file = g_key_file_new();
    g_key_file_set_string_list(key_file,
                               "Plugins",
                               "enabled",
                               (const gchar * const *)enabled->pdata,
                               enabled->len);
    g_key_file_set_string_list(key_file,
                               "Plugins",
                               "disabled",
                               (const gchar * const *)disabled->pdata,
                               disabled->len);
    g_ptr_array_free(enabled, TRUE);
    g_ptr_array_free(disabled, TRUE);

    gsize len = 0u;
    g_autofree char *data = g_key_file_to_data(key_file, &len, NULL);
    g_autofree char *dir = g_path_get_dirname(path);
    if (!data || g_mkdir_with_parents(dir, 0700) != 0) return FALSE;
    return g_file_set_contents(path, data, (gssize)len, NULL);
}

gboolean graptos_plugin_registry_set_enabled(GraptosPluginRegistry *registry,
                                             const char *plugin_id,
                                             gboolean enabled) {
    if (!registry || !registry->plugins || !plugin_id) return FALSE;
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (plugin && g_strcmp0(plugin->id, plugin_id) == 0) {
            plugin->enabled = enabled;
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Select a contribution array from a plugin.
 * @param plugin Plugin manifest.
 * @param kind Contribution kind.
 * @return Contribution array, or NULL.
 */
static GPtrArray *plugin_contribution_array(GraptosPlugin *plugin,
                                            GraptosPluginContributionKind kind) {
    if (!plugin) return NULL;
    switch (kind) {
        case GRAPTOS_PLUGIN_CONTRIBUTION_SYNTAX: return plugin->syntax_dirs;
        case GRAPTOS_PLUGIN_CONTRIBUTION_THEME: return plugin->theme_dirs;
        case GRAPTOS_PLUGIN_CONTRIBUTION_TEMPLATE: return plugin->template_dirs;
        case GRAPTOS_PLUGIN_CONTRIBUTION_SNIPPET: return plugin->snippet_dirs;
        case GRAPTOS_PLUGIN_CONTRIBUTION_COMMAND: return plugin->commands;
        case GRAPTOS_PLUGIN_CONTRIBUTION_MENU: return plugin->menus;
        default: return NULL;
    }
}

/**
 * @brief Check whether a plugin declared a permission.
 * @details Native loading is intentionally gated even when callers explicitly
 *          invoke it, because loading a shared library gives that plugin the
 *          same process trust as Graptoς.
 * @param plugin Plugin manifest.
 * @param permission Permission id.
 * @return TRUE when the permission is present.
 */
static gboolean plugin_has_permission(GraptosPlugin *plugin,
                                      const char *permission) {
    if (!plugin || !plugin->permissions || !permission) return FALSE;
    for (guint i = 0u; i < plugin->permissions->len; i++) {
        if (g_strcmp0(g_ptr_array_index(plugin->permissions, i),
                      permission) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

#ifndef GRAPTOS_PLUGIN_NO_UI
/**
 * @brief Context carried by one plugin editor menu row.
 */
typedef struct {
    GraptosPlugin *plugin; /**< Plugin that owns the command. */
    EditorTab *tab; /**< Editor tab under the menu. */
    GtkWidget *popover; /**< Context popover to close before running. */
    char *command; /**< Command id from the manifest. */
    char *label; /**< Human-facing menu label. */
    guint line; /**< One-based editor line. */
} GraptosPluginEditorMenuAction;

static guint plugin_tab_cursor_line(EditorTab *tab);
static void plugin_run_native_command(GraptosPlugin *plugin,
                                      GraptosNativeCommand *command,
                                      EditorTab *tab,
                                      guint line);

/**
 * @brief Free a plugin editor menu action.
 * @param data Menu action data.
 */
static void plugin_editor_menu_action_free(gpointer data) {
    GraptosPluginEditorMenuAction *action = data;
    if (!action) return;
    g_free(action->command);
    g_free(action->label);
    g_free(action);
}

/**
 * @brief Check whether a plugin declared a command.
 * @param plugin Plugin manifest.
 * @param command Command id.
 * @return TRUE when the command is declared.
 */
static gboolean plugin_declares_command(GraptosPlugin *plugin,
                                        const char *command) {
    if (!plugin || !plugin->commands || !command) return FALSE;
    for (guint i = 0u; i < plugin->commands->len; i++) {
        if (g_strcmp0(g_ptr_array_index(plugin->commands, i), command) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Return text for one editor line.
 * @details Demo plugin commands use GtkTextBuffer APIs through Graptoς core,
 *          not by exposing the buffer to plugin manifests.
 * @param tab Editor tab.
 * @param line One-based line number.
 * @return Newly allocated line text.
 */
static char *plugin_editor_line_text(EditorTab *tab, guint line) {
    if (!tab || !tab->buffer || line == 0u) return g_strdup("");
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_iter_at_line(tab->buffer, &start, (gint)(line - 1u));
    end = start;
    if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);
    return gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
}

const char *graptos_plugin_context_plugin_id(GraptosPluginCommandContext *context) {
    return context && context->plugin ? context->plugin->id : NULL;
}

const char *graptos_plugin_context_command_id(GraptosPluginCommandContext *context) {
    return context ? context->command : NULL;
}

guint graptos_plugin_context_line(GraptosPluginCommandContext *context) {
    return context ? context->line : 0u;
}

char *graptos_plugin_context_file_path(GraptosPluginCommandContext *context) {
    if (!context || !context->tab || !context->tab->file_path) return NULL;
    return g_strdup(context->tab->file_path);
}

char *graptos_plugin_context_text(GraptosPluginCommandContext *context) {
    if (!context || !context->tab || !context->tab->buffer) return g_strdup("");
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(context->tab->buffer, &start, &end);
    return gtk_text_buffer_get_text(context->tab->buffer, &start, &end, FALSE);
}

char *graptos_plugin_context_selection(GraptosPluginCommandContext *context) {
    if (!context || !context->tab || !context->tab->buffer) return g_strdup("");
    GtkTextIter start;
    GtkTextIter end;
    if (!gtk_text_buffer_get_selection_bounds(context->tab->buffer,
                                              &start,
                                              &end)) {
        return g_strdup("");
    }
    return gtk_text_buffer_get_text(context->tab->buffer, &start, &end, FALSE);
}

char *graptos_plugin_context_line_text(GraptosPluginCommandContext *context,
                                       guint line) {
    if (!context) return g_strdup("");
    return plugin_editor_line_text(context->tab, line);
}

char *graptos_plugin_context_line_prefix(GraptosPluginCommandContext *context) {
    if (!context || !context->tab || !context->tab->buffer) return g_strdup("");
    GtkTextIter cursor;
    GtkTextMark *insert = gtk_text_buffer_get_insert(context->tab->buffer);
    gtk_text_buffer_get_iter_at_mark(context->tab->buffer, &cursor, insert);
    GtkTextIter start = cursor;
    gtk_text_iter_set_line_offset(&start, 0);
    return gtk_text_buffer_get_text(context->tab->buffer,
                                    &start,
                                    &cursor,
                                    FALSE);
}

const char *graptos_plugin_context_syntax_name(GraptosPluginCommandContext *context) {
    return context && context->tab && context->tab->active_syntax
        ? context->tab->active_syntax->name
        : NULL;
}

gboolean graptos_plugin_context_is_modified(GraptosPluginCommandContext *context) {
    return context && context->tab ? context->tab->modified : FALSE;
}

gboolean graptos_plugin_context_insert_text(GraptosPluginCommandContext *context,
                                            const char *text) {
    if (!context || !context->tab || !context->tab->buffer || !text) return FALSE;
    gtk_text_buffer_insert_at_cursor(context->tab->buffer, text, -1);
    return TRUE;
}

gboolean graptos_plugin_context_replace_selection(GraptosPluginCommandContext *context,
                                                  const char *text) {
    if (!context || !context->tab || !context->tab->buffer || !text) return FALSE;
    GtkTextIter start;
    GtkTextIter end;
    if (gtk_text_buffer_get_selection_bounds(context->tab->buffer,
                                             &start,
                                             &end)) {
        gtk_text_buffer_delete(context->tab->buffer, &start, &end);
        gtk_text_buffer_insert(context->tab->buffer, &start, text, -1);
    } else {
        gtk_text_buffer_insert_at_cursor(context->tab->buffer, text, -1);
    }
    return TRUE;
}

gboolean graptos_plugin_context_replace_text(GraptosPluginCommandContext *context,
                                             const char *text) {
    if (!context || !context->tab || !context->tab->buffer || !text ||
        context->tab->locked) {
        return FALSE;
    }

    gtk_text_buffer_begin_user_action(context->tab->buffer);
    gtk_text_buffer_set_text(context->tab->buffer, text, -1);
    gtk_text_buffer_end_user_action(context->tab->buffer);

    editor_tab_schedule_lightweight_ui_refresh(context->tab);
    editor_tab_schedule_syntax_diagnostics(context->tab);
    return TRUE;
}

void graptos_plugin_context_clear_diagnostics(GraptosPluginCommandContext *context) {
    if (!context || !context->tab) return;
    clear_syntax_diagnostics(context->tab);
    editor_tab_schedule_lightweight_ui_refresh(context->tab);
}

gboolean graptos_plugin_context_add_line_diagnostic(GraptosPluginCommandContext *context,
                                                    guint line,
                                                    const char *message) {
    if (!context || !context->tab || line == 0u) return FALSE;
    gboolean applied = editor_tab_apply_external_diagnostic(context->tab,
                                                           (gint)(line - 1u),
                                                           0,
                                                           (gint)(line - 1u),
                                                           G_MAXINT,
                                                           message);
    if (applied) editor_tab_schedule_lightweight_ui_refresh(context->tab);
    return applied;
}

void graptos_plugin_context_show_output(GraptosPluginCommandContext *context,
                                        const char *title,
                                        const char *heading,
                                        const char *body) {
    GtkWindow *parent = context && context->tab && context->tab->win
        ? app_window_gtk(context->tab->win)
        : NULL;
    dialog_output(parent,
                  title ? title : "Plugin Output",
                  heading ? heading : "Plugin Output",
                  body ? body : "");
}

/**
 * @brief Return a safe generated file stem.
 * @param basename Requested basename.
 * @return Owned safe basename.
 */
static char *plugin_safe_basename(const char *basename) {
    GString *out = g_string_new(NULL);
    for (const char *p = basename && basename[0] ? basename : "plugin-report"; *p; p++) {
        if (g_ascii_isalnum(*p) || *p == '-' || *p == '_') {
            g_string_append_c(out, *p);
        } else {
            g_string_append_c(out, '-');
        }
    }
    if (out->len == 0u) g_string_append(out, "plugin-report");
    return g_string_free(out, FALSE);
}

/**
 * @brief Open a plugin-generated PDF.
 * @param context Command context supplied by Graptoς.
 * @param pdf_path Path to generated PDF.
 * @return TRUE when the default application launched.
 */
static gboolean plugin_open_pdf(GraptosPluginCommandContext *context,
                                const char *pdf_path) {
    g_autoptr(GError) error = NULL;
    g_autofree char *uri = g_filename_to_uri(pdf_path, NULL, &error);
    if (!uri) {
        if (context && context->tab && context->tab->win) {
            app_window_set_error_status(context->tab->win,
                                        "Could not open exported PDF",
                                        error ? error->message : "Invalid PDF path");
        }
        return FALSE;
    }
    if (!g_app_info_launch_default_for_uri(uri, NULL, &error)) {
        if (context && context->tab && context->tab->win) {
            app_window_set_error_status(context->tab->win,
                                        "Could not open exported PDF",
                                        error ? error->message : "No default PDF application");
        }
        return FALSE;
    }
    return TRUE;
}

void graptos_plugin_context_show_preview(GraptosPluginCommandContext *context,
                                         const char *title,
                                         const char *body) {
    if (!context || !context->tab || !context->tab->preview_buffer) return;
    EditorTab *tab = context->tab;
    g_free(tab->plugin_preview_title);
    g_free(tab->plugin_preview_body);
    tab->plugin_preview_title = g_strdup(title && title[0] ? title : "Plugin Preview");
    tab->plugin_preview_body = g_strdup(body ? body : "");
    tab->plugin_preview_active = TRUE;
    if (tab->win) tab->win->preview_enabled = TRUE;
    editor_tab_update_preview(tab);
    editor_tab_set_preview_visible(tab, TRUE);
    if (tab->win) {
        app_window_update_ui(tab->win);
        app_window_set_status(tab->win, "Plugin preview updated.");
    }
}

void graptos_plugin_context_set_preview_visible(GraptosPluginCommandContext *context,
                                                gboolean visible) {
    if (!context || !context->tab || !context->tab->win) return;
    context->tab->win->preview_enabled = visible;
    editor_tab_set_preview_visible(context->tab, visible);
    app_window_update_ui(context->tab->win);
}

gboolean graptos_plugin_context_export_latex_pdf(GraptosPluginCommandContext *context,
                                                 const char *basename,
                                                 const char *latex_source) {
    if (!context || !context->tab || !context->tab->win || !latex_source) {
        return FALSE;
    }
    EditorTab *tab = context->tab;
    if (!tab->file_path || !tab->file_path[0]) {
        app_window_set_error_status(tab->win,
                                    "Export requires a saved file",
                                    "Open or save the active file before exporting a plugin PDF.");
        return FALSE;
    }
    g_autofree char *command = graptos_latex_resolve_command(tab->win);
    if (!command) {
        app_window_set_error_status(tab->win,
                                    "LaTeX command not found",
                                    "Install pdflatex, xelatex, or lualatex, or set latex_command in config.ini.");
        return FALSE;
    }

    g_autofree char *dir = g_path_get_dirname(tab->file_path);
    g_autofree char *output_dir = g_build_filename(dir, ".graptos-beancount-build", NULL);
    if (g_mkdir_with_parents(output_dir, 0700) != 0) {
        app_window_set_error_status(tab->win,
                                    "Could not create export dir",
                                    "Graptoς could not create .graptos-beancount-build.");
        return FALSE;
    }

    g_autofree char *safe = plugin_safe_basename(basename);
    g_autofree char *tex_name = g_strdup_printf("%s.tex", safe);
    g_autofree char *pdf_name = g_strdup_printf("%s.pdf", safe);
    g_autofree char *tex_path = g_build_filename(output_dir, tex_name, NULL);
    g_autofree char *pdf_path = g_build_filename(output_dir, pdf_name, NULL);
    g_autoptr(GError) error = NULL;
    if (!write_text_atomic(tex_path, latex_source, &error)) {
        app_window_set_error_status(tab->win,
                                    "Could not write LaTeX report",
                                    error ? error->message : tex_path);
        return FALSE;
    }

    g_autofree char *stdout_text = NULL;
    g_autofree char *stderr_text = NULL;
    g_auto(GStrv) argv = graptos_latex_build_argv(tab->win,
                                                  command,
                                                  output_dir,
                                                  tex_path,
                                                  &error);
    if (!argv) {
        app_window_set_error_status(tab->win,
                                    "LaTeX arguments invalid",
                                    error ? error->message : "Could not parse latex_arguments.");
        return FALSE;
    }
    int status = 0;
    if (!g_spawn_sync(output_dir, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
                      &stdout_text, &stderr_text, &status, &error) ||
        status != 0) {
        g_autofree char *message = g_strdup_printf("%s\n%s",
                                                   error ? error->message : "LaTeX export failed.",
                                                   stderr_text ? stderr_text : "");
        app_window_set_error_status(tab->win, "LaTeX export failed", message);
        return FALSE;
    }
    if (!g_file_test(pdf_path, G_FILE_TEST_IS_REGULAR)) {
        app_window_set_error_status(tab->win,
                                    "Exported PDF not found",
                                    "LaTeX finished, but Graptoς could not find the report PDF.");
        return FALSE;
    }
    if (plugin_open_pdf(context, pdf_path)) {
        app_window_set_status(tab->win, "Plugin PDF exported.");
        return TRUE;
    }
    return FALSE;
}

void graptos_plugin_context_set_status(GraptosPluginCommandContext *context,
                                       const char *text) {
    if (!context || !context->tab || !context->tab->win) return;
    app_window_set_status(context->tab->win, text ? text : "");
}

void graptos_plugin_context_show_completions(GraptosPluginCommandContext *context,
                                             const char *replace_prefix,
                                             const char *source_label,
                                             GPtrArray *candidates) {
    if (!context || !context->tab) return;
    editor_tab_show_plugin_completion(context->tab,
                                      replace_prefix,
                                      source_label,
                                      candidates,
                                      TRUE);
}

guint graptos_plugin_context_tab_count(GraptosPluginCommandContext *context) {
    return context && context->tab && context->tab->win
        ? app_window_tab_count(context->tab->win)
        : 0u;
}

char *graptos_plugin_context_project_root(GraptosPluginCommandContext *context) {
    if (!context || !context->tab || !context->tab->win ||
        !context->tab->win->project_roots ||
        context->tab->win->project_roots->len == 0u) {
        return NULL;
    }
    const char *root = g_ptr_array_index(context->tab->win->project_roots, 0u);
    return root ? g_strdup(root) : NULL;
}

gboolean graptos_plugin_context_open_file(GraptosPluginCommandContext *context,
                                          const char *path) {
    if (!context || !context->tab || !context->tab->win || !path) return FALSE;
    return app_window_open_file(context->tab->win, path);
}

/**
 * @brief Count whitespace-delimited words.
 * @param text Text to inspect.
 * @return Word count.
 */
static guint plugin_count_words(const char *text) {
    guint words = 0u;
    gboolean in_word = FALSE;
    for (const char *p = text ? text : ""; *p; p = g_utf8_next_char(p)) {
        gunichar ch = g_utf8_get_char(p);
        if (g_unichar_isspace(ch)) {
            in_word = FALSE;
        } else if (!in_word) {
            words++;
            in_word = TRUE;
        }
    }
    return words;
}

/**
 * @brief Insert a section banner above one line.
 * @param tab Editor tab.
 * @param line One-based line number.
 */
static void plugin_insert_section_banner(EditorTab *tab, guint line) {
    if (!tab || !tab->buffer || line == 0u) return;
    const char *comment = tab->active_syntax && tab->active_syntax->line_comment
        ? tab->active_syntax->line_comment : "#";
    g_autofree char *banner = g_strdup_printf("%s ---- Section ----\n", comment);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line(tab->buffer, &iter, (gint)(line - 1u));
    gtk_text_buffer_insert(tab->buffer, &iter, banner, -1);
}

/**
 * @brief Run one declarative editor command.
 * @details V1 command ids are owned by Graptoς. Plugins expose where commands
 *          appear, while execution stays inside reviewed core code until the
 *          native permission UI exists.
 * @param action Menu action data.
 */
static void plugin_run_editor_command(GraptosPluginEditorMenuAction *action) {
    if (!action || !action->tab || !action->command) return;
    EditorTab *tab = action->tab;
    GraptosNativeCommand *native = action->plugin && action->plugin->native_commands
        ? g_hash_table_lookup(action->plugin->native_commands, action->command)
        : NULL;
    if (native && native->callback) {
        plugin_run_native_command(action->plugin, native, tab, action->line);
        return;
    }
    if (g_strcmp0(action->command, "git-blame-line") == 0) {
        if (!plugin_has_permission(action->plugin, "git")) {
            dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                          "Plugin Permission",
                          "Git Permission Required",
                          "This plugin command requires the git permission.");
            return;
        }
        g_autofree char *output = graptos_git_blame_line(tab->file_path,
                                                         action->line);
        g_autofree char *heading = g_strdup_printf("Git Blame: line %u",
                                                   action->line);
        dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                      "Git Blame",
                      heading,
                      output ? output : "Git blame returned no output.");
        return;
    }
    if (g_strcmp0(action->command, "line-word-count") == 0) {
        if (!plugin_has_permission(action->plugin, "editor.read")) {
            dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                          "Plugin Permission",
                          "Editor Read Permission Required",
                          "This plugin command requires the editor.read permission.");
            return;
        }
        g_autofree char *text = plugin_editor_line_text(tab, action->line);
        guint words = plugin_count_words(text);
        glong chars = g_utf8_strlen(text ? text : "", -1);
        g_autofree char *body = g_strdup_printf("Line: %u\nWords: %u\nCharacters: %ld\n\n%s",
                                                action->line,
                                                words,
                                                chars,
                                                text ? text : "");
        dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                      "Line Word Count",
                      "Line Word Count",
                      body);
        return;
    }
    if (g_strcmp0(action->command, "insert-section-banner") == 0) {
        if (!plugin_has_permission(action->plugin, "editor.write")) {
            dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                          "Plugin Permission",
                          "Editor Write Permission Required",
                          "This plugin command requires the editor.write permission.");
            return;
        }
        plugin_insert_section_banner(tab, action->line);
        return;
    }
    if (g_strcmp0(action->command, "project-summary") == 0) {
        if (!plugin_has_permission(action->plugin, "project.read")) {
            dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                          "Plugin Permission",
                          "Project Read Permission Required",
                          "This plugin command requires the project.read permission.");
            return;
        }
        guint roots = tab->win && tab->win->project_roots ? tab->win->project_roots->len : 0u;
        guint tabs = tab->win ? app_window_tab_count(tab->win) : 0u;
        g_autofree char *body = g_strdup_printf("Project roots: %u\nOpen tabs: %u\nCurrent file: %s",
                                                roots,
                                                tabs,
                                                tab->file_path ? tab->file_path : "Unsaved");
        dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                      "Project Summary",
                      "Project Summary",
                      body);
        return;
    }
    if (g_strcmp0(action->command, "show-plugin-info") == 0) {
        if (!plugin_has_permission(action->plugin, "ui")) {
            dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                          "Plugin Permission",
                          "UI Permission Required",
                          "This plugin command requires the ui permission.");
            return;
        }
        g_autofree char *body = g_strdup_printf("%s\n\n%s\nVersion: %s",
                                                action->plugin && action->plugin->name ? action->plugin->name : "Plugin",
                                                action->plugin && action->plugin->description ? action->plugin->description : "No description.",
                                                action->plugin && action->plugin->version ? action->plugin->version : "unknown");
        dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                      "Plugin Info",
                      "Plugin Info",
                      body);
        return;
    }

    g_autofree char *body = g_strdup_printf("Plugin command '%s' is not implemented.",
                                            action->command);
    dialog_output(tab->win ? app_window_gtk(tab->win) : NULL,
                  "Plugin Command",
                  "Unsupported Plugin Command",
                  body);
}

/**
 * @brief Handle a plugin editor context menu click.
 * @param button Menu button.
 * @param user_data GraptosPluginEditorMenuAction.
 */
static void plugin_editor_menu_clicked(GtkButton *button, gpointer user_data) {
    (void)button;
    GraptosPluginEditorMenuAction *action = user_data;
    if (action && action->popover && GTK_IS_POPOVER(action->popover)) {
        gtk_popover_popdown(GTK_POPOVER(action->popover));
    }
    plugin_run_editor_command(action);
}

/**
 * @brief Append one parsed plugin editor command button.
 * @details Ownership of the action is attached to the GTK button so command
 *          rows stay valid until the menu or tool panel is rebuilt.
 * @param plugin Plugin that owns the command.
 * @param tab Editor tab receiving the command.
 * @param box Box receiving the button.
 * @param popover Optional popover to close before running.
 * @param label Human-facing button label.
 * @param command Command id.
 * @param line One-based editor line.
 */
static void plugin_append_editor_command_button(GraptosPlugin *plugin,
                                                EditorTab *tab,
                                                GtkWidget *box,
                                                GtkWidget *popover,
                                                const char *label,
                                                const char *command,
                                                guint line) {
    GraptosPluginEditorMenuAction *action = g_new0(GraptosPluginEditorMenuAction, 1);
    action->plugin = plugin;
    action->tab = tab;
    action->popover = popover;
    action->label = g_strdup(label);
    action->command = g_strdup(command);
    action->line = line;

    GtkWidget *button = graptos_flat_button_new(label,
                                                NULL,
                                                G_CALLBACK(plugin_editor_menu_clicked),
                                                action);
    g_object_set_data_full(G_OBJECT(button),
                           "graptos-plugin-menu-action",
                           action,
                           plugin_editor_menu_action_free);
    gtk_box_append(GTK_BOX(box), button);
}

/**
 * @brief Append editor-line plugin menu specs.
 * @details The shared parser keeps context menus and the plugin tool panel on
 *          the same declarative command path.
 * @param registry Plugin registry to inspect.
 * @param tab Editor tab receiving commands.
 * @param box Box receiving buttons.
 * @param popover Optional popover to close before running.
 * @param line One-based editor line.
 * @param prefix_plugin_name TRUE to include plugin names in row labels.
 * @return Number of rows appended.
 */
static guint plugin_append_editor_line_specs(GraptosPluginRegistry *registry,
                                             EditorTab *tab,
                                             GtkWidget *box,
                                             GtkWidget *popover,
                                             guint line,
                                             gboolean prefix_plugin_name) {
    if (!registry || !registry->plugins || !tab || !box || line == 0u) {
        return 0u;
    }

    guint added = 0u;
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->enabled || !plugin->menus) continue;
        for (guint j = 0u; j < plugin->menus->len; j++) {
            const char *spec = g_ptr_array_index(plugin->menus, j);
            if (!spec || spec[0] == '\0') continue;
            g_auto(GStrv) parts = g_strsplit(spec, ":", 3);
            if (!parts || g_strcmp0(parts[0], "editor-line") != 0 ||
                !parts[1] || !parts[2] ||
                !plugin_declares_command(plugin, parts[2])) {
                continue;
            }
            g_autofree char *owned_label = prefix_plugin_name
                ? g_strdup_printf("%s: %s",
                                  plugin->name ? plugin->name : "Plugin",
                                  parts[1])
                : NULL;
            plugin_append_editor_command_button(plugin,
                                                tab,
                                                box,
                                                popover,
                                                owned_label ? owned_label : parts[1],
                                                parts[2],
                                                line);
            added++;
        }
        if (plugin->native_commands) {
            GHashTableIter iter;
            gpointer value = NULL;
            g_hash_table_iter_init(&iter, plugin->native_commands);
            while (g_hash_table_iter_next(&iter, NULL, &value)) {
                GraptosNativeCommand *native = value;
                if (!native || !native->editor_line || !native->label) continue;
                g_autofree char *owned_label = prefix_plugin_name
                    ? g_strdup_printf("%s: %s",
                                      plugin->name ? plugin->name : "Plugin",
                                      native->label)
                    : NULL;
                plugin_append_editor_command_button(plugin,
                                                    tab,
                                                    box,
                                                    popover,
                                                    owned_label ? owned_label : native->label,
                                                    native->command,
                                                    line);
                added++;
            }
        }
    }
    return added;
}

/**
 * @brief Return active cursor line for plugin command context.
 * @param tab Editor tab to inspect.
 * @return One-based cursor line.
 */
static guint plugin_tab_cursor_line(EditorTab *tab) {
    if (!tab || !tab->buffer) return 1u;
    GtkTextIter iter;
    GtkTextMark *insert = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &iter, insert);
    return (guint)gtk_text_iter_get_line(&iter) + 1u;
}

/**
 * @brief Run one native command against a tab.
 * @param plugin Plugin owning the command.
 * @param command Native command to run.
 * @param tab Editor tab receiving the command.
 */
static void plugin_run_native_command(GraptosPlugin *plugin,
                                      GraptosNativeCommand *command,
                                      EditorTab *tab,
                                      guint line) {
    if (!plugin || !command || !command->callback || !tab) return;
    GraptosPluginCommandContext context = {0};
    context.plugin = plugin;
    context.tab = tab;
    context.command = command->command;
    context.line = line ? line : plugin_tab_cursor_line(tab);
    command->callback(&context, command->user_data);
}
#else
const char *graptos_plugin_context_plugin_id(GraptosPluginCommandContext *context) {
    return context && context->plugin ? context->plugin->id : NULL;
}

const char *graptos_plugin_context_command_id(GraptosPluginCommandContext *context) {
    return context ? context->command : NULL;
}

guint graptos_plugin_context_line(GraptosPluginCommandContext *context) {
    return context ? context->line : 0u;
}

char *graptos_plugin_context_file_path(GraptosPluginCommandContext *context) {
    (void)context;
    return NULL;
}

char *graptos_plugin_context_text(GraptosPluginCommandContext *context) {
    (void)context;
    return g_strdup("");
}

char *graptos_plugin_context_selection(GraptosPluginCommandContext *context) {
    (void)context;
    return g_strdup("");
}

char *graptos_plugin_context_line_text(GraptosPluginCommandContext *context,
                                       guint line) {
    (void)context;
    (void)line;
    return g_strdup("");
}

char *graptos_plugin_context_line_prefix(GraptosPluginCommandContext *context) {
    (void)context;
    return g_strdup("");
}

const char *graptos_plugin_context_syntax_name(GraptosPluginCommandContext *context) {
    (void)context;
    return NULL;
}

gboolean graptos_plugin_context_is_modified(GraptosPluginCommandContext *context) {
    (void)context;
    return FALSE;
}

gboolean graptos_plugin_context_insert_text(GraptosPluginCommandContext *context,
                                            const char *text) {
    (void)context;
    (void)text;
    return FALSE;
}

gboolean graptos_plugin_context_replace_selection(GraptosPluginCommandContext *context,
                                                  const char *text) {
    (void)context;
    (void)text;
    return FALSE;
}

gboolean graptos_plugin_context_replace_text(GraptosPluginCommandContext *context,
                                             const char *text) {
    (void)context;
    (void)text;
    return FALSE;
}

void graptos_plugin_context_clear_diagnostics(GraptosPluginCommandContext *context) {
    (void)context;
}

gboolean graptos_plugin_context_add_line_diagnostic(GraptosPluginCommandContext *context,
                                                    guint line,
                                                    const char *message) {
    (void)context;
    (void)line;
    (void)message;
    return FALSE;
}

void graptos_plugin_context_show_output(GraptosPluginCommandContext *context,
                                        const char *title,
                                        const char *heading,
                                        const char *body) {
    (void)context;
    (void)title;
    (void)heading;
    (void)body;
}

void graptos_plugin_context_show_preview(GraptosPluginCommandContext *context,
                                         const char *title,
                                         const char *body) {
    (void)context;
    (void)title;
    (void)body;
}

void graptos_plugin_context_set_preview_visible(GraptosPluginCommandContext *context,
                                                gboolean visible) {
    (void)context;
    (void)visible;
}

gboolean graptos_plugin_context_export_latex_pdf(GraptosPluginCommandContext *context,
                                                 const char *basename,
                                                 const char *latex_source) {
    (void)context;
    (void)basename;
    (void)latex_source;
    return FALSE;
}

void graptos_plugin_context_set_status(GraptosPluginCommandContext *context,
                                       const char *text) {
    (void)context;
    (void)text;
}

void graptos_plugin_context_show_completions(GraptosPluginCommandContext *context,
                                             const char *replace_prefix,
                                             const char *source_label,
                                             GPtrArray *candidates) {
    (void)context;
    (void)replace_prefix;
    (void)source_label;
    (void)candidates;
}

guint graptos_plugin_context_tab_count(GraptosPluginCommandContext *context) {
    (void)context;
    return 0u;
}

char *graptos_plugin_context_project_root(GraptosPluginCommandContext *context) {
    (void)context;
    return NULL;
}

gboolean graptos_plugin_context_open_file(GraptosPluginCommandContext *context,
                                          const char *path) {
    (void)context;
    (void)path;
    return FALSE;
}
#endif /* GRAPTOS_PLUGIN_NO_UI */

GPtrArray *graptos_plugin_registry_contribution_dirs(GraptosPluginRegistry *registry,
                                                    GraptosPluginContributionKind kind) {
    GPtrArray *dirs = g_ptr_array_new_with_free_func(g_free);
    if (!dirs) return NULL;
    if (!registry || !registry->plugins) return dirs;
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->enabled || !plugin->base_path) continue;
        GPtrArray *items = plugin_contribution_array(plugin, kind);
        if (!items) continue;
        for (guint j = 0u; j < items->len; j++) {
            const char *item = g_ptr_array_index(items, j);
            if (!item || item[0] == '\0') continue;
            g_autofree char *path = g_path_is_absolute(item)
                ? g_strdup(item)
                : g_build_filename(plugin->base_path, item, NULL);
            if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
                g_ptr_array_add(dirs, g_canonicalize_filename(path, NULL));
            }
        }
    }
    return dirs;
}

gboolean graptos_plugin_registry_load_native(GraptosPluginRegistry *registry,
                                             GError **error) {
    if (!registry || !registry->plugins) return TRUE;
    if (!g_module_supported()) {
        g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                    GRAPTOS_PLUGIN_ERROR_NATIVE,
                    "Native plugin loading is not supported on this platform");
        return FALSE;
    }
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->enabled || !plugin->native_library ||
            plugin->native_library[0] == '\0') {
            continue;
        }
        if (plugin->native_handle) continue;
        if (!plugin_has_permission(plugin, "native")) {
            g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                        GRAPTOS_PLUGIN_ERROR_NATIVE,
                        "Plugin %s declares native code without native permission",
                        plugin->id);
            return FALSE;
        }
        g_autofree char *path = g_path_is_absolute(plugin->native_library)
            ? g_strdup(plugin->native_library)
            : g_build_filename(plugin->base_path, plugin->native_library, NULL);
        if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
            g_message("Plugin %s native library not found: %s",
                      plugin->id ? plugin->id : "(unknown)",
                      path);
            continue;
        }
        GModule *module = g_module_open(path, G_MODULE_BIND_LOCAL);
        if (!module) {
            g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                        GRAPTOS_PLUGIN_ERROR_NATIVE,
                        "Failed to load plugin %s: %s",
                        plugin->id,
                        g_module_error());
            return FALSE;
        }
        GraptosPluginRegisterFunc register_func = NULL;
        if (!g_module_symbol(module,
                             "graptos_plugin_register",
                             (gpointer *)&register_func) ||
            !register_func) {
            g_module_close(module);
            g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                        GRAPTOS_PLUGIN_ERROR_NATIVE,
                        "Plugin %s does not export graptos_plugin_register",
                        plugin->id);
            return FALSE;
        }
        GraptosPluginHost host = {0};
        host.plugin = plugin;
        if (!register_func(&host)) {
            g_module_close(module);
            g_set_error(error, GRAPTOS_PLUGIN_ERROR,
                        GRAPTOS_PLUGIN_ERROR_NATIVE,
                        "Plugin %s registration failed",
                        plugin->id);
            return FALSE;
        }
        plugin->native_handle = module;
    }
    return TRUE;
}

void graptos_plugin_append_editor_context_items(GraptosPluginRegistry *registry,
                                                EditorTab *tab,
                                                GtkWidget *menu_box,
                                                GtkWidget *popover,
                                                guint line) {
#ifdef GRAPTOS_PLUGIN_NO_UI
    (void)registry;
    (void)tab;
    (void)menu_box;
    (void)popover;
    (void)line;
#else
    if (!registry || !registry->plugins || !tab || !menu_box || line == 0u) {
        return;
    }

    GtkWidget *plugin_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    guint added = plugin_append_editor_line_specs(registry,
                                                 tab,
                                                 plugin_box,
                                                 popover,
                                                 line,
                                                 FALSE);
    if (added > 0u) {
        gtk_box_append(GTK_BOX(menu_box),
                       gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
        GtkWidget *child = gtk_widget_get_first_child(plugin_box);
        while (child) {
            GtkWidget *next = gtk_widget_get_next_sibling(child);
            g_object_ref(child);
            gtk_box_remove(GTK_BOX(plugin_box), child);
            gtk_box_append(GTK_BOX(menu_box), child);
            g_object_unref(child);
            child = next;
        }
    }
#endif
}

guint graptos_plugin_append_editor_tool_items(GraptosPluginRegistry *registry,
                                              EditorTab *tab,
                                              GtkWidget *box,
                                              guint line) {
#ifdef GRAPTOS_PLUGIN_NO_UI
    (void)registry;
    (void)tab;
    (void)box;
    (void)line;
    return 0u;
#else
    return plugin_append_editor_line_specs(registry,
                                           tab,
                                           box,
                                           NULL,
                                           line,
                                           TRUE);
#endif
}

guint graptos_plugin_append_editor_tool_menu_items(GraptosPluginRegistry *registry,
                                                   EditorTab *tab,
                                                   GtkWidget *box,
                                                   GtkWidget *popover,
                                                   guint line) {
#ifdef GRAPTOS_PLUGIN_NO_UI
    (void)registry;
    (void)tab;
    (void)box;
    (void)popover;
    (void)line;
    return 0u;
#else
    return plugin_append_editor_line_specs(registry,
                                           tab,
                                           box,
                                           popover,
                                           line,
                                           TRUE);
#endif
}

gboolean graptos_plugin_registry_run_shortcut(GraptosPluginRegistry *registry,
                                              EditorTab *tab,
                                              const char *shortcut,
                                              guint line) {
#ifdef GRAPTOS_PLUGIN_NO_UI
    (void)registry;
    (void)tab;
    (void)shortcut;
    (void)line;
    return FALSE;
#else
    if (!registry || !registry->plugins || !tab || !shortcut) return FALSE;
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->enabled || !plugin->native_commands) continue;
        GHashTableIter iter;
        gpointer value = NULL;
        g_hash_table_iter_init(&iter, plugin->native_commands);
        while (g_hash_table_iter_next(&iter, NULL, &value)) {
            GraptosNativeCommand *command = value;
            if (!command || g_strcmp0(command->shortcut, shortcut) != 0) {
                continue;
            }
            plugin_run_native_command(plugin, command, tab, line);
            return TRUE;
        }
    }
    return FALSE;
#endif
}

GPtrArray *graptos_plugin_registry_completion_candidates(GraptosPluginRegistry *registry,
                                                        EditorTab *tab,
                                                        char **replace_prefix_out,
                                                        char **source_label_out) {
#ifdef GRAPTOS_PLUGIN_NO_UI
    (void)registry;
    (void)tab;
    (void)replace_prefix_out;
    (void)source_label_out;
    return NULL;
#else
    if (replace_prefix_out) *replace_prefix_out = NULL;
    if (source_label_out) *source_label_out = NULL;
    if (!registry || !registry->plugins || !tab) return NULL;
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->enabled ||
            !plugin->native_completion_providers) {
            continue;
        }
        for (guint j = 0u; j < plugin->native_completion_providers->len; j++) {
            GraptosNativeCompletionProvider *provider =
                g_ptr_array_index(plugin->native_completion_providers, j);
            if (!provider || !provider->callback) continue;
            GraptosPluginCommandContext context = {0};
            context.plugin = plugin;
            context.tab = tab;
            context.command = provider->provider_id;
            context.line = plugin_tab_cursor_line(tab);
            g_autofree char *replace_prefix = NULL;
            GPtrArray *items = provider->callback(&context,
                                                  &replace_prefix,
                                                  provider->user_data);
            if (!items || items->len == 0u || !replace_prefix ||
                replace_prefix[0] == '\0') {
                if (items) g_ptr_array_free(items, TRUE);
                continue;
            }
            if (replace_prefix_out) {
                *replace_prefix_out = g_steal_pointer(&replace_prefix);
            }
            if (source_label_out) {
                *source_label_out = g_strdup(provider->label
                                             ? provider->label
                                             : "Plugin");
            }
            return items;
        }
    }
    return NULL;
#endif
}

char *graptos_plugin_registry_hover_text(GraptosPluginRegistry *registry,
                                         EditorTab *tab,
                                         const char *word,
                                         char **source_label_out) {
#ifdef GRAPTOS_PLUGIN_NO_UI
    (void)registry;
    (void)tab;
    (void)word;
    (void)source_label_out;
    return NULL;
#else
    if (source_label_out) *source_label_out = NULL;
    if (!registry || !registry->plugins || !tab || !word || !word[0]) {
        return NULL;
    }
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->enabled || !plugin->native_hover_providers) {
            continue;
        }
        for (guint j = 0u; j < plugin->native_hover_providers->len; j++) {
            GraptosNativeHoverProvider *provider =
                g_ptr_array_index(plugin->native_hover_providers, j);
            if (!provider || !provider->callback) continue;
            GraptosPluginCommandContext context = {0};
            context.plugin = plugin;
            context.tab = tab;
            context.command = provider->provider_id;
            context.line = plugin_tab_cursor_line(tab);
            char *body = provider->callback(&context, word, provider->user_data);
            if (!body || !body[0]) {
                g_free(body);
                continue;
            }
            if (source_label_out) {
                *source_label_out = g_strdup(provider->label
                                             ? provider->label
                                             : "Plugin");
            }
            return body;
        }
    }
    return NULL;
#endif
}

void graptos_plugin_hub_view_free(gpointer data) {
    GraptosPluginHubView *view = data;
    if (!view) return;
    g_free(view->plugin_id);
    g_free(view->hub_id);
    g_free(view->label);
    g_free(view);
}

GPtrArray *graptos_plugin_registry_hub_views(GraptosPluginRegistry *registry) {
    GPtrArray *views = g_ptr_array_new_with_free_func(graptos_plugin_hub_view_free);
    if (!views) return NULL;
    if (!registry || !registry->plugins) return views;
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->enabled || !plugin->native_hubs) continue;
        for (guint j = 0u; j < plugin->native_hubs->len; j++) {
            GraptosNativeHub *hub = g_ptr_array_index(plugin->native_hubs, j);
            if (!hub || !hub->render || !hub->action) continue;
            GraptosPluginHubView *view = g_new0(GraptosPluginHubView, 1);
            view->plugin_id = g_strdup(plugin->id);
            view->hub_id = g_strdup(hub->hub_id);
            view->label = g_strdup(hub->label ? hub->label : "Plugin Hub");
            g_ptr_array_add(views, view);
        }
    }
    return views;
}

#ifndef GRAPTOS_PLUGIN_NO_UI
static GraptosNativeHub *plugin_find_hub(GraptosPluginRegistry *registry,
                                         const char *plugin_id,
                                         const char *hub_id,
                                         GraptosPlugin **plugin_out) {
    if (plugin_out) *plugin_out = NULL;
    if (!registry || !registry->plugins || !plugin_id || !hub_id) return NULL;
    for (guint i = 0u; i < registry->plugins->len; i++) {
        GraptosPlugin *plugin = g_ptr_array_index(registry->plugins, i);
        if (!plugin || !plugin->enabled || g_strcmp0(plugin->id, plugin_id) != 0 ||
            !plugin->native_hubs) {
            continue;
        }
        for (guint j = 0u; j < plugin->native_hubs->len; j++) {
            GraptosNativeHub *hub = g_ptr_array_index(plugin->native_hubs, j);
            if (hub && g_strcmp0(hub->hub_id, hub_id) == 0) {
                if (plugin_out) *plugin_out = plugin;
                return hub;
            }
        }
    }
    return NULL;
}
#endif

char *graptos_plugin_registry_render_hub(GraptosPluginRegistry *registry,
                                         EditorTab *tab,
                                         const char *plugin_id,
                                         const char *hub_id) {
#ifdef GRAPTOS_PLUGIN_NO_UI
    (void)registry;
    (void)tab;
    (void)plugin_id;
    (void)hub_id;
    return NULL;
#else
    GraptosPlugin *plugin = NULL;
    GraptosNativeHub *hub = plugin_find_hub(registry, plugin_id, hub_id, &plugin);
    if (!hub || !hub->render || !plugin || !tab) return NULL;
    GraptosPluginCommandContext context = {0};
    context.plugin = plugin;
    context.tab = tab;
    context.command = hub->hub_id;
    context.line = plugin_tab_cursor_line(tab);
    return hub->render(&context, hub->user_data);
#endif
}

gboolean graptos_plugin_registry_run_hub_action(GraptosPluginRegistry *registry,
                                                EditorTab *tab,
                                                const char *plugin_id,
                                                const char *hub_id,
                                                const char *action_id,
                                                const char *input) {
#ifdef GRAPTOS_PLUGIN_NO_UI
    (void)registry;
    (void)tab;
    (void)plugin_id;
    (void)hub_id;
    (void)action_id;
    (void)input;
    return FALSE;
#else
    GraptosPlugin *plugin = NULL;
    GraptosNativeHub *hub = plugin_find_hub(registry, plugin_id, hub_id, &plugin);
    if (!hub || !hub->action || !plugin || !tab || !action_id) return FALSE;
    GraptosPluginCommandContext context = {0};
    context.plugin = plugin;
    context.tab = tab;
    context.command = hub->hub_id;
    context.line = plugin_tab_cursor_line(tab);
    hub->action(&context, action_id, input, hub->user_data);
    return TRUE;
#endif
}
