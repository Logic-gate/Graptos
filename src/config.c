#include "config.h"

#include <errno.h>
#include <string.h>

static gboolean parse_bool(GKeyFile *key_file, const char *key, gboolean fallback) {
    GError *error = NULL;
    gboolean value = g_key_file_get_boolean(key_file, "Editor", key, &error);
    if (error) {
        g_clear_error(&error);
        return fallback;
    }
    return value;
}

static guint parse_uint(GKeyFile *key_file, const char *key, guint fallback, guint min_value, guint max_value) {
    GError *error = NULL;
    gint value = g_key_file_get_integer(key_file, "Editor", key, &error);
    if (error) {
        g_clear_error(&error);
        return fallback;
    }
    if (value < (gint)min_value) return min_value;
    if (value > (gint)max_value) return max_value;
    return (guint)value;
}

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

static void save_color(GKeyFile *key_file, const char *key, const char *value) {
    if (!key_file || !key || !value || value[0] == '\0') return;
    g_key_file_set_string(key_file, "Editor", key, value);
}

char *cleaf_config_path(void) {
    const char *base = g_get_user_config_dir();
    if (!base || base[0] == '\0') return NULL;
    return g_build_filename(base, "cleaf", "config.ini", NULL);
}

void cleaf_config_load(EditorWindow *win) {
    if (!win) return;
    char *path = cleaf_config_path();
    if (!path) return;
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_free(path);
        return;
    }

    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, &error)) {
        g_clear_error(&error);
        g_key_file_unref(key_file);
        g_free(path);
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
    load_color(key_file, "scroll_preview_background_color", &win->scroll_preview_bg_color);
    load_color(key_file, "scroll_preview_foreground_color", &win->scroll_preview_fg_color);
    load_color(key_file, "popover_background_color", &win->popover_bg_color);
    load_color(key_file, "ref_popover_background_color", &win->ref_popover_bg_color);
    load_color(key_file, "ref_popover_foreground_color", &win->ref_popover_fg_color);
    load_color(key_file, "ref_popover_heading_color", &win->ref_popover_heading_color);
    load_color(key_file, "ref_popover_title_color", &win->ref_popover_title_color);
    load_color(key_file, "ref_popover_kind_color", &win->ref_popover_kind_color);
    load_color(key_file, "ref_popover_snippet_color", &win->ref_popover_snippet_color);
    load_color(key_file, "autocomplete_popover_background_color", &win->completion_popover_bg_color);
    load_color(key_file, "autocomplete_popover_foreground_color", &win->completion_popover_fg_color);
    load_color(key_file, "autocomplete_popover_detail_color", &win->completion_popover_detail_color);
    load_color(key_file, "autocomplete_selection_background_color", &win->completion_selection_bg_color);
    load_color(key_file, "autocomplete_selection_foreground_color", &win->completion_selection_fg_color);
    load_color(key_file, "dialog_background_color", &win->dialog_bg_color);
    load_color(key_file, "dialog_foreground_color", &win->dialog_fg_color);
    load_color(key_file, "dialog_border_color", &win->dialog_border_color);
    load_color(key_file, "dialog_title_color", &win->dialog_title_color);
    load_color(key_file, "search_match_background_color", &win->search_match_bg_color);
    load_color(key_file, "search_match_foreground_color", &win->search_match_fg_color);
    load_color(key_file, "diagnostic_warning_background_color", &win->diagnostic_warning_bg_color);
    load_color(key_file, "diagnostic_warning_foreground_color", &win->diagnostic_warning_fg_color);
    load_color(key_file, "codex_preview_background_color", &win->codex_preview_bg_color);
    load_color(key_file, "codex_preview_foreground_color", &win->codex_preview_fg_color);
    load_color(key_file, "codex_prompt_background_color", &win->codex_prompt_bg_color);

    win->use_system_interface_font = parse_bool(key_file, "use_system_interface_font", win->use_system_interface_font);
    win->autocomplete_enabled = parse_bool(key_file, "autocomplete_enabled", win->autocomplete_enabled);
    win->auto_save_enabled = parse_bool(key_file, "auto_save_enabled", win->auto_save_enabled);
    win->backup_enabled = parse_bool(key_file, "backup_enabled", win->backup_enabled);
    win->insert_spaces = parse_bool(key_file, "insert_spaces", win->insert_spaces);
    win->tab_width = parse_uint(key_file, "tab_width", win->tab_width, 1u, 16u);
    win->minimap_enabled = parse_bool(key_file, "scroll_preview_enabled", win->minimap_enabled);
    win->preview_enabled = parse_bool(key_file, "preview_enabled", win->preview_enabled);
    win->use_gtksourceview_highlighting = TRUE;
    win->use_yaml_style_overrides = parse_bool(key_file, "use_yaml_style_overrides", win->use_yaml_style_overrides);

    g_key_file_unref(key_file);
    if (!win->codex_preview_bg_color) {
        win->codex_preview_bg_color = g_strdup(win->editor_bg_color);
    }
    if (!win->codex_preview_fg_color) {
        win->codex_preview_fg_color = g_strdup("#d4d4d4");
    }
    if (!win->codex_prompt_bg_color) {
        win->codex_prompt_bg_color = g_strdup(win->editor_bg_color);
    }
    g_free(path);
}

void cleaf_config_save(EditorWindow *win) {
    if (!win) return;
    char *path = cleaf_config_path();
    if (!path) return;
    char *dir = g_path_get_dirname(path);
    if (!dir) {
        g_free(path);
        return;
    }
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        g_free(dir);
        g_free(path);
        return;
    }

    GKeyFile *key_file = g_key_file_new();
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
    save_color(key_file, "scroll_preview_background_color", win->scroll_preview_bg_color);
    save_color(key_file, "scroll_preview_foreground_color", win->scroll_preview_fg_color);
    save_color(key_file, "popover_background_color", win->popover_bg_color);
    save_color(key_file, "ref_popover_background_color", win->ref_popover_bg_color);
    save_color(key_file, "ref_popover_foreground_color", win->ref_popover_fg_color);
    save_color(key_file, "ref_popover_heading_color", win->ref_popover_heading_color);
    save_color(key_file, "ref_popover_title_color", win->ref_popover_title_color);
    save_color(key_file, "ref_popover_kind_color", win->ref_popover_kind_color);
    save_color(key_file, "ref_popover_snippet_color", win->ref_popover_snippet_color);
    save_color(key_file, "autocomplete_popover_background_color", win->completion_popover_bg_color);
    save_color(key_file, "autocomplete_popover_foreground_color", win->completion_popover_fg_color);
    save_color(key_file, "autocomplete_popover_detail_color", win->completion_popover_detail_color);
    save_color(key_file, "autocomplete_selection_background_color", win->completion_selection_bg_color);
    save_color(key_file, "autocomplete_selection_foreground_color", win->completion_selection_fg_color);
    save_color(key_file, "dialog_background_color", win->dialog_bg_color);
    save_color(key_file, "dialog_foreground_color", win->dialog_fg_color);
    save_color(key_file, "dialog_border_color", win->dialog_border_color);
    save_color(key_file, "dialog_title_color", win->dialog_title_color);
    save_color(key_file, "search_match_background_color", win->search_match_bg_color);
    save_color(key_file, "search_match_foreground_color", win->search_match_fg_color);
    save_color(key_file, "diagnostic_warning_background_color", win->diagnostic_warning_bg_color);
    save_color(key_file, "diagnostic_warning_foreground_color", win->diagnostic_warning_fg_color);
    save_color(key_file, "codex_preview_background_color", win->codex_preview_bg_color);
    save_color(key_file, "codex_preview_foreground_color", win->codex_preview_fg_color);
    save_color(key_file, "codex_prompt_background_color", win->codex_prompt_bg_color);
    g_key_file_set_boolean(key_file, "Editor", "use_system_interface_font", win->use_system_interface_font);
    g_key_file_set_boolean(key_file, "Editor", "autocomplete_enabled", win->autocomplete_enabled);
    g_key_file_set_boolean(key_file, "Editor", "auto_save_enabled", win->auto_save_enabled);
    g_key_file_set_boolean(key_file, "Editor", "backup_enabled", win->backup_enabled);
    g_key_file_set_boolean(key_file, "Editor", "insert_spaces", win->insert_spaces);
    g_key_file_set_integer(key_file, "Editor", "tab_width", (gint)win->tab_width);
    g_key_file_set_boolean(key_file, "Editor", "scroll_preview_enabled", win->minimap_enabled);
    g_key_file_set_boolean(key_file, "Editor", "preview_enabled", win->preview_enabled);
    g_key_file_set_boolean(key_file, "Editor", "use_gtksourceview_highlighting", TRUE);
    g_key_file_set_boolean(key_file, "Editor", "use_yaml_style_overrides", win->use_yaml_style_overrides);

    gsize length = 0u;
    char *data = g_key_file_to_data(key_file, &length, NULL);
    if (data) {
        (void)g_file_set_contents(path, data, (gssize)length, NULL);
        g_free(data);
    }
    g_key_file_unref(key_file);
    g_free(dir);
    g_free(path);
}
