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
#include "dialogs.h"
#include "editor_tab.h"
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
    plugin->graptos_api_version = GRAPTOS_PLUGIN_API_VERSION;
    plugin->permissions = g_ptr_array_new_with_free_func(g_free);
    plugin->syntax_dirs = g_ptr_array_new_with_free_func(g_free);
    plugin->theme_dirs = g_ptr_array_new_with_free_func(g_free);
    plugin->template_dirs = g_ptr_array_new_with_free_func(g_free);
    plugin->snippet_dirs = g_ptr_array_new_with_free_func(g_free);
    plugin->commands = g_ptr_array_new_with_free_func(g_free);
    plugin->menus = g_ptr_array_new_with_free_func(g_free);
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

    gboolean added_any = FALSE;
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
            if (!added_any) {
                gtk_box_append(GTK_BOX(menu_box),
                               gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
                added_any = TRUE;
            }

            GraptosPluginEditorMenuAction *action = g_new0(GraptosPluginEditorMenuAction, 1);
            action->plugin = plugin;
            action->tab = tab;
            action->popover = popover;
            action->label = g_strdup(parts[1]);
            action->command = g_strdup(parts[2]);
            action->line = line;

            GtkWidget *button = graptos_flat_button_new(parts[1],
                                                        NULL,
                                                        G_CALLBACK(plugin_editor_menu_clicked),
                                                        action);
            g_object_set_data_full(G_OBJECT(button),
                                   "graptos-plugin-menu-action",
                                   action,
                                   plugin_editor_menu_action_free);
            gtk_box_append(GTK_BOX(menu_box), button);
        }
    }
#endif
}
