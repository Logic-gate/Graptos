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
 * @brief Return the default CSS theme path.
 * @details CSS themes live next to config.ini because they are user-editable
 *          runtime state, not compiled resources.
 * @return An owned path, or NULL when the config directory cannot be resolved.
 */
static char *theme_css_default_path(void) {
    const char *base = g_get_user_config_dir();
    if (!base || base[0] == '\0') return NULL;
    return g_build_filename(base, "graptos", "theme.css", NULL);
}

/**
 * @brief Replace a nullable string slot.
 * @details Theme migration writes through this helper so config and CSS parsing
 *          use the same ownership rules for window strings.
 * @param slot The owned string slot.
 * @param value The new string value.
 */
static void replace_config_string(char **slot, const char *value) {
    if (!slot) return;
    g_free(*slot);
    *slot = g_strdup(value ? value : "");
}

/**
 * @brief Append one managed color variable.
 * @details The managed block is intentionally explicit. Each variable maps to
 *          one Theme dialog control and one legacy config key during migration.
 * @param css The CSS buffer being written.
 * @param name The CSS variable name without the `graptos_` prefix.
 * @param value The configured value.
 * @param fallback The fallback value.
 */
static void append_theme_define(GString *css,
                                const char *name,
                                const char *value,
                                const char *fallback) {
    if (!css || !name) return;
    const char *resolved = value && value[0] == '#' ? value : fallback;
    g_string_append_printf(css,
                           "@define-color graptos_%s %s;\n",
                           name,
                           resolved ? resolved : "#000000");
}

/**
 * @brief Append one managed font metadata line.
 * @details GTK CSS cannot be reliably round-tripped for every font selector, so
 *          the Theme dialog stores structured font values in comments beside
 *          the generated rules.
 * @param css The CSS buffer being written.
 * @param name The metadata key.
 * @param value The font description.
 */
static void append_theme_font_meta(GString *css,
                                   const char *name,
                                   const char *value) {
    if (!css || !name) return;
    g_string_append_printf(css,
                           "/* @graptos-%s-font: %s */\n",
                           name,
                           value ? value : "");
}

/**
 * @brief Build the managed CSS block for the current window theme.
 * @details The block exposes all Theme dialog color values as CSS variables and
 *          uses selector rules broad enough for the application chrome, editor,
 *          popovers, dialogs, search, diagnostics, previews, and tab state.
 * @param win The source window.
 * @return Owned CSS text for the managed block.
 */
static char *build_managed_theme_css(EditorWindow *win) {
    if (!win) return NULL;
    GString *css = g_string_new("/* GRAPTOS THEME BEGIN */\n");
    if (!css) return NULL;

    append_theme_define(css, "editor_bg", win->editor_bg_color, "#181a1f");
    append_theme_define(css, "editor_fg", win->editor_fg_color, "#d4d4d4");
    append_theme_define(css, "editor_gutter_bg", win->editor_gutter_bg_color, "#181a1f");
    append_theme_define(css, "editor_gutter_fg", win->editor_gutter_fg_color, "#8b949e");
    append_theme_define(css, "editor_current_line_bg", win->editor_current_line_bg_color, "#20232b");
    append_theme_define(css, "editor_selection_bg", win->editor_selection_bg_color, "#3a405c");
    append_theme_define(css, "editor_selection_fg", win->editor_selection_fg_color, "#ffffff");
    append_theme_define(css, "editor_cursor", win->editor_cursor_color, "#d4d4d4");
    append_theme_define(css, "sidebar_bg", win->sidebar_bg_color, "#111318");
    append_theme_define(css, "tabbar_bg", win->tabbar_bg_color, "#111318");
    append_theme_define(css, "tabbar_fg", win->tabbar_fg_color, "#d4d4d4");
    append_theme_define(css, "tab_active_bg", win->tab_active_bg_color, "#20232b");
    append_theme_define(css, "tab_active_fg", win->tab_active_fg_color, "#ffffff");
    append_theme_define(css, "topbar_bg", win->topbar_bg_color, "#111318");
    append_theme_define(css, "topbar_fg", win->topbar_fg_color, "#d4d4d4");
    append_theme_define(css, "bottombar_bg", win->bottombar_bg_color, "#111318");
    append_theme_define(css, "bottombar_fg", win->bottombar_fg_color, "#d4d4d4");
    append_theme_define(css, "status_error", win->status_error_color, "#ff6b6b");
    append_theme_define(css, "button_bg", win->button_bg_color, "#181a1f");
    append_theme_define(css, "button_fg", win->button_fg_color, "#d4d4d4");
    append_theme_define(css, "button_hover_bg", win->button_hover_bg_color, "#2a2e3d");
    append_theme_define(css, "button_active_bg", win->button_active_bg_color, "#31364a");
    append_theme_define(css, "input_bg", win->input_bg_color, "#111318");
    append_theme_define(css, "input_fg", win->input_fg_color, "#d4d4d4");
    append_theme_define(css, "input_border", win->input_border_color, "#3a4050");
    append_theme_define(css, "project_tree_fg", win->project_tree_fg_color, "#d4d4d4");
    append_theme_define(css, "project_tree_selected_bg", win->project_tree_selected_bg_color, "#2a2e3d");
    append_theme_define(css, "project_tree_selected_fg", win->project_tree_selected_fg_color, "#ffffff");
    append_theme_define(css, "git_status_modified", win->git_status_modified_color, "#f9c74f");
    append_theme_define(css, "git_status_added", win->git_status_added_color, "#57cc99");
    append_theme_define(css, "git_status_deleted", win->git_status_deleted_color, "#ff6b6b");
    append_theme_define(css, "git_status_renamed", win->git_status_renamed_color, "#4cc9f0");
    append_theme_define(css, "git_status_conflict", win->git_status_conflict_color, "#ff4d6d");
    append_theme_define(css, "git_status_untracked", win->git_status_untracked_color, "#77bdfb");
    append_theme_define(css, "git_status_staged", win->git_status_staged_color, "#cba6f7");
    append_theme_define(css, "scroll_preview_bg", win->scroll_preview_bg_color, "#111318");
    append_theme_define(css, "scroll_preview_fg", win->scroll_preview_fg_color, "#8b949e");
    append_theme_define(css, "popover_bg", win->popover_bg_color, "#1b1f24");
    append_theme_define(css, "popover_border", win->popover_border_color, "#00000000");
    append_theme_define(css, "tooltip_bg", win->tooltip_bg_color, "#1b1f24");
    append_theme_define(css, "tooltip_fg", win->tooltip_fg_color, "#d4d4d4");
    append_theme_define(css, "tooltip_border", win->tooltip_border_color, "#00000000");
    append_theme_define(css, "ref_popover_bg", win->ref_popover_bg_color, "#1b1f24");
    append_theme_define(css, "ref_popover_fg", win->ref_popover_fg_color, "#d4d4d4");
    append_theme_define(css, "ref_popover_heading", win->ref_popover_heading_color, "#cba6f7");
    append_theme_define(css, "ref_popover_title", win->ref_popover_title_color, "#89dceb");
    append_theme_define(css, "ref_popover_kind", win->ref_popover_kind_color, "#a6adc8");
    append_theme_define(css, "ref_popover_snippet", win->ref_popover_snippet_color, "#d4d4d4");
    append_theme_define(css, "ref_popover_hover_bg", win->ref_popover_hover_bg_color, "#2a2e3d");
    append_theme_define(css, "ref_popover_hover_fg", win->ref_popover_hover_fg_color, "#ffffff");
    append_theme_define(css, "completion_popover_bg", win->completion_popover_bg_color, "#1b1f24");
    append_theme_define(css, "completion_popover_fg", win->completion_popover_fg_color, "#d4d4d4");
    append_theme_define(css, "completion_popover_detail", win->completion_popover_detail_color, "#a6adc8");
    append_theme_define(css, "completion_selection_bg", win->completion_selection_bg_color, "#89b4fa");
    append_theme_define(css, "completion_selection_fg", win->completion_selection_fg_color, "#11111b");
    append_theme_define(css, "dialog_bg", win->dialog_bg_color, "#1b1f24");
    append_theme_define(css, "dialog_fg", win->dialog_fg_color, "#d4d4d4");
    append_theme_define(css, "dialog_border", win->dialog_border_color, "#00000000");
    append_theme_define(css, "dialog_title", win->dialog_title_color, "#ffffff");
    append_theme_define(css, "dialog_body", win->dialog_body_color, "#d4d4d4");
    append_theme_define(css, "dialog_muted", win->dialog_muted_color, "#a6adc8");
    append_theme_define(css, "dialog_output", win->dialog_output_color, "#d4d4d4");
    append_theme_define(css, "git_output_bg", win->git_output_bg_color, "#1b1f24");
    append_theme_define(css, "dialog_action", win->dialog_action_color, "#d4d4d4");
    append_theme_define(css, "dialog_destructive_action", win->dialog_destructive_action_color, "#ff6b6b");
    append_theme_define(css, "dialog_input_fg", win->dialog_input_fg_color, "#d4d4d4");
    append_theme_define(css, "dialog_input_bg", win->dialog_input_bg_color, "#111318");
    append_theme_define(css, "search_match_bg", win->search_match_bg_color, "#515c7a");
    append_theme_define(css, "search_match_fg", win->search_match_fg_color, "#ffffff");
    append_theme_define(css, "diagnostic_warning_bg", win->diagnostic_warning_bg_color, "#5f4b24");
    append_theme_define(css, "diagnostic_warning_fg", win->diagnostic_warning_fg_color, "#ffd166");
    append_theme_define(css, "codex_preview_bg", win->codex_preview_bg_color, "#1b1f24");
    append_theme_define(css, "codex_preview_fg", win->codex_preview_fg_color, "#d4d4d4");
    append_theme_define(css, "codex_prompt_bg", win->codex_prompt_bg_color, "#111318");
    g_string_append_c(css, '\n');
    append_theme_font_meta(css, "ui", win->ui_font);
    append_theme_font_meta(css, "editor", win->editor_font);
    append_theme_font_meta(css, "preview", win->preview_font);
    append_theme_font_meta(css, "terminal", win->terminal_font);
    append_theme_font_meta(css, "code", win->code_font);
    g_string_append(css,
        "\n"
        "window.graptos-window, window.graptos-window > contents,\n"
        ".graptos-root, .graptos-root > box, .graptos-tab-page,\n"
        ".graptos-editor-area, .graptos-editor-overlay,\n"
        ".graptos-editor-content {\n"
        "  background: @graptos_editor_bg;\n"
        "  background-color: @graptos_editor_bg;\n"
        "  color: @graptos_editor_fg;\n"
        "}\n\n"
        ".graptos-editor, .graptos-editor text,\n"
        "textview.graptos-editor, textview.graptos-editor text,\n"
        "sourceview.graptos-editor, sourceview.graptos-editor text {\n"
        "  background: @graptos_editor_bg;\n"
        "  background-color: @graptos_editor_bg;\n"
        "  color: @graptos_editor_fg;\n"
        "  caret-color: @graptos_editor_cursor;\n"
        "}\n\n"
        "sourceview.graptos-editor gutter,\n"
        "sourceview.graptos-editor gutter * {\n"
        "  background: @graptos_editor_gutter_bg;\n"
        "  background-color: @graptos_editor_gutter_bg;\n"
        "  color: @graptos_editor_gutter_fg;\n"
        "}\n\n"
        "sourceview.graptos-editor text selection {\n"
        "  background: @graptos_editor_selection_bg;\n"
        "  background-color: @graptos_editor_selection_bg;\n"
        "  color: @graptos_editor_selection_fg;\n"
        "}\n\n"
        ".graptos-current-line { background: @graptos_editor_current_line_bg; }\n"
        ".graptos-top { background: @graptos_topbar_bg; color: @graptos_topbar_fg; }\n"
        ".graptos-bottom, .graptos-search-panel, .graptos-tool-panel {\n"
        "  background: @graptos_bottombar_bg;\n"
        "  color: @graptos_bottombar_fg;\n"
        "}\n"
        ".graptos-bottom label.graptos-status-error,\n"
        "label.graptos-status-error { color: @graptos_status_error; }\n\n"
        ".graptos-root button {\n"
        "  background: @graptos_button_bg;\n"
        "  color: @graptos_button_fg;\n"
        "}\n"
        ".graptos-root button:hover { background: @graptos_button_hover_bg; }\n"
        ".graptos-root button:checked, .graptos-root button:active { background: @graptos_button_active_bg; }\n\n"
        ".graptos-root entry, .graptos-root entry text,\n"
        ".graptos-root spinbutton, .graptos-root spinbutton text,\n"
        ".graptos-root dropdown, .graptos-root combobox {\n"
        "  background: @graptos_input_bg;\n"
        "  color: @graptos_input_fg;\n"
        "  border-color: @graptos_input_border;\n"
        "}\n\n"
        ".graptos-project-pane, .graptos-project-tree,\n"
        ".graptos-project-tree row {\n"
        "  background: @graptos_sidebar_bg;\n"
        "  color: @graptos_project_tree_fg;\n"
        "}\n"
        ".graptos-project-tree row:selected, .graptos-project-tree row:hover {\n"
        "  background: @graptos_project_tree_selected_bg;\n"
        "  color: @graptos_project_tree_selected_fg;\n"
        "}\n"
        ".graptos-project-tree label.graptos-git-status-modified { color: @graptos_git_status_modified; }\n"
        ".graptos-project-tree label.graptos-git-status-added { color: @graptos_git_status_added; }\n"
        ".graptos-project-tree label.graptos-git-status-deleted { color: @graptos_git_status_deleted; }\n"
        ".graptos-project-tree label.graptos-git-status-renamed { color: @graptos_git_status_renamed; }\n"
        ".graptos-project-tree label.graptos-git-status-conflict { color: @graptos_git_status_conflict; }\n"
        ".graptos-project-tree label.graptos-git-status-untracked { color: @graptos_git_status_untracked; }\n"
        ".graptos-project-tree label.graptos-git-status-staged { color: @graptos_git_status_staged; }\n\n"
        ".graptos-root notebook > header,\n"
        ".graptos-root notebook > header.top > tabs > tab {\n"
        "  background: @graptos_tabbar_bg;\n"
        "  color: @graptos_tabbar_fg;\n"
        "}\n"
        ".graptos-root notebook > header.top > tabs > tab:checked {\n"
        "  background: @graptos_tab_active_bg;\n"
        "  color: @graptos_tab_active_fg;\n"
        "  border-bottom-color: @graptos_completion_selection_bg;\n"
        "}\n"
        ".graptos-tab-tiled { box-shadow: inset 0 -2px @graptos_completion_selection_bg; }\n\n");
    g_string_append(css,
        ".graptos-minimap, .graptos-minimap text,\n"
        ".graptos-preview, .graptos-preview text {\n"
        "  background: @graptos_scroll_preview_bg;\n"
        "  color: @graptos_scroll_preview_fg;\n"
        "}\n"
        ".graptos-codex-preview, .graptos-codex-preview text {\n"
        "  background: @graptos_codex_preview_bg;\n"
        "  color: @graptos_codex_preview_fg;\n"
        "}\n"
        ".graptos-codex-prompt, .graptos-codex-prompt text { background: @graptos_codex_prompt_bg; }\n\n"
        "popover.graptos-context-popover, popover.graptos-tools-popover,\n"
        "popover.graptos-completion-popover, popover.graptos-hover-popover,\n"
        "popover.graptos-context-popover contents, popover.graptos-tools-popover contents,\n"
        "popover.graptos-completion-popover contents, popover.graptos-hover-popover contents {\n"
        "  background: @graptos_popover_bg;\n"
        "  border-color: @graptos_popover_border;\n"
        "}\n"
        "tooltip, tooltip.background, tooltip > box {\n"
        "  background: @graptos_tooltip_bg;\n"
        "  color: @graptos_tooltip_fg;\n"
        "  border-color: @graptos_tooltip_border;\n"
        "}\n\n"
        ".graptos-completion-list, .graptos-completion-list row {\n"
        "  background: @graptos_completion_popover_bg;\n"
        "  color: @graptos_completion_popover_fg;\n"
        "}\n"
        ".graptos-completion-detail { color: @graptos_completion_popover_detail; }\n"
        ".graptos-completion-list row:selected,\n"
        ".graptos-completion-list row:hover {\n"
        "  background: @graptos_completion_selection_bg;\n"
        "  color: @graptos_completion_selection_fg;\n"
        "}\n\n"
        ".graptos-ref-list, .graptos-ref-list row {\n"
        "  background: @graptos_ref_popover_bg;\n"
        "  color: @graptos_ref_popover_fg;\n"
        "}\n"
        ".graptos-ref-heading { color: @graptos_ref_popover_heading; }\n"
        ".graptos-ref-title { color: @graptos_ref_popover_title; }\n"
        ".graptos-ref-kind { color: @graptos_ref_popover_kind; }\n"
        ".graptos-ref-snippet { color: @graptos_ref_popover_snippet; }\n"
        ".graptos-ref-list row:hover { background: @graptos_ref_popover_hover_bg; color: @graptos_ref_popover_hover_fg; }\n\n"
        "window.graptos-window .graptos-root.graptos-dialog-root,\n"
        "window.graptos-window .graptos-dialog-output,\n"
        "window.graptos-window .graptos-dialog-output text {\n"
        "  background: @graptos_dialog_bg;\n"
        "  color: @graptos_dialog_fg;\n"
        "  border-color: @graptos_dialog_border;\n"
        "}\n"
        ".graptos-window .graptos-dialog-title,\n"
        ".graptos-window .graptos-menu-title { color: @graptos_dialog_title; }\n"
        ".graptos-window .graptos-dialog-body { color: @graptos_dialog_body; }\n"
        ".graptos-window .graptos-dialog-muted { color: @graptos_dialog_muted; }\n"
        ".graptos-window .graptos-dialog-output { color: @graptos_dialog_output; }\n"
        ".graptos-window .graptos-git-dialog-output { background: @graptos_git_output_bg; }\n"
        ".graptos-window .graptos-dialog-action { color: @graptos_dialog_action; }\n"
        ".graptos-window .graptos-dialog-action-destructive { color: @graptos_dialog_destructive_action; }\n"
        ".graptos-window .graptos-root.graptos-dialog-root entry,\n"
        ".graptos-window .graptos-root.graptos-dialog-root entry text {\n"
        "  background: @graptos_dialog_input_bg;\n"
        "  color: @graptos_dialog_input_fg;\n"
        "}\n\n"
        ".graptos-search-match { background: @graptos_search_match_bg; color: @graptos_search_match_fg; }\n"
        ".graptos-diagnostic-warning { background: @graptos_diagnostic_warning_bg; color: @graptos_diagnostic_warning_fg; }\n"
        "/* GRAPTOS THEME END */\n");
    return g_string_free(css, FALSE);
}

/**
 * @brief Extract one CSS variable from a theme file.
 * @details The parser intentionally reads only Graptoς-managed variables. Free
 *          CSS remains valid but is not projected into structured controls.
 * @param css Theme CSS text.
 * @param name Variable name without the `graptos_` prefix.
 * @param slot Destination string slot.
 * @return TRUE when a valid color was loaded.
 */
static gboolean load_theme_define(const char *css,
                                  const char *name,
                                  char **slot) {
    if (!css || !name || !slot) return FALSE;
    g_autofree char *needle = g_strdup_printf("@define-color graptos_%s", name);
    char *pos = strstr(css, needle);
    if (!pos) return FALSE;
    pos += strlen(needle);
    while (*pos == ' ' || *pos == '\t') pos++;
    char *end = pos;
    while (*end && *end != ';' && *end != '\n' && *end != '\r' &&
           *end != ' ' && *end != '\t') {
        end++;
    }
    if (end <= pos) return FALSE;
    g_autofree char *value = g_strndup(pos, (gsize)(end - pos));
    GdkRGBA rgba;
    if (!value || !gdk_rgba_parse(&rgba, value)) return FALSE;
    replace_config_string(slot, value);
    return TRUE;
}

/**
 * @brief Extract one font metadata value from a CSS theme file.
 * @details Font values are stored as comments so hand-written CSS can still
 *          override font selectors without breaking the structured dialog.
 * @param css Theme CSS text.
 * @param name Metadata key.
 * @param slot Destination string slot.
 */
static void load_theme_font_meta(const char *css,
                                 const char *name,
                                 char **slot) {
    if (!css || !name || !slot) return;
    g_autofree char *needle = g_strdup_printf("/* @graptos-%s-font:", name);
    char *pos = strstr(css, needle);
    if (!pos) return;
    pos += strlen(needle);
    char *end = strstr(pos, "*/");
    if (!end) return;
    g_autofree char *value = g_strndup(pos, (gsize)(end - pos));
    if (!value) return;
    g_strstrip(value);
    replace_config_string(slot, value);
}

/**
 * @brief Load theme values from a Graptoς CSS theme file.
 * @details CSS is authoritative once present. Legacy config values may seed
 *          the window before this call, but values parsed here win.
 * @param win The window receiving theme values.
 * @param path The CSS theme file path.
 * @return TRUE when the CSS file was read.
 */
gboolean graptos_theme_css_load_into_window(EditorWindow *win,
                                            const char *path) {
    if (!win || !path || path[0] == '\0') return FALSE;
    g_autofree char *css = NULL;
    gsize length = 0u;
    g_autoptr(GError) error = NULL;
    if (!g_file_get_contents(path, &css, &length, &error) || !css) return FALSE;

#define LOAD_THEME_VAR(name, field) \
    (void)load_theme_define(css, name, &win->field)
    LOAD_THEME_VAR("editor_bg", editor_bg_color);
    LOAD_THEME_VAR("editor_fg", editor_fg_color);
    LOAD_THEME_VAR("editor_gutter_bg", editor_gutter_bg_color);
    LOAD_THEME_VAR("editor_gutter_fg", editor_gutter_fg_color);
    LOAD_THEME_VAR("editor_current_line_bg", editor_current_line_bg_color);
    LOAD_THEME_VAR("editor_selection_bg", editor_selection_bg_color);
    LOAD_THEME_VAR("editor_selection_fg", editor_selection_fg_color);
    LOAD_THEME_VAR("editor_cursor", editor_cursor_color);
    LOAD_THEME_VAR("sidebar_bg", sidebar_bg_color);
    LOAD_THEME_VAR("tabbar_bg", tabbar_bg_color);
    LOAD_THEME_VAR("tabbar_fg", tabbar_fg_color);
    LOAD_THEME_VAR("tab_active_bg", tab_active_bg_color);
    LOAD_THEME_VAR("tab_active_fg", tab_active_fg_color);
    LOAD_THEME_VAR("topbar_bg", topbar_bg_color);
    LOAD_THEME_VAR("topbar_fg", topbar_fg_color);
    LOAD_THEME_VAR("bottombar_bg", bottombar_bg_color);
    LOAD_THEME_VAR("bottombar_fg", bottombar_fg_color);
    LOAD_THEME_VAR("status_error", status_error_color);
    LOAD_THEME_VAR("button_bg", button_bg_color);
    LOAD_THEME_VAR("button_fg", button_fg_color);
    LOAD_THEME_VAR("button_hover_bg", button_hover_bg_color);
    LOAD_THEME_VAR("button_active_bg", button_active_bg_color);
    LOAD_THEME_VAR("input_bg", input_bg_color);
    LOAD_THEME_VAR("input_fg", input_fg_color);
    LOAD_THEME_VAR("input_border", input_border_color);
    LOAD_THEME_VAR("project_tree_fg", project_tree_fg_color);
    LOAD_THEME_VAR("project_tree_selected_bg", project_tree_selected_bg_color);
    LOAD_THEME_VAR("project_tree_selected_fg", project_tree_selected_fg_color);
    LOAD_THEME_VAR("git_status_modified", git_status_modified_color);
    LOAD_THEME_VAR("git_status_added", git_status_added_color);
    LOAD_THEME_VAR("git_status_deleted", git_status_deleted_color);
    LOAD_THEME_VAR("git_status_renamed", git_status_renamed_color);
    LOAD_THEME_VAR("git_status_conflict", git_status_conflict_color);
    LOAD_THEME_VAR("git_status_untracked", git_status_untracked_color);
    LOAD_THEME_VAR("git_status_staged", git_status_staged_color);
    LOAD_THEME_VAR("scroll_preview_bg", scroll_preview_bg_color);
    LOAD_THEME_VAR("scroll_preview_fg", scroll_preview_fg_color);
    LOAD_THEME_VAR("popover_bg", popover_bg_color);
    LOAD_THEME_VAR("popover_border", popover_border_color);
    LOAD_THEME_VAR("tooltip_bg", tooltip_bg_color);
    LOAD_THEME_VAR("tooltip_fg", tooltip_fg_color);
    LOAD_THEME_VAR("tooltip_border", tooltip_border_color);
    LOAD_THEME_VAR("ref_popover_bg", ref_popover_bg_color);
    LOAD_THEME_VAR("ref_popover_fg", ref_popover_fg_color);
    LOAD_THEME_VAR("ref_popover_heading", ref_popover_heading_color);
    LOAD_THEME_VAR("ref_popover_title", ref_popover_title_color);
    LOAD_THEME_VAR("ref_popover_kind", ref_popover_kind_color);
    LOAD_THEME_VAR("ref_popover_snippet", ref_popover_snippet_color);
    LOAD_THEME_VAR("ref_popover_hover_bg", ref_popover_hover_bg_color);
    LOAD_THEME_VAR("ref_popover_hover_fg", ref_popover_hover_fg_color);
    LOAD_THEME_VAR("completion_popover_bg", completion_popover_bg_color);
    LOAD_THEME_VAR("completion_popover_fg", completion_popover_fg_color);
    LOAD_THEME_VAR("completion_popover_detail", completion_popover_detail_color);
    LOAD_THEME_VAR("completion_selection_bg", completion_selection_bg_color);
    LOAD_THEME_VAR("completion_selection_fg", completion_selection_fg_color);
    LOAD_THEME_VAR("dialog_bg", dialog_bg_color);
    LOAD_THEME_VAR("dialog_fg", dialog_fg_color);
    LOAD_THEME_VAR("dialog_border", dialog_border_color);
    LOAD_THEME_VAR("dialog_title", dialog_title_color);
    LOAD_THEME_VAR("dialog_body", dialog_body_color);
    LOAD_THEME_VAR("dialog_muted", dialog_muted_color);
    LOAD_THEME_VAR("dialog_output", dialog_output_color);
    LOAD_THEME_VAR("git_output_bg", git_output_bg_color);
    LOAD_THEME_VAR("dialog_action", dialog_action_color);
    LOAD_THEME_VAR("dialog_destructive_action", dialog_destructive_action_color);
    LOAD_THEME_VAR("dialog_input_fg", dialog_input_fg_color);
    LOAD_THEME_VAR("dialog_input_bg", dialog_input_bg_color);
    LOAD_THEME_VAR("search_match_bg", search_match_bg_color);
    LOAD_THEME_VAR("search_match_fg", search_match_fg_color);
    LOAD_THEME_VAR("diagnostic_warning_bg", diagnostic_warning_bg_color);
    LOAD_THEME_VAR("diagnostic_warning_fg", diagnostic_warning_fg_color);
    LOAD_THEME_VAR("codex_preview_bg", codex_preview_bg_color);
    LOAD_THEME_VAR("codex_preview_fg", codex_preview_fg_color);
    LOAD_THEME_VAR("codex_prompt_bg", codex_prompt_bg_color);
#undef LOAD_THEME_VAR
    load_theme_font_meta(css, "ui", &win->ui_font);
    load_theme_font_meta(css, "editor", &win->editor_font);
    load_theme_font_meta(css, "preview", &win->preview_font);
    load_theme_font_meta(css, "terminal", &win->terminal_font);
    load_theme_font_meta(css, "code", &win->code_font);
    return TRUE;
}

/**
 * @brief Save the window theme into a Graptoς CSS theme file.
 * @details User CSS outside the managed block survives Theme dialog saves.
 *          When no block exists, the managed block is appended after any
 *          existing hand-written CSS.
 * @param win The window whose theme values are written.
 * @param path The CSS theme file path.
 * @param preserve_custom TRUE to keep CSS outside the managed block.
 * @return TRUE when the CSS reached disk.
 */
gboolean graptos_theme_css_save_from_window(EditorWindow *win,
                                            const char *path,
                                            gboolean preserve_custom) {
    if (!win || !path || path[0] == '\0') return FALSE;
    g_autofree char *managed = build_managed_theme_css(win);
    if (!managed) return FALSE;

    g_autofree char *existing = NULL;
    gsize existing_len = 0u;
    (void)g_file_get_contents(path, &existing, &existing_len, NULL);

    GString *out = g_string_new(NULL);
    if (!out) return FALSE;
    const char *begin = existing ? strstr(existing, "/* GRAPTOS THEME BEGIN */") : NULL;
    const char *end = existing ? strstr(existing, "/* GRAPTOS THEME END */") : NULL;
    if (preserve_custom && existing && begin && end && end >= begin) {
        end += strlen("/* GRAPTOS THEME END */");
        while (*end == '\r' || *end == '\n') end++;
        g_string_append_len(out, existing, (gssize)(begin - existing));
        g_string_append(out, managed);
        if (*end != '\0') {
            if (out->len > 0u && out->str[out->len - 1u] != '\n') g_string_append_c(out, '\n');
            g_string_append(out, end);
        }
    } else if (preserve_custom && existing && existing[0] != '\0' && !begin) {
        g_string_append(out, existing);
        if (out->str[out->len - 1u] != '\n') g_string_append_c(out, '\n');
        g_string_append_c(out, '\n');
        g_string_append(out, managed);
    } else {
        g_string_append(out, managed);
    }

    g_autofree char *dir = g_path_get_dirname(path);
    gboolean ok = dir && g_mkdir_with_parents(dir, 0700) == 0 &&
        g_file_set_contents(path, out->str, (gssize)out->len, NULL);
    g_string_free(out, TRUE);
    return ok;
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

    save_string(key_file, "theme_css_path", win->theme_css_path ? win->theme_css_path : "");
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
        if (win->theme_css_path && win->theme_css_path[0] != '\0') {
            (void)graptos_theme_css_save_from_window(win, win->theme_css_path, FALSE);
        }
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
    /**
     * @brief Resolve the active CSS theme path from new and legacy keys.
     * @details CSS is now the primary theme source. The old custom_css_path key
     *          is accepted only as a migration alias so existing config files do
     *          not lose their selected theme file.
     */
    if (g_key_file_has_key(key_file, "Editor", "theme_css_path", NULL)) {
        load_string(key_file, "theme_css_path", &win->theme_css_path);
    } else if (g_key_file_has_key(key_file, "Editor", "custom_css_path", NULL)) {
        load_string(key_file, "custom_css_path", &win->theme_css_path);
    } else {
        g_clear_pointer(&win->theme_css_path, g_free);
        win->theme_css_path = theme_css_default_path();
    }
    if (!win->theme_css_path || win->theme_css_path[0] == '\0') {
        g_clear_pointer(&win->theme_css_path, g_free);
        win->theme_css_path = theme_css_default_path();
    }
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
        win->codex_preview_bg_color = g_strdup("#1b1f24");
    }
    if (!win->codex_preview_fg_color) {
        win->codex_preview_fg_color = g_strdup("#d4d4d4");
    }
    if (!win->codex_prompt_bg_color) {
        win->codex_prompt_bg_color = g_strdup("#111318");
    }
    if (win->theme_css_path && win->theme_css_path[0] != '\0' &&
        g_file_test(win->theme_css_path, G_FILE_TEST_IS_REGULAR)) {
        (void)graptos_theme_css_load_into_window(win, win->theme_css_path);
    } else if (win->theme_css_path && win->theme_css_path[0] != '\0') {
        (void)graptos_theme_css_save_from_window(win, win->theme_css_path, FALSE);
    }
    graptos_config_save(win);
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
