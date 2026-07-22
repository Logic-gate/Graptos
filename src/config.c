/**
 * @file src/config.c
 * @brief Persistent Graptoς configuration loading and saving.
 * @details Configuration is the shared contract between defaults, manual edits, and live
 *          theme changes. We parse it once here so individual features do not invent their
 *          own config behavior.
 */

#include "config.h"

#include <errno.h>
#include <string.h>

/**
 * @brief Parse bool.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @param key_file The key file supplied by the caller.
 * @param key The key supplied by the caller.
 * @param fallback The fallback supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean parse_bool(GKeyFile *key_file, const char *key, gboolean fallback) {
    g_autoptr(GError) error = NULL;
    gboolean value = g_key_file_get_boolean(key_file, "Editor", key, &error);
    if (error) {
        return fallback;
    }
    return value;
}

/**
 * @brief Parse uint.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @param key_file The key file supplied by the caller.
 * @param key The key supplied by the caller.
 * @param fallback The fallback supplied by the caller.
 * @param min_value The min value supplied by the caller.
 * @param max_value The max value supplied by the caller.
 * @return The computed value requested by the caller.
 */
static guint parse_uint(GKeyFile *key_file, const char *key, guint fallback, guint min_value, guint max_value) {
    g_autoptr(GError) error = NULL;
    gint value = g_key_file_get_integer(key_file, "Editor", key, &error);
    if (error) {
        return fallback;
    }
    if (value < (gint)min_value) return min_value;
    if (value > (gint)max_value) return max_value;
    return (guint)value;
}

/**
 * @brief Load color.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @param key_file The key file supplied by the caller.
 * @param key The key supplied by the caller.
 * @param slot The slot supplied by the caller.
 */
static void load_color(GKeyFile *key_file, const char *key, char **slot) {
    if (!key_file || !key || !slot) return;
    char *value = g_key_file_get_string(key_file, "Editor", key, NULL);
    if (value && value[0] != '\0' && gdk_rgba_parse(&(GdkRGBA){0}, value)) {
        g_free(*slot);
        *slot = value;
        return;
    }
    g_free(value);
}

/**
 * @brief Save color.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @param key_file The key file supplied by the caller.
 * @param key The key supplied by the caller.
 * @param value The value being parsed, stored, or applied.
 */
static void save_color(GKeyFile *key_file, const char *key, const char *value) {
    if (!key_file || !key || !value || value[0] == '\0') return;
    g_key_file_set_string(key_file, "Editor", key, value);
}

/**
 * @brief Load a string config value.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @param key_file The key file supplied by the caller.
 * @param key The key supplied by the caller.
 * @param slot The slot supplied by the caller.
 */
static void load_string(GKeyFile *key_file, const char *key, char **slot) {
    if (!key_file || !key || !slot) return;
    g_autoptr(GError) error = NULL;
    char *value = g_key_file_get_string(key_file, "Editor", key, &error);
    if (error || !value) {
        g_free(value);
        return;
    }
    g_free(*slot);
    *slot = value;
}

/**
 * @brief Save a string config value.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @param key_file The key file supplied by the caller.
 * @param key The key supplied by the caller.
 * @param value The value being parsed, stored, or applied.
 */
static void save_string(GKeyFile *key_file, const char *key, const char *value) {
    if (!key_file || !key || !value) return;
    g_key_file_set_string(key_file, "Editor", key, value);
}

/**
 * @brief Report a configuration save failure without interrupting shutdown.
 * @details Config writes can happen from live theme changes and from window
 *          teardown. During normal use we expose the failure through the status
 *          bar; during close we only log it because opening UI while GTK is
 *          destroying the window can create harder shutdown bugs.
 * @param win The window that owns status/debug state.
 * @param path The config file path that could not be written.
 * @param detail The detailed failure text.
 */
static void report_config_save_failure(EditorWindow *win,
                                       const char *path,
                                       const char *detail) {
    const char *message = detail && detail[0] != '\0'
        ? detail
        : "Unknown config write error";
    if (win && win->debug_mode) {
        g_message("Config save failed path=%s: %s",
                  path ? path : "(none)",
                  message);
    }
    if (win && !win->closing) {
        app_window_set_error_status(win, "Config save failed", message);
    }
}

/**
 * @brief Write a key file to the Graptoς config path.
 * @details Config writes happen from normal saves and from migration. Keeping
 *          the disk write in one helper makes both paths report failures the
 *          same way and prevents startup migration from inventing a second
 *          cleanup path.
 * @param win The window used for status and debug reporting.
 * @param key_file The fully prepared key file.
 * @param path The destination path.
 * @return TRUE when the config reached disk; otherwise FALSE.
 */
static gboolean write_config_key_file(EditorWindow *win,
                                      GKeyFile *key_file,
                                      const char *path) {
    if (!key_file || !path) return FALSE;

    g_autofree char *dir = g_path_get_dirname(path);
    if (!dir) {
        report_config_save_failure(win, path, "Could not resolve config directory");
        return FALSE;
    }
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        g_autofree char *message =
            g_strdup_printf("Could not create %s: %s", dir, g_strerror(errno));
        report_config_save_failure(win, path, message);
        return FALSE;
    }

    gsize length = 0u;
    g_autoptr(GError) serialize_error = NULL;
    g_autofree char *data = g_key_file_to_data(key_file, &length, &serialize_error);
    if (!data) {
        report_config_save_failure(win,
                                   path,
                                   serialize_error ? serialize_error->message : NULL);
        return FALSE;
    }

    g_autoptr(GError) write_error = NULL;
    if (!g_file_set_contents(path, data, (gssize)length, &write_error)) {
        report_config_save_failure(win,
                                   path,
                                   write_error ? write_error->message : NULL);
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Populate a key file with the complete known Graptoς config shape.
 * @details This helper is the single list of keys Graptoς knows how to write.
 *          Normal saves use it to write the full config. Startup migration uses
 *          it as the source of missing default keys, so new builds do not leave
 *          older config files half-known.
 * @param win The window whose effective config values should be serialized.
 * @param key_file The key file that receives the known settings.
 */
static void populate_config_key_file(EditorWindow *win, GKeyFile *key_file) {
    if (!win || !key_file) return;

    save_color(key_file, "background_color", win->editor_bg_color);
    save_color(key_file, "foreground_color", win->editor_fg_color);
    save_color(key_file, "editor_gutter_background_color", win->editor_gutter_bg_color);
    save_color(key_file, "editor_gutter_foreground_color", win->editor_gutter_fg_color);
    save_color(key_file, "editor_current_line_background_color", win->editor_current_line_bg_color);
    save_color(key_file, "editor_selection_background_color", win->editor_selection_bg_color);
    save_color(key_file, "editor_selection_foreground_color", win->editor_selection_fg_color);
    save_color(key_file, "editor_cursor_color", win->editor_cursor_color);
    save_color(key_file, "sidebar_background_color", win->sidebar_bg_color);
    save_color(key_file, "tabbar_background_color", win->tabbar_bg_color);
    save_color(key_file, "tabbar_foreground_color", win->tabbar_fg_color);
    save_color(key_file, "tab_active_background_color", win->tab_active_bg_color);
    save_color(key_file, "tab_active_foreground_color", win->tab_active_fg_color);
    save_color(key_file, "topbar_background_color", win->topbar_bg_color);
    save_color(key_file, "topbar_foreground_color", win->topbar_fg_color);
    save_color(key_file, "bottombar_background_color", win->bottombar_bg_color);
    save_color(key_file, "bottombar_foreground_color", win->bottombar_fg_color);
    save_color(key_file, "status_error_color", win->status_error_color);
    save_color(key_file, "button_background_color", win->button_bg_color);
    save_color(key_file, "button_foreground_color", win->button_fg_color);
    save_color(key_file, "button_hover_background_color", win->button_hover_bg_color);
    save_color(key_file, "button_active_background_color", win->button_active_bg_color);
    save_color(key_file, "input_background_color", win->input_bg_color);
    save_color(key_file, "input_foreground_color", win->input_fg_color);
    save_color(key_file, "input_border_color", win->input_border_color);
    save_color(key_file, "project_tree_foreground_color", win->project_tree_fg_color);
    save_color(key_file, "project_tree_selected_background_color", win->project_tree_selected_bg_color);
    save_color(key_file, "project_tree_selected_foreground_color", win->project_tree_selected_fg_color);
    save_color(key_file, "git_status_modified_color", win->git_status_modified_color);
    save_color(key_file, "git_status_added_color", win->git_status_added_color);
    save_color(key_file, "git_status_deleted_color", win->git_status_deleted_color);
    save_color(key_file, "git_status_renamed_color", win->git_status_renamed_color);
    save_color(key_file, "git_status_conflict_color", win->git_status_conflict_color);
    save_color(key_file, "git_status_untracked_color", win->git_status_untracked_color);
    save_color(key_file, "git_status_staged_color", win->git_status_staged_color);
    save_color(key_file, "scroll_preview_background_color", win->scroll_preview_bg_color);
    save_color(key_file, "scroll_preview_foreground_color", win->scroll_preview_fg_color);
    save_color(key_file, "popover_background_color", win->popover_bg_color);
    save_color(key_file, "popover_border_color", win->popover_border_color);
    save_color(key_file, "tooltip_background_color", win->tooltip_bg_color);
    save_color(key_file, "tooltip_foreground_color", win->tooltip_fg_color);
    save_color(key_file, "tooltip_border_color", win->tooltip_border_color);
    save_color(key_file, "ref_popover_background_color", win->ref_popover_bg_color);
    save_color(key_file, "ref_popover_foreground_color", win->ref_popover_fg_color);
    save_color(key_file, "ref_popover_heading_color", win->ref_popover_heading_color);
    save_color(key_file, "ref_popover_title_color", win->ref_popover_title_color);
    save_color(key_file, "ref_popover_kind_color", win->ref_popover_kind_color);
    save_color(key_file, "ref_popover_snippet_color", win->ref_popover_snippet_color);
    save_color(key_file, "ref_popover_hover_background_color", win->ref_popover_hover_bg_color);
    save_color(key_file, "ref_popover_hover_foreground_color", win->ref_popover_hover_fg_color);
    save_color(key_file, "autocomplete_popover_background_color", win->completion_popover_bg_color);
    save_color(key_file, "autocomplete_popover_foreground_color", win->completion_popover_fg_color);
    save_color(key_file, "autocomplete_popover_detail_color", win->completion_popover_detail_color);
    save_color(key_file, "autocomplete_selection_background_color", win->completion_selection_bg_color);
    save_color(key_file, "autocomplete_selection_foreground_color", win->completion_selection_fg_color);
    save_color(key_file, "dialog_background_color", win->dialog_bg_color);
    save_color(key_file, "dialog_foreground_color", win->dialog_fg_color);
    save_color(key_file, "dialog_border_color", win->dialog_border_color);
    save_color(key_file, "dialog_title_color", win->dialog_title_color);
    save_color(key_file, "dialog_body_color", win->dialog_body_color);
    save_color(key_file, "dialog_muted_color", win->dialog_muted_color);
    save_color(key_file, "dialog_output_color", win->dialog_output_color);
    save_color(key_file, "git_output_background_color", win->git_output_bg_color);
    save_color(key_file, "dialog_action_color", win->dialog_action_color);
    save_color(key_file, "dialog_destructive_action_color",
               win->dialog_destructive_action_color);
    save_color(key_file, "dialog_input_foreground_color",
               win->dialog_input_fg_color);
    save_color(key_file, "dialog_input_background_color",
               win->dialog_input_bg_color);
    save_color(key_file, "search_match_background_color", win->search_match_bg_color);
    save_color(key_file, "search_match_foreground_color", win->search_match_fg_color);
    save_color(key_file, "diagnostic_warning_background_color", win->diagnostic_warning_bg_color);
    save_color(key_file, "diagnostic_warning_foreground_color", win->diagnostic_warning_fg_color);
    save_color(key_file, "codex_preview_background_color", win->codex_preview_bg_color);
    save_color(key_file, "codex_preview_foreground_color", win->codex_preview_fg_color);
    save_color(key_file, "codex_prompt_background_color", win->codex_prompt_bg_color);
    save_string(key_file, "ui_font", win->ui_font ? win->ui_font : "");
    save_string(key_file, "editor_font", win->editor_font ? win->editor_font : "");
    save_string(key_file, "preview_font", win->preview_font ? win->preview_font : "");
    save_string(key_file, "terminal_font", win->terminal_font ? win->terminal_font : "");
    save_string(key_file, "code_font", win->code_font ? win->code_font : "");
    g_key_file_set_boolean(key_file, "Editor", "use_system_interface_font", win->use_system_interface_font);
    g_key_file_set_boolean(key_file, "Editor", "autocomplete_enabled", win->autocomplete_enabled);
    g_key_file_set_boolean(key_file, "Editor", "regex_tester_enabled", win->regex_tester_enabled);
    g_key_file_set_boolean(key_file, "Editor", "diagnostics_enabled", win->diagnostics_enabled);
    g_key_file_set_boolean(key_file, "Editor", "terminal_dynamic_directory", win->terminal_dynamic_directory);
    g_key_file_set_boolean(key_file, "Editor", "auto_save_enabled", win->auto_save_enabled);
    g_key_file_set_integer(key_file, "Editor", "auto_save_interval_seconds", (gint)win->auto_save_interval_seconds);
    g_key_file_set_boolean(key_file, "Editor", "backup_enabled", win->backup_enabled);
    g_key_file_set_boolean(key_file, "Editor", "insert_spaces", win->insert_spaces);
    g_key_file_set_integer(key_file, "Editor", "tab_width", (gint)win->tab_width);
    g_key_file_set_integer(key_file, "Editor", "tile_max_tabs", (gint)win->tile_max_tabs);
    g_key_file_set_integer(key_file, "Editor", "lsp_completion_max_results", (gint)win->lsp_completion_max_results);
    g_key_file_set_integer(key_file, "Editor", "lsp_completion_max_retries", (gint)win->lsp_completion_max_retries);
    g_key_file_set_integer(key_file, "Editor", "lsp_completion_retry_delay_ms", (gint)win->lsp_completion_retry_delay_ms);
    g_key_file_set_integer(key_file, "Editor", "lsp_references_max_results", (gint)win->lsp_references_max_results);
    g_key_file_set_integer(key_file, "Editor", "lsp_change_delay_ms", (gint)win->lsp_change_delay_ms);
    g_key_file_set_boolean(key_file, "Editor", "scroll_preview_enabled", win->minimap_enabled);
    g_key_file_set_boolean(key_file, "Editor", "preview_enabled", win->preview_enabled);
    g_key_file_set_boolean(key_file, "Editor", "use_gtksourceview_highlighting", TRUE);
    g_key_file_set_boolean(key_file, "Editor", "use_yaml_style_overrides", win->use_yaml_style_overrides);
}

/**
 * @brief Copy one default key only when the user config does not define it.
 * @details Missing-key migration must be conservative. A present value stays
 *          untouched even when it is unusual, because manual config edits are
 *          user ownership.
 * @param target The user config being migrated.
 * @param defaults The current complete default/effective config.
 * @param group The key group to inspect.
 * @param key The key to copy if absent.
 * @return TRUE when a new key was added; otherwise FALSE.
 */
static gboolean copy_missing_config_key(GKeyFile *target,
                                        GKeyFile *defaults,
                                        const char *group,
                                        const char *key) {
    if (!target || !defaults || !group || !key) return FALSE;
    if (g_key_file_has_key(target, group, key, NULL)) return FALSE;

    g_autoptr(GError) error = NULL;
    g_autofree char *value = g_key_file_get_value(defaults, group, key, &error);
    if (error || !value) return FALSE;

    g_key_file_set_value(target, group, key, value);
    return TRUE;
}

/**
 * @brief Backfill missing config keys after loading an existing file.
 * @details New builds add new config keys over time. This scans the loaded file
 *          and appends only absent keys using the effective values already
 *          loaded into the window, so existing user choices remain unchanged.
 * @param win The window whose effective settings provide migration defaults.
 * @param key_file The loaded user config.
 * @param path The config path to update when migration changed the file.
 */
static void backfill_missing_config_keys(EditorWindow *win,
                                         GKeyFile *key_file,
                                         const char *path) {
    if (!win || !key_file || !path) return;

    g_autoptr(GKeyFile) defaults = g_key_file_new();
    populate_config_key_file(win, defaults);

    gboolean changed = FALSE;
    gsize group_count = 0u;
    gchar **groups = g_key_file_get_groups(defaults, &group_count);
    for (gsize group_index = 0u; group_index < group_count; group_index++) {
        gsize key_count = 0u;
        gchar **keys = g_key_file_get_keys(defaults, groups[group_index], &key_count, NULL);
        for (gsize key_index = 0u; key_index < key_count; key_index++) {
            changed |= copy_missing_config_key(key_file,
                                               defaults,
                                               groups[group_index],
                                               keys[key_index]);
        }
        g_strfreev(keys);
    }
    g_strfreev(groups);

    if (!changed) return;
    if (write_config_key_file(win, key_file, path) && win->debug_mode) {
        g_message("Config migration: added missing keys to %s", path);
    }
}

/**
 * @brief Replace removed legacy bundled font names with current defaults.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @param slot The slot supplied by the caller.
 */
static void migrate_removed_font(char **slot) {
    if (!slot || !*slot) return;
    if (g_str_has_prefix(*slot, "Space Mono")) {
        const char *suffix = *slot + strlen("Space Mono");
        char *replacement = g_strdup_printf("Inconsolata%s", suffix);
        g_free(*slot);
        *slot = replacement;
    }
}

/**
 * @brief Graptoς config path.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_config_path(void) {
    const char *base = g_get_user_config_dir();
    if (!base || base[0] == '\0') return NULL;
    return g_build_filename(base, "graptos", "config.ini", NULL);
}

/**
 * @brief Graptoς config load.
 * @details Missing config is not an error; first launch should just use
 *          defaults. When the file is present, we layer values onto the already
 *          initialized window so new config keys automatically keep defaults.
 * @param win The win supplied by the caller.
 */
void graptos_config_load(EditorWindow *win) {
    if (!win) return;
    g_autofree char *path = graptos_config_path();
    if (!path) return;
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        graptos_config_save(win);
        return;
    }

    g_autoptr(GKeyFile) key_file = g_key_file_new();
    g_autoptr(GError) error = NULL;
    if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_KEEP_COMMENTS, &error)) {
        return;
    }

    load_color(key_file, "background_color", &win->editor_bg_color);
    load_color(key_file, "foreground_color", &win->editor_fg_color);
    load_color(key_file, "editor_gutter_background_color", &win->editor_gutter_bg_color);
    load_color(key_file, "editor_gutter_foreground_color", &win->editor_gutter_fg_color);
    load_color(key_file, "editor_current_line_background_color", &win->editor_current_line_bg_color);
    load_color(key_file, "editor_selection_background_color", &win->editor_selection_bg_color);
    load_color(key_file, "editor_selection_foreground_color", &win->editor_selection_fg_color);
    load_color(key_file, "editor_cursor_color", &win->editor_cursor_color);
    load_color(key_file, "sidebar_background_color", &win->sidebar_bg_color);
    load_color(key_file, "tabbar_background_color", &win->tabbar_bg_color);
    load_color(key_file, "tabbar_foreground_color", &win->tabbar_fg_color);
    load_color(key_file, "tab_active_background_color", &win->tab_active_bg_color);
    load_color(key_file, "tab_active_foreground_color", &win->tab_active_fg_color);
    load_color(key_file, "topbar_background_color", &win->topbar_bg_color);
    load_color(key_file, "topbar_foreground_color", &win->topbar_fg_color);
    load_color(key_file, "bottombar_background_color", &win->bottombar_bg_color);
    load_color(key_file, "bottombar_foreground_color", &win->bottombar_fg_color);
    load_color(key_file, "status_error_color", &win->status_error_color);
    load_color(key_file, "button_background_color", &win->button_bg_color);
    load_color(key_file, "button_foreground_color", &win->button_fg_color);
    load_color(key_file, "button_hover_background_color", &win->button_hover_bg_color);
    load_color(key_file, "button_active_background_color", &win->button_active_bg_color);
    load_color(key_file, "input_background_color", &win->input_bg_color);
    load_color(key_file, "input_foreground_color", &win->input_fg_color);
    load_color(key_file, "input_border_color", &win->input_border_color);
    load_color(key_file, "project_tree_foreground_color", &win->project_tree_fg_color);
    load_color(key_file, "project_tree_selected_background_color", &win->project_tree_selected_bg_color);
    load_color(key_file, "project_tree_selected_foreground_color", &win->project_tree_selected_fg_color);
    load_color(key_file, "git_status_modified_color", &win->git_status_modified_color);
    load_color(key_file, "git_status_added_color", &win->git_status_added_color);
    load_color(key_file, "git_status_deleted_color", &win->git_status_deleted_color);
    load_color(key_file, "git_status_renamed_color", &win->git_status_renamed_color);
    load_color(key_file, "git_status_conflict_color", &win->git_status_conflict_color);
    load_color(key_file, "git_status_untracked_color", &win->git_status_untracked_color);
    load_color(key_file, "git_status_staged_color", &win->git_status_staged_color);
    load_color(key_file, "scroll_preview_background_color", &win->scroll_preview_bg_color);
    load_color(key_file, "scroll_preview_foreground_color", &win->scroll_preview_fg_color);
    load_color(key_file, "popover_background_color", &win->popover_bg_color);
    load_color(key_file, "popover_border_color", &win->popover_border_color);
    load_color(key_file, "tooltip_background_color", &win->tooltip_bg_color);
    load_color(key_file, "tooltip_foreground_color", &win->tooltip_fg_color);
    load_color(key_file, "tooltip_border_color", &win->tooltip_border_color);
    load_color(key_file, "ref_popover_background_color", &win->ref_popover_bg_color);
    load_color(key_file, "ref_popover_foreground_color", &win->ref_popover_fg_color);
    load_color(key_file, "ref_popover_heading_color", &win->ref_popover_heading_color);
    load_color(key_file, "ref_popover_title_color", &win->ref_popover_title_color);
    load_color(key_file, "ref_popover_kind_color", &win->ref_popover_kind_color);
    load_color(key_file, "ref_popover_snippet_color", &win->ref_popover_snippet_color);
    load_color(key_file, "ref_popover_hover_background_color", &win->ref_popover_hover_bg_color);
    load_color(key_file, "ref_popover_hover_foreground_color", &win->ref_popover_hover_fg_color);
    load_color(key_file, "autocomplete_popover_background_color", &win->completion_popover_bg_color);
    load_color(key_file, "autocomplete_popover_foreground_color", &win->completion_popover_fg_color);
    load_color(key_file, "autocomplete_popover_detail_color", &win->completion_popover_detail_color);
    load_color(key_file, "autocomplete_selection_background_color", &win->completion_selection_bg_color);
    load_color(key_file, "autocomplete_selection_foreground_color", &win->completion_selection_fg_color);
    load_color(key_file, "dialog_background_color", &win->dialog_bg_color);
    load_color(key_file, "dialog_foreground_color", &win->dialog_fg_color);
    load_color(key_file, "dialog_border_color", &win->dialog_border_color);
    load_color(key_file, "dialog_title_color", &win->dialog_title_color);
    load_color(key_file, "dialog_body_color", &win->dialog_body_color);
    load_color(key_file, "dialog_muted_color", &win->dialog_muted_color);
    load_color(key_file, "dialog_output_color", &win->dialog_output_color);
    load_color(key_file, "git_output_background_color", &win->git_output_bg_color);
    load_color(key_file, "dialog_action_color", &win->dialog_action_color);
    load_color(key_file, "dialog_destructive_action_color",
               &win->dialog_destructive_action_color);
    load_color(key_file, "dialog_input_foreground_color",
               &win->dialog_input_fg_color);
    load_color(key_file, "dialog_input_background_color",
               &win->dialog_input_bg_color);
    load_color(key_file, "search_match_background_color", &win->search_match_bg_color);
    load_color(key_file, "search_match_foreground_color", &win->search_match_fg_color);
    load_color(key_file, "diagnostic_warning_background_color", &win->diagnostic_warning_bg_color);
    load_color(key_file, "diagnostic_warning_foreground_color", &win->diagnostic_warning_fg_color);
    load_color(key_file, "codex_preview_background_color", &win->codex_preview_bg_color);
    load_color(key_file, "codex_preview_foreground_color", &win->codex_preview_fg_color);
    load_color(key_file, "codex_prompt_background_color", &win->codex_prompt_bg_color);
    load_string(key_file, "ui_font", &win->ui_font);
    load_string(key_file, "editor_font", &win->editor_font);
    load_string(key_file, "preview_font", &win->preview_font);
    load_string(key_file, "terminal_font", &win->terminal_font);
    load_string(key_file, "code_font", &win->code_font);
    migrate_removed_font(&win->ui_font);
    migrate_removed_font(&win->editor_font);
    migrate_removed_font(&win->preview_font);
    migrate_removed_font(&win->terminal_font);
    migrate_removed_font(&win->code_font);

    win->use_system_interface_font = parse_bool(key_file, "use_system_interface_font", win->use_system_interface_font);
    win->autocomplete_enabled = parse_bool(key_file, "autocomplete_enabled", win->autocomplete_enabled);
    win->regex_tester_enabled = parse_bool(key_file, "regex_tester_enabled", win->regex_tester_enabled);
    win->diagnostics_enabled = parse_bool(key_file, "diagnostics_enabled", win->diagnostics_enabled);
    win->terminal_dynamic_directory = parse_bool(key_file, "terminal_dynamic_directory", win->terminal_dynamic_directory);
    win->auto_save_enabled = parse_bool(key_file, "auto_save_enabled", win->auto_save_enabled);
    win->auto_save_interval_seconds = parse_uint(key_file, "auto_save_interval_seconds", win->auto_save_interval_seconds, 1u, 3600u);
    win->backup_enabled = parse_bool(key_file, "backup_enabled", win->backup_enabled);
    win->insert_spaces = parse_bool(key_file, "insert_spaces", win->insert_spaces);
    win->tab_width = parse_uint(key_file, "tab_width", win->tab_width, 1u, 16u);
    win->tile_max_tabs = parse_uint(key_file, "tile_max_tabs", win->tile_max_tabs, 1u, 8u);
    win->lsp_completion_max_results = parse_uint(key_file, "lsp_completion_max_results", win->lsp_completion_max_results, 8u, 512u);
    win->lsp_completion_max_retries = parse_uint(key_file, "lsp_completion_max_retries", win->lsp_completion_max_retries, 0u, 8u);
    win->lsp_completion_retry_delay_ms = parse_uint(key_file, "lsp_completion_retry_delay_ms", win->lsp_completion_retry_delay_ms, 20u, 2000u);
    win->lsp_references_max_results = parse_uint(key_file, "lsp_references_max_results", win->lsp_references_max_results, 1u, 1000u);
    win->lsp_change_delay_ms = parse_uint(key_file, "lsp_change_delay_ms", win->lsp_change_delay_ms, 50u, 5000u);
    win->minimap_enabled = parse_bool(key_file, "scroll_preview_enabled", win->minimap_enabled);
    win->preview_enabled = parse_bool(key_file, "preview_enabled", win->preview_enabled);
    win->use_gtksourceview_highlighting = TRUE;
    win->use_yaml_style_overrides = parse_bool(key_file, "use_yaml_style_overrides", win->use_yaml_style_overrides);

    if (!win->codex_preview_bg_color) {
        win->codex_preview_bg_color = g_strdup(win->editor_bg_color);
    }
    if (!win->codex_preview_fg_color) {
        win->codex_preview_fg_color = g_strdup("#d4d4d4");
    }
    if (!win->codex_prompt_bg_color) {
        win->codex_prompt_bg_color = g_strdup(win->editor_bg_color);
    }

    backfill_missing_config_keys(win, key_file, path);
}

/**
 * @brief Graptoς config save.
 * @details Saving writes the whole known config shape, not only dirty fields.
 *          That makes manual edits easier to discover and keeps newly added
 *          theme/config keys visible after the next preferences save.
 * @param win The win supplied by the caller.
 */
void graptos_config_save(EditorWindow *win) {
    if (!win) return;
    g_autofree char *path = graptos_config_path();
    if (!path) return;

    g_autoptr(GKeyFile) key_file = g_key_file_new();
    populate_config_key_file(win, key_file);
    (void)write_config_key_file(win, key_file, path);
}
