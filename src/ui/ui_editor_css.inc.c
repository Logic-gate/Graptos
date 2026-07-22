/**
 * @file src/ui/ui_editor_css.inc.c
 * @brief Graptoς ui editor css module.
 * @details Theme values should be traceable. We translate config into GTK CSS here so
 *          colors and fonts do not get scattered through feature code where they are hard
 *          to reason about later.
 */

/**
 * @brief Append a CSS background rule when a configured color is valid.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param css The css supplied by the caller.
 * @param selector The selector supplied by the caller.
 * @param color The color supplied by the caller.
 */
static void append_bg_rule(GString *css, const char *selector,
                           const char *color) {
    if (!css || !selector || !color || color[0] != '#') return;
    g_string_append_printf(css,
                           "%s { background: %s; background-color: %s; }\n",
                           selector, color, color);
}

/**
 * @brief Append fg rule.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param css The css supplied by the caller.
 * @param selector The selector supplied by the caller.
 * @param color The color supplied by the caller.
 */
static void append_fg_rule(GString *css, const char *selector,
                           const char *color) {
    if (!css || !selector || !color || color[0] != '#') return;
    g_string_append_printf(css, "%s { color: %s; }\n", selector, color);
}

/**
 * @brief Append a CSS font rule from a Pango font description string.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param css The css supplied by the caller.
 * @param selector The selector supplied by the caller.
 * @param font_desc The font desc supplied by the caller.
 */
static void append_font_rule(GString *css,
                             const char *selector,
                             const char *font_desc) {
    if (!css || !selector || !font_desc || font_desc[0] == '\0') return;

    PangoFontDescription *desc =
        pango_font_description_from_string(font_desc);
    if (!desc) return;

    const char *family = pango_font_description_get_family(desc);
    if (!family || family[0] == '\0') {
        pango_font_description_free(desc);
        return;
    }

    char *escaped = g_strescape(family, NULL);
    g_string_append_printf(css, "%s { font-family: \"%s\";\n",
                           selector, escaped ? escaped : family);
    g_free(escaped);

    if (pango_font_description_get_size_is_absolute(desc) ||
        pango_font_description_get_size(desc) > 0) {
        double pt = (double)pango_font_description_get_size(desc) /
            (double)PANGO_SCALE;
        if (pt > 0.0) {
            g_string_append_printf(css, "  font-size: %.2fpt;\n", pt);
        }
    }

    g_string_append(css, "}\n");
    pango_font_description_free(desc);
}

/**
 * @brief Apply dynamic editor, chrome, popover, and font CSS.
 *
 * Graptoς applies color config with CSS because many GTK widgets involved here
 * are constructed before file-specific SourceView schemes are known. The CSS
 * covers container chrome and popovers; GtkSourceView token colors are handled
 * separately through generated style schemes.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param editor_bg_color The editor bg color supplied by the caller.
 * @param editor_fg_color The editor fg color supplied by the caller.
 * @param editor_gutter_bg_color The editor gutter bg color supplied by the caller.
 * @param editor_gutter_fg_color The editor gutter fg color supplied by the caller.
 * @param editor_current_line_bg_color The editor current line bg color supplied by the caller.
 * @param editor_selection_bg_color The editor selection bg color supplied by the caller.
 * @param editor_selection_fg_color The editor selection fg color supplied by the caller.
 * @param editor_cursor_color The editor cursor color supplied by the caller.
 * @param sidebar_bg_color The sidebar bg color supplied by the caller.
 * @param tabbar_bg_color The tabbar bg color supplied by the caller.
 * @param tabbar_fg_color The tabbar fg color supplied by the caller.
 * @param tab_active_bg_color The tab active bg color supplied by the caller.
 * @param tab_active_fg_color The tab active fg color supplied by the caller.
 * @param topbar_bg_color The topbar bg color supplied by the caller.
 * @param topbar_fg_color The topbar fg color supplied by the caller.
 * @param bottombar_bg_color The bottombar bg color supplied by the caller.
 * @param bottombar_fg_color The bottombar fg color supplied by the caller.
 * @param status_error_color The status error color supplied by the caller.
 * @param button_bg_color The button bg color supplied by the caller.
 * @param button_fg_color The button fg color supplied by the caller.
 * @param button_hover_bg_color The button hover bg color supplied by the caller.
 * @param button_active_bg_color The button active bg color supplied by the caller.
 * @param input_bg_color The input bg color supplied by the caller.
 * @param input_fg_color The input fg color supplied by the caller.
 * @param input_border_color The input border color supplied by the caller.
 * @param project_tree_fg_color The project tree fg color supplied by the caller.
 * @param project_tree_selected_bg_color The project tree selected bg color supplied by the caller.
 * @param project_tree_selected_fg_color The project tree selected fg color supplied by the caller.
 * @param git_status_modified_color The git status modified color supplied by the caller.
 * @param git_status_added_color The git status added color supplied by the caller.
 * @param git_status_deleted_color The git status deleted color supplied by the caller.
 * @param git_status_renamed_color The git status renamed color supplied by the caller.
 * @param git_status_conflict_color The git status conflict color supplied by the caller.
 * @param git_status_untracked_color The git status untracked color supplied by the caller.
 * @param git_status_staged_color The git status staged color supplied by the caller.
 * @param scroll_preview_bg_color The scroll preview bg color supplied by the caller.
 * @param scroll_preview_fg_color The scroll preview fg color supplied by the caller.
 * @param popover_bg_color The popover bg color supplied by the caller.
 * @param popover_border_color The popover border color supplied by the caller.
 * @param tooltip_bg_color The tooltip background color supplied by the caller.
 * @param tooltip_fg_color The tooltip foreground color supplied by the caller.
 * @param tooltip_border_color The tooltip border color supplied by the caller.
 * @param ref_popover_bg_color The ref popover bg color supplied by the caller.
 * @param ref_popover_fg_color The ref popover fg color supplied by the caller.
 * @param ref_popover_heading_color The ref popover heading color supplied by the caller.
 * @param ref_popover_title_color The ref popover title color supplied by the caller.
 * @param ref_popover_kind_color The ref popover kind color supplied by the caller.
 * @param ref_popover_snippet_color The ref popover snippet color supplied by the caller.
 * @param ref_popover_hover_bg_color The ref popover hover bg color supplied by the caller.
 * @param ref_popover_hover_fg_color The ref popover hover fg color supplied by the caller.
 * @param completion_popover_bg_color The completion popover bg color supplied by the caller.
 * @param completion_popover_fg_color The completion popover fg color supplied by the caller.
 * @param completion_popover_detail_color The completion popover detail color supplied by the caller.
 * @param completion_selection_bg_color The completion selection bg color supplied by the caller.
 * @param completion_selection_fg_color The completion selection fg color supplied by the caller.
 * @param dialog_bg_color The dialog bg color supplied by the caller.
 * @param dialog_fg_color The dialog fg color supplied by the caller.
 * @param dialog_border_color The dialog border color supplied by the caller.
 * @param dialog_title_color The dialog title color supplied by the caller.
 * @param dialog_body_color The dialog body color supplied by the caller.
 * @param dialog_muted_color The dialog muted color supplied by the caller.
 * @param dialog_output_color The dialog output color supplied by the caller.
 * @param git_output_bg_color The git output background color supplied by the caller.
 * @param dialog_action_color The dialog action color supplied by the caller.
 * @param dialog_destructive_action_color The dialog destructive action color supplied by the caller.
 * @param dialog_input_fg_color The dialog input foreground color supplied by the caller.
 * @param dialog_input_bg_color The dialog input background color supplied by the caller.
 * @param search_match_bg_color The search match bg color supplied by the caller.
 * @param search_match_fg_color The search match fg color supplied by the caller.
 * @param diagnostic_warning_bg_color The diagnostic warning bg color supplied by the caller.
 * @param diagnostic_warning_fg_color The diagnostic warning fg color supplied by the caller.
 * @param codex_preview_bg_color The codex preview bg color supplied by the caller.
 * @param codex_preview_fg_color The codex preview fg color supplied by the caller.
 * @param codex_prompt_bg_color The codex prompt bg color supplied by the caller.
 * @param ui_font The ui font supplied by the caller.
 * @param editor_font The editor font supplied by the caller.
 * @param preview_font The preview font supplied by the caller.
 * @param terminal_font The terminal font supplied by the caller.
 * @param code_font The code font supplied by the caller.
 * @param use_system_interface_font The use system interface font supplied by the caller.
 */
void graptos_apply_editor_css(const char *editor_bg_color,
                            const char *editor_fg_color,
                            const char *editor_gutter_bg_color,
                            const char *editor_gutter_fg_color,
                            const char *editor_current_line_bg_color,
                            const char *editor_selection_bg_color,
                            const char *editor_selection_fg_color,
                            const char *editor_cursor_color,
                            const char *sidebar_bg_color,
                            const char *tabbar_bg_color,
                            const char *tabbar_fg_color,
                            const char *tab_active_bg_color,
                            const char *tab_active_fg_color,
                            const char *topbar_bg_color,
                            const char *topbar_fg_color,
                            const char *bottombar_bg_color,
                            const char *bottombar_fg_color,
                            const char *status_error_color,
                            const char *button_bg_color,
                            const char *button_fg_color,
                            const char *button_hover_bg_color,
                            const char *button_active_bg_color,
                            const char *input_bg_color,
                            const char *input_fg_color,
                            const char *input_border_color,
                            const char *project_tree_fg_color,
                            const char *project_tree_selected_bg_color,
                            const char *project_tree_selected_fg_color,
                            const char *git_status_modified_color,
                            const char *git_status_added_color,
                            const char *git_status_deleted_color,
                            const char *git_status_renamed_color,
                            const char *git_status_conflict_color,
                            const char *git_status_untracked_color,
                            const char *git_status_staged_color,
                            const char *scroll_preview_bg_color,
                            const char *scroll_preview_fg_color,
                            const char *popover_bg_color,
                            const char *popover_border_color,
                            const char *tooltip_bg_color,
                            const char *tooltip_fg_color,
                            const char *tooltip_border_color,
                            const char *ref_popover_bg_color,
                            const char *ref_popover_fg_color,
                            const char *ref_popover_heading_color,
                            const char *ref_popover_title_color,
                            const char *ref_popover_kind_color,
                            const char *ref_popover_snippet_color,
                            const char *ref_popover_hover_bg_color,
                            const char *ref_popover_hover_fg_color,
                            const char *completion_popover_bg_color,
                            const char *completion_popover_fg_color,
                            const char *completion_popover_detail_color,
                            const char *completion_selection_bg_color,
                            const char *completion_selection_fg_color,
                            const char *dialog_bg_color,
                            const char *dialog_fg_color,
                            const char *dialog_border_color,
                            const char *dialog_title_color,
                            const char *dialog_body_color,
                            const char *dialog_muted_color,
                            const char *dialog_output_color,
                            const char *git_output_bg_color,
                            const char *dialog_action_color,
                            const char *dialog_destructive_action_color,
                            const char *dialog_input_fg_color,
                            const char *dialog_input_bg_color,
                            const char *search_match_bg_color,
                            const char *search_match_fg_color,
                            const char *diagnostic_warning_bg_color,
                            const char *diagnostic_warning_fg_color,
                            const char *codex_preview_bg_color,
                            const char *codex_preview_fg_color,
                            const char *codex_prompt_bg_color,
                            const char *ui_font,
                            const char *editor_font,
                            const char *preview_font,
                            const char *terminal_font,
                            const char *code_font,
                            gboolean use_system_interface_font) {
    GdkDisplay *display = gdk_display_get_default();
    if (!display) return;
    if (dynamic_provider) {
        gtk_style_context_remove_provider_for_display(
            display, GTK_STYLE_PROVIDER(dynamic_provider));
        g_clear_object(&dynamic_provider);
    }

    GString *css = g_string_new(NULL);
    if (!css) return;

    const char *effective_bg =
        (editor_bg_color && editor_bg_color[0] == '#') ? editor_bg_color : "#181a1f";
    const char *effective_fg =
        (editor_fg_color && editor_fg_color[0] == '#') ? editor_fg_color : "#d4d4d4";

    const char *effective_gutter_bg =
        (editor_gutter_bg_color && editor_gutter_bg_color[0] == '#') ? editor_gutter_bg_color : effective_bg;
    const char *effective_gutter_fg =
        (editor_gutter_fg_color && editor_gutter_fg_color[0] == '#') ? editor_gutter_fg_color : "#8b949e";
    const char *effective_current_line =
        (editor_current_line_bg_color && editor_current_line_bg_color[0] == '#') ? editor_current_line_bg_color : "#20232b";
    const char *effective_selection_bg =
        (editor_selection_bg_color && editor_selection_bg_color[0] == '#') ? editor_selection_bg_color : "#3a405c";
    const char *effective_selection_fg =
        (editor_selection_fg_color && editor_selection_fg_color[0] == '#') ? editor_selection_fg_color : "#ffffff";
    const char *effective_cursor =
        (editor_cursor_color && editor_cursor_color[0] == '#') ? editor_cursor_color : effective_fg;
    const char *effective_sidebar =
        (sidebar_bg_color && sidebar_bg_color[0] == '#') ? sidebar_bg_color : effective_bg;
    const char *effective_tabbar =
        (tabbar_bg_color && tabbar_bg_color[0] == '#') ? tabbar_bg_color : effective_bg;
    const char *effective_tabbar_fg =
        (tabbar_fg_color && tabbar_fg_color[0] == '#') ? tabbar_fg_color : effective_fg;
    const char *effective_tab_active_bg =
        (tab_active_bg_color && tab_active_bg_color[0] == '#') ? tab_active_bg_color : "#20232b";
    const char *effective_tab_active_fg =
        (tab_active_fg_color && tab_active_fg_color[0] == '#') ? tab_active_fg_color : "#ffffff";
    const char *effective_topbar_bg =
        (topbar_bg_color && topbar_bg_color[0] == '#') ? topbar_bg_color : effective_bg;
    const char *effective_topbar_fg =
        (topbar_fg_color && topbar_fg_color[0] == '#') ? topbar_fg_color : effective_fg;
    const char *effective_bottombar_bg =
        (bottombar_bg_color && bottombar_bg_color[0] == '#') ? bottombar_bg_color : effective_bg;
    const char *effective_bottombar_fg =
        (bottombar_fg_color && bottombar_fg_color[0] == '#') ? bottombar_fg_color : effective_fg;
    const char *effective_status_error =
        (status_error_color && status_error_color[0] == '#') ? status_error_color : "#ff6b6b";
    const char *effective_button_bg =
        (button_bg_color && button_bg_color[0] == '#') ? button_bg_color : effective_bg;
    const char *effective_button_fg =
        (button_fg_color && button_fg_color[0] == '#') ? button_fg_color : effective_fg;
    const char *effective_button_hover_bg =
        (button_hover_bg_color && button_hover_bg_color[0] == '#') ? button_hover_bg_color : "#2a2e3d";
    const char *effective_button_active_bg =
        (button_active_bg_color && button_active_bg_color[0] == '#') ? button_active_bg_color : "#31364a";
    const char *effective_input_bg =
        (input_bg_color && input_bg_color[0] == '#') ? input_bg_color : "#111318";
    const char *effective_input_fg =
        (input_fg_color && input_fg_color[0] == '#') ? input_fg_color : effective_fg;
    const char *effective_input_border =
        (input_border_color && input_border_color[0] == '#') ? input_border_color : "#3a4050";
    const char *effective_project_fg =
        (project_tree_fg_color && project_tree_fg_color[0] == '#') ? project_tree_fg_color : effective_fg;
    const char *effective_project_selected_bg =
        (project_tree_selected_bg_color && project_tree_selected_bg_color[0] == '#') ? project_tree_selected_bg_color : "#2a2e3d";
    const char *effective_project_selected_fg =
        (project_tree_selected_fg_color && project_tree_selected_fg_color[0] == '#') ? project_tree_selected_fg_color : "#ffffff";
    const char *effective_git_modified =
        (git_status_modified_color && git_status_modified_color[0] == '#') ? git_status_modified_color : "#f9c74f";
    const char *effective_git_added =
        (git_status_added_color && git_status_added_color[0] == '#') ? git_status_added_color : "#57cc99";
    const char *effective_git_deleted =
        (git_status_deleted_color && git_status_deleted_color[0] == '#') ? git_status_deleted_color : "#ff6b6b";
    const char *effective_git_renamed =
        (git_status_renamed_color && git_status_renamed_color[0] == '#') ? git_status_renamed_color : "#4cc9f0";
    const char *effective_git_conflict =
        (git_status_conflict_color && git_status_conflict_color[0] == '#') ? git_status_conflict_color : "#ff4d6d";
    const char *effective_git_untracked =
        (git_status_untracked_color && git_status_untracked_color[0] == '#') ? git_status_untracked_color : "#77bdfb";
    const char *effective_git_staged =
        (git_status_staged_color && git_status_staged_color[0] == '#') ? git_status_staged_color : "#cba6f7";
    const char *effective_preview =
        (scroll_preview_bg_color && scroll_preview_bg_color[0] == '#') ?
            scroll_preview_bg_color : effective_bg;
    const char *effective_preview_fg =
        (scroll_preview_fg_color && scroll_preview_fg_color[0] == '#') ?
            scroll_preview_fg_color : "#8b949e";
    const char *effective_popover =
        (popover_bg_color && popover_bg_color[0] == '#') ? popover_bg_color : effective_bg;
    const char *effective_popover_border =
        (popover_border_color && popover_border_color[0] == '#') ?
            popover_border_color : "#00000000";
    const char *effective_tooltip_bg =
        (tooltip_bg_color && tooltip_bg_color[0] == '#') ?
            tooltip_bg_color : effective_popover;
    const char *effective_tooltip_fg =
        (tooltip_fg_color && tooltip_fg_color[0] == '#') ?
            tooltip_fg_color : effective_fg;
    const char *effective_tooltip_border =
        (tooltip_border_color && tooltip_border_color[0] == '#') ?
            tooltip_border_color : effective_popover_border;
    const char *effective_ref_bg =
        (ref_popover_bg_color && ref_popover_bg_color[0] == '#') ?
            ref_popover_bg_color : effective_popover;
    const char *effective_ref_fg =
        (ref_popover_fg_color && ref_popover_fg_color[0] == '#') ?
            ref_popover_fg_color : effective_fg;
    const char *effective_ref_heading =
        (ref_popover_heading_color && ref_popover_heading_color[0] == '#') ?
            ref_popover_heading_color : "#cba6f7";
    const char *effective_ref_title =
        (ref_popover_title_color && ref_popover_title_color[0] == '#') ?
            ref_popover_title_color : "#89dceb";
    const char *effective_ref_kind =
        (ref_popover_kind_color && ref_popover_kind_color[0] == '#') ?
            ref_popover_kind_color : "#a6adc8";
    const char *effective_ref_snippet =
        (ref_popover_snippet_color && ref_popover_snippet_color[0] == '#') ?
            ref_popover_snippet_color : effective_ref_fg;
    const char *effective_ref_hover_bg =
        (ref_popover_hover_bg_color && ref_popover_hover_bg_color[0] == '#') ?
            ref_popover_hover_bg_color : "#2a2e3d";
    const char *effective_ref_hover_fg =
        (ref_popover_hover_fg_color && ref_popover_hover_fg_color[0] == '#') ?
            ref_popover_hover_fg_color : "#ffffff";
    const char *effective_completion_bg =
        (completion_popover_bg_color && completion_popover_bg_color[0] == '#') ?
            completion_popover_bg_color : effective_popover;
    const char *effective_completion_fg =
        (completion_popover_fg_color && completion_popover_fg_color[0] == '#') ?
            completion_popover_fg_color : effective_fg;
    const char *effective_completion_detail =
        (completion_popover_detail_color && completion_popover_detail_color[0] == '#') ?
            completion_popover_detail_color : "#a6adc8";
    const char *effective_completion_selection_bg =
        (completion_selection_bg_color && completion_selection_bg_color[0] == '#') ?
            completion_selection_bg_color : "#89b4fa";
    const char *effective_completion_selection_fg =
        (completion_selection_fg_color && completion_selection_fg_color[0] == '#') ?
            completion_selection_fg_color : "#11111b";

    const char *effective_dialog_bg =
        (dialog_bg_color && dialog_bg_color[0] == '#') ? dialog_bg_color : "#1b1f24";
    const char *effective_dialog_fg =
        (dialog_fg_color && dialog_fg_color[0] == '#') ? dialog_fg_color : effective_fg;
    const char *effective_dialog_border =
        (dialog_border_color && dialog_border_color[0] == '#') ? dialog_border_color : "#00000000";
    const char *effective_dialog_title =
        (dialog_title_color && dialog_title_color[0] == '#') ? dialog_title_color : "#ffffff";
    const char *effective_dialog_body =
        (dialog_body_color && dialog_body_color[0] == '#') ? dialog_body_color : effective_dialog_fg;
    const char *effective_dialog_muted =
        (dialog_muted_color && dialog_muted_color[0] == '#') ? dialog_muted_color : "#a6adc8";
    const char *effective_dialog_output =
        (dialog_output_color && dialog_output_color[0] == '#') ? dialog_output_color : effective_dialog_fg;
    const char *effective_git_output_bg =
        (git_output_bg_color && git_output_bg_color[0] == '#') ? git_output_bg_color : effective_dialog_bg;
    const char *effective_dialog_action =
        (dialog_action_color && dialog_action_color[0] == '#') ? dialog_action_color : effective_button_fg;
    const char *effective_dialog_destructive =
        (dialog_destructive_action_color && dialog_destructive_action_color[0] == '#') ?
            dialog_destructive_action_color : "#ff6b6b";
    const char *effective_dialog_input_fg =
        (dialog_input_fg_color && dialog_input_fg_color[0] == '#') ? dialog_input_fg_color : effective_input_fg;
    const char *effective_dialog_input_bg =
        (dialog_input_bg_color && dialog_input_bg_color[0] == '#') ? dialog_input_bg_color : effective_input_bg;
    const char *effective_search_match_bg =
        (search_match_bg_color && search_match_bg_color[0] == '#') ? search_match_bg_color : "#515c7a";
    const char *effective_search_match_fg =
        (search_match_fg_color && search_match_fg_color[0] == '#') ? search_match_fg_color : "#ffffff";
    const char *effective_diag_warn_bg =
        (diagnostic_warning_bg_color && diagnostic_warning_bg_color[0] == '#') ? diagnostic_warning_bg_color : "#5f4b24";
    const char *effective_diag_warn_fg =
        (diagnostic_warning_fg_color && diagnostic_warning_fg_color[0] == '#') ? diagnostic_warning_fg_color : "#ffd166";
    const char *effective_codex_preview =
        (codex_preview_bg_color && codex_preview_bg_color[0] == '#')
            ? codex_preview_bg_color : effective_bg;
    const char *effective_codex_preview_fg =
        (codex_preview_fg_color && codex_preview_fg_color[0] == '#')
            ? codex_preview_fg_color : effective_fg;
    const char *effective_codex_prompt =
        (codex_prompt_bg_color && codex_prompt_bg_color[0] == '#')
            ? codex_prompt_bg_color : effective_bg;
    const char *effective_ui_font =
        (ui_font && ui_font[0] != '\0') ? ui_font : NULL;
    const char *effective_editor_font =
        (editor_font && editor_font[0] != '\0') ? editor_font : NULL;
    const char *effective_preview_font =
        (preview_font && preview_font[0] != '\0') ? preview_font : NULL;
    const char *effective_terminal_font =
        (terminal_font && terminal_font[0] != '\0') ? terminal_font : NULL;
    const char *effective_code_font =
        (code_font && code_font[0] != '\0') ? code_font : "monospace";

    append_font_rule(css,
        ".graptos-root, .graptos-root label, .graptos-root button, "
        ".graptos-root entry, .graptos-root popover, "
        ".graptos-root popover contents, .graptos-root listbox, "
        ".graptos-root listbox row, .graptos-root notebook > header, "
        ".graptos-root notebook > header label, .graptos-top, "
        ".graptos-search-panel, .graptos-tool-panel, .graptos-bottom, "
        ".graptos-project-pane, .graptos-codex-panel, "
        ".graptos-terminal-panel",
        effective_ui_font);
    append_font_rule(css,
        ".graptos-dialog-output, .graptos-dialog-output text, "
        "textview.graptos-dialog-output, textview.graptos-dialog-output text, "
        "sourceview.graptos-dialog-output, sourceview.graptos-dialog-output text",
        effective_ui_font);
    append_font_rule(css, "tooltip, tooltip label", effective_ui_font);

    g_string_append_printf(css,
        "window.graptos-window, window.graptos-window > contents, "
        "window.graptos-window > contents > *, "
        "window.graptos-window box, window.graptos-window paned, "
        "window.graptos-window stack, window.graptos-window revealer, "
        "window.graptos-window scrolledwindow, window.graptos-window viewport, "
        ".graptos-root, .graptos-root > box, .graptos-tab-page, "
        ".graptos-editor-area, .graptos-editor-overlay, "
        ".graptos-root paned, .graptos-root revealer, "
        ".graptos-root frame, .graptos-root viewport, "
        ".graptos-root scrolledwindow, .graptos-root stack, "
        ".graptos-root notebook, .graptos-root notebook > stack, "
        ".graptos-root notebook > stack > box { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_bg, effective_bg, effective_fg);

    g_string_append_printf(css,
        ".graptos-root entry, .graptos-root entry text, "
        ".graptos-root spinbutton, .graptos-root spinbutton text, "
        ".graptos-root dropdown, .graptos-root combobox { "
        "background: %s; background-color: %s; color: %s; "
        "border-color: %s; }\n",
        effective_input_bg, effective_input_bg, effective_input_fg,
        effective_input_border);
    g_string_append_printf(css,
        ".graptos-root button { "
        "background: %s; background-color: %s; color: %s; }\n"
        ".graptos-root button:hover { "
        "background: %s; background-color: %s; color: %s; }\n"
        ".graptos-root button:checked, .graptos-root button:active { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_button_bg, effective_button_bg, effective_button_fg,
        effective_button_hover_bg, effective_button_hover_bg, effective_button_fg,
        effective_button_active_bg, effective_button_active_bg, effective_button_fg);
    g_string_append_printf(css,
        "button.graptos-preview-detach-button, "
        "window.graptos-preview-window button.graptos-preview-detach-button { "
        "background: %s; background-color: %s; color: %s; }\n"
        "button.graptos-preview-detach-button:hover, "
        "window.graptos-preview-window button.graptos-preview-detach-button:hover { "
        "background: %s; background-color: %s; color: %s; }\n"
        "button.graptos-preview-detach-button:active, "
        "window.graptos-preview-window button.graptos-preview-detach-button:active { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_button_bg, effective_button_bg, effective_button_fg,
        effective_button_hover_bg, effective_button_hover_bg, effective_button_fg,
        effective_button_active_bg, effective_button_active_bg, effective_button_fg);
    g_string_append_printf(css,
        ".graptos-root list, .graptos-root listview, .graptos-root columnview, "
        ".graptos-root treeview, .graptos-root row { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_bg, effective_bg, effective_fg);

    g_string_append_printf(css,
        ".graptos-editor, .graptos-editor text, "
        "textview.graptos-editor, textview.graptos-editor text, "
        "sourceview.graptos-editor, sourceview.graptos-editor text { "
        "background: %s; background-color: %s; color: %s; caret-color: %s; }\n"
        "sourceview.graptos-editor gutter, sourceview.graptos-editor gutter * { "
        "background: %s; background-color: %s; color: %s; }\n"
        "sourceview.graptos-editor gutter renderer, "
        "sourceview.graptos-editor gutter lines, "
        "sourceview.graptos-editor gutter marks { "
        "background: %s; background-color: %s; color: %s; }\n"
        "sourceview.graptos-editor text selection { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_bg, effective_bg, effective_fg, effective_cursor,
        effective_gutter_bg, effective_gutter_bg, effective_gutter_fg,
        effective_gutter_bg, effective_gutter_bg, effective_gutter_fg,
        effective_selection_bg, effective_selection_bg, effective_selection_fg);
    if (effective_editor_font) {
        append_font_rule(css,
            ".graptos-editor, .graptos-editor text, "
            "textview.graptos-editor, textview.graptos-editor text, "
            "sourceview.graptos-editor, sourceview.graptos-editor text",
            effective_editor_font);
    } else if (!use_system_interface_font) {
        g_string_append(css,
            ".graptos-editor, .graptos-editor text, "
            "textview.graptos-editor, textview.graptos-editor text, "
            "sourceview.graptos-editor, sourceview.graptos-editor text { "
            "font-family: monospace; }\n");
    }
    g_string_append_printf(css,
        ".graptos-current-line { background: %s; background-color: %s; }\n",
        effective_current_line, effective_current_line);
    append_bg_rule(css, ".graptos-top", effective_topbar_bg);
    append_fg_rule(css, ".graptos-top", effective_topbar_fg);
    append_fg_rule(css, ".graptos-top label", effective_topbar_fg);
    append_bg_rule(css, ".graptos-search-panel", effective_bottombar_bg);
    append_fg_rule(css, ".graptos-search-panel label", effective_bottombar_fg);
    append_bg_rule(css, ".graptos-tool-panel", effective_bottombar_bg);
    append_fg_rule(css, ".graptos-tool-panel label", effective_bottombar_fg);
    append_bg_rule(css, ".graptos-bottom", effective_bottombar_bg);
    append_fg_rule(css, ".graptos-bottom", effective_bottombar_fg);
    append_fg_rule(css, ".graptos-bottom label", effective_bottombar_fg);
    append_fg_rule(css, ".graptos-bottom label.graptos-status-error", effective_status_error);
    append_fg_rule(css, "label.graptos-status-error", effective_status_error);
    append_fg_rule(css, ".graptos-root", effective_fg);
    append_fg_rule(css, ".graptos-root label", effective_fg);
    g_string_append_printf(css,
        ".graptos-project-pane, .graptos-project-tree, .graptos-project-tree row { "
        "background: %s; background-color: %s; color: %s; }\n"
        ".graptos-project-tree row:selected, .graptos-project-tree row:hover { "
        "background: %s; background-color: %s; color: %s; }\n"
        ".graptos-project-tree label { color: %s; }\n",
        effective_sidebar, effective_sidebar, effective_project_fg,
        effective_project_selected_bg, effective_project_selected_bg,
        effective_project_selected_fg,
        effective_project_fg);
    g_string_append_printf(css,
        ".graptos-project-tree label.graptos-git-status { "
        "font-weight: 800; opacity: 1.0; }\n"
        ".graptos-project-tree label.graptos-git-status-modified { color: %s; }\n"
        ".graptos-project-tree label.graptos-git-status-added { color: %s; }\n"
        ".graptos-project-tree label.graptos-git-status-deleted { color: %s; }\n"
        ".graptos-project-tree label.graptos-git-status-renamed { color: %s; }\n"
        ".graptos-project-tree label.graptos-git-status-conflict { color: %s; }\n"
        ".graptos-project-tree label.graptos-git-status-untracked { color: %s; }\n"
        ".graptos-project-tree label.graptos-git-status-staged { color: %s; }\n",
        effective_git_modified, effective_git_added, effective_git_deleted,
        effective_git_renamed, effective_git_conflict,
        effective_git_untracked, effective_git_staged);
    g_string_append_printf(css,
        ".graptos-root notebook > header { background: %s; background-color: %s; color: %s; }\n"
        ".graptos-root notebook > header.top > tabs > tab { background: %s; background-color: %s; color: %s; }\n"
        ".graptos-root notebook > header.top > tabs > tab:checked { background: %s; background-color: %s; color: %s; }\n"
        ".graptos-tab label { color: %s; opacity: 0.68; }\n"
        ".graptos-root notebook > header.top > tabs > tab:checked .graptos-tab label { color: %s; opacity: 1.0; }\n",
        effective_tabbar, effective_tabbar, effective_tabbar_fg,
        effective_tabbar, effective_tabbar, effective_tabbar_fg,
        effective_tab_active_bg, effective_tab_active_bg, effective_tab_active_fg,
        effective_tabbar_fg,
        effective_tab_active_fg);
    g_string_append_printf(css,
        ".graptos-minimap, .graptos-minimap text, "
        "textview.graptos-minimap, textview.graptos-minimap text, "
        "sourceview.graptos-minimap, sourceview.graptos-minimap text { "
        "background: %s; background-color: %s; color: %s; }\n"
        "sourceview.graptos-minimap gutter, sourceview.graptos-minimap gutter * { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_preview, effective_preview, effective_preview_fg,
        effective_preview, effective_preview, effective_preview_fg);
    append_bg_rule(css, ".graptos-preview-box", effective_preview);
    append_bg_rule(css, ".graptos-preview-header", effective_preview);
    append_bg_rule(css, ".graptos-preview-paned", effective_bg);
    append_bg_rule(css, "window.graptos-preview-window", effective_preview);
    append_bg_rule(css, "window.graptos-preview-window > contents", effective_preview);
    append_bg_rule(css, "window.graptos-preview-window > contents > *", effective_preview);
    append_bg_rule(css, ".graptos-preview", effective_preview);
    append_bg_rule(css, ".graptos-preview text", effective_preview);
    append_fg_rule(css, ".graptos-preview-box", effective_preview_fg);
    append_fg_rule(css, ".graptos-preview-header", effective_preview_fg);
    append_fg_rule(css, ".graptos-preview-title", effective_preview_fg);
    append_fg_rule(css, ".graptos-preview", effective_preview_fg);
    append_fg_rule(css, ".graptos-preview text", effective_preview_fg);
    append_font_rule(css,
                     ".graptos-preview, .graptos-preview text",
                     effective_preview_font);
    append_bg_rule(css, ".graptos-codex-preview", effective_codex_preview);
    append_bg_rule(css, ".graptos-codex-preview text", effective_codex_preview);
    append_fg_rule(css, ".graptos-codex-preview", effective_codex_preview_fg);
    append_fg_rule(css, ".graptos-codex-preview text", effective_codex_preview_fg);
    append_bg_rule(css, ".graptos-codex-prompt", effective_codex_prompt);
    append_bg_rule(css, ".graptos-codex-prompt text", effective_codex_prompt);
    g_string_append_printf(css,
        "popover.graptos-context-popover, popover.graptos-tools-popover, "
        "popover.graptos-context-popover contents, popover.graptos-tools-popover contents { "
        "background: %s; background-color: %s; "
        "border: 1px solid %s; border-color: %s; }\n",
        effective_popover, effective_popover,
        effective_popover_border, effective_popover_border);
    g_string_append_printf(css,
        "popover.graptos-completion-popover, popover.graptos-hover-popover, "
        "popover.graptos-completion-popover contents, "
        "popover.graptos-hover-popover contents { "
        "border: 1px solid %s; border-color: %s; }\n",
        effective_popover_border, effective_popover_border);
    g_string_append_printf(css,
        "tooltip, tooltip.background, tooltip > box { "
        "background: %s; background-color: %s; color: %s; "
        "border: 1px solid %s; border-color: %s; "
        "border-radius: 0; box-shadow: none; }\n"
        "tooltip label { color: %s; }\n",
        effective_tooltip_bg, effective_tooltip_bg, effective_tooltip_fg,
        effective_tooltip_border, effective_tooltip_border,
        effective_tooltip_fg);

    g_string_append_printf(css,
        "popover.graptos-completion-popover, "
        "popover.graptos-completion-popover contents, "
        ".graptos-completion-scroll, .graptos-completion-list, "
        ".graptos-completion-list row { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_completion_bg, effective_completion_bg,
        effective_completion_fg);
    g_string_append_printf(css,
        ".graptos-completion-list label, .graptos-completion-title { "
        "color: %s; }\n",
        effective_completion_fg);
    g_string_append_printf(css,
        ".graptos-completion-detail { color: %s; }\n",
        effective_completion_detail);
    append_font_rule(css,
                     ".graptos-completion-title, .graptos-completion-detail, "
                     ".graptos-ref-title, .graptos-ref-snippet",
                     effective_code_font);
    g_string_append_printf(css,
        ".graptos-completion-list row:selected, "
        ".graptos-completion-list row:selected label, "
        ".graptos-completion-list row:selected .graptos-completion-title, "
        ".graptos-completion-list row:selected .graptos-completion-detail { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_completion_selection_bg, effective_completion_selection_bg,
        effective_completion_selection_fg);
    g_string_append_printf(css,
        ".graptos-completion-list row:hover { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_completion_selection_bg, effective_completion_selection_bg,
        effective_completion_selection_fg);

    g_string_append_printf(css,
        "popover.graptos-hover-popover, popover.graptos-hover-popover contents, "
        ".graptos-ref-list, .graptos-ref-list row { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_ref_bg, effective_ref_bg, effective_ref_fg);
    g_string_append_printf(css,
        ".graptos-ref-list label { color: %s; }\n", effective_ref_fg);
    g_string_append_printf(css,
        ".graptos-ref-heading { color: %s; }\n", effective_ref_heading);
    g_string_append_printf(css,
        ".graptos-ref-title { color: %s; }\n", effective_ref_title);
    g_string_append_printf(css,
        ".graptos-ref-kind { color: %s; }\n", effective_ref_kind);
    g_string_append_printf(css,
        ".graptos-ref-snippet { color: %s; }\n", effective_ref_snippet);
    g_string_append_printf(css,
        ".graptos-ref-list row:hover, "
        ".graptos-ref-list row:hover label, "
        ".graptos-ref-list row:hover .graptos-ref-heading, "
        ".graptos-ref-list row:hover .graptos-ref-title, "
        ".graptos-ref-list row:hover .graptos-ref-kind, "
        ".graptos-ref-list row:hover .graptos-ref-snippet { "
        "background: %s; background-color: %s; color: %s; "
        "border: none; outline: none; box-shadow: none; }\n",
        effective_ref_hover_bg, effective_ref_hover_bg, effective_ref_hover_fg);

    g_string_append_printf(css,
        "window.graptos-window.graptos-dialog, window.graptos-window dialog, "
        "window.graptos-window .graptos-dialog, "
        "window.graptos-window .graptos-root.graptos-dialog-root { "
        "background: %s; background-color: %s; color: %s; }\n"
        "window.graptos-window .graptos-root.graptos-dialog-root { "
        "border: 1px solid %s; border-color: %s; }\n"
        "window.graptos-window .graptos-root.graptos-dialog-root.graptos-flat-dialog { "
        "border: none; border-color: transparent; box-shadow: none; outline: none; }\n"
        "window.graptos-window .graptos-root.graptos-dialog-root.graptos-about-dialog-root { "
        "background: %s; background-color: %s; color: %s; "
        "border: none; border-color: transparent; box-shadow: none; outline: none; }\n"
        "window.graptos-window.graptos-about-dialog, "
        "window.graptos-window.graptos-about-dialog > contents, "
        "window.graptos-window.graptos-about-dialog > contents > * { "
        "background: %s; background-color: %s; color: %s; "
        "border: none; border-color: transparent; box-shadow: none; outline: none; }\n"
        "window.graptos-window .graptos-root.graptos-dialog-root.graptos-output-dialog-root, "
        "window.graptos-window .graptos-dialog-output-scroll, "
        "window.graptos-window .graptos-dialog-output-scroll viewport, "
        "window.graptos-window .graptos-dialog-output, "
        "window.graptos-window .graptos-dialog-output text, "
        "window.graptos-window textview.graptos-dialog-output, "
        "window.graptos-window textview.graptos-dialog-output text, "
        "window.graptos-window sourceview.graptos-dialog-output, "
        "window.graptos-window sourceview.graptos-dialog-output text, "
        "window.graptos-window sourceview.graptos-dialog-output gutter, "
        "window.graptos-window sourceview.graptos-dialog-output gutter * { "
        "background: %s; background-color: %s; color: %s; "
        "border: none; border-color: transparent; box-shadow: none; outline: none; }\n"
        ".graptos-window .graptos-dialog-title, "
        ".graptos-window .graptos-menu-title { color: %s; }\n"
        ".graptos-window .graptos-dialog-body { color: %s; }\n"
        ".graptos-window .graptos-dialog-muted { color: %s; }\n"
        ".graptos-window .graptos-dialog-output, "
        ".graptos-window .graptos-dialog-output text, "
        ".graptos-window textview.graptos-dialog-output, "
        ".graptos-window textview.graptos-dialog-output text, "
        ".graptos-window sourceview.graptos-dialog-output, "
        ".graptos-window sourceview.graptos-dialog-output text { color: %s; }\n"
        ".graptos-window .graptos-dialog-action { color: %s; }\n"
        ".graptos-window button.graptos-dialog-action label { color: %s; }\n"
        ".graptos-window .graptos-dialog-action-destructive { color: %s; }\n"
        ".graptos-window button.graptos-dialog-action-destructive label { color: %s; }\n"
        ".graptos-window .graptos-root.graptos-dialog-root entry, "
        ".graptos-window .graptos-root.graptos-dialog-root entry text, "
        ".graptos-window .graptos-root.graptos-dialog-root textview, "
        ".graptos-window .graptos-root.graptos-dialog-root textview text { "
        "background: %s; background-color: %s; color: %s; }\n"
        ".graptos-window .graptos-dialog-output, "
        ".graptos-window .graptos-dialog-output text, "
        ".graptos-window textview.graptos-dialog-output, "
        ".graptos-window textview.graptos-dialog-output text, "
        ".graptos-window sourceview.graptos-dialog-output, "
        ".graptos-window sourceview.graptos-dialog-output text, "
        ".graptos-window sourceview.graptos-dialog-output gutter, "
        ".graptos-window sourceview.graptos-dialog-output gutter * { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_dialog_bg, effective_dialog_bg, effective_dialog_fg,
        effective_dialog_border, effective_dialog_border,
        effective_bg, effective_bg, effective_fg,
        effective_bg, effective_bg, effective_fg,
        effective_dialog_bg, effective_dialog_bg, effective_dialog_output,
        effective_dialog_title,
        effective_dialog_body,
        effective_dialog_muted,
        effective_dialog_output,
        effective_dialog_action,
        effective_dialog_action,
        effective_dialog_destructive,
        effective_dialog_destructive,
        effective_dialog_input_bg,
        effective_dialog_input_bg,
        effective_dialog_input_fg,
        effective_dialog_bg,
        effective_dialog_bg,
        effective_dialog_output);
    g_string_append_printf(css,
        "window.graptos-window .graptos-git-output-dialog-root, "
        "window.graptos-window .graptos-git-dialog-output-scroll, "
        "window.graptos-window .graptos-git-dialog-output-scroll viewport, "
        "window.graptos-window .graptos-git-dialog-output, "
        "window.graptos-window .graptos-git-dialog-output text, "
        "window.graptos-window .graptos-dialog-output.graptos-git-dialog-output, "
        "window.graptos-window .graptos-dialog-output.graptos-git-dialog-output text, "
        "window.graptos-window textview.graptos-dialog-output.graptos-git-dialog-output, "
        "window.graptos-window textview.graptos-dialog-output.graptos-git-dialog-output text, "
        "window.graptos-window sourceview.graptos-git-dialog-output, "
        "window.graptos-window sourceview.graptos-git-dialog-output text, "
        "window.graptos-window sourceview.graptos-dialog-output.graptos-git-dialog-output, "
        "window.graptos-window sourceview.graptos-dialog-output.graptos-git-dialog-output text, "
        "window.graptos-window sourceview.graptos-git-dialog-output gutter, "
        "window.graptos-window sourceview.graptos-git-dialog-output gutter *, "
        "window.graptos-window sourceview.graptos-dialog-output.graptos-git-dialog-output gutter, "
        "window.graptos-window sourceview.graptos-dialog-output.graptos-git-dialog-output gutter * { "
        "background: %s; background-color: %s; color: %s; "
        "border: none; border-color: transparent; box-shadow: none; outline: none; }\n",
        effective_git_output_bg, effective_git_output_bg, effective_dialog_output);
    g_string_append_printf(css,
        ".graptos-search-match { background: %s; color: %s; }\n"
        ".graptos-diagnostic-warning { background: %s; color: %s; }\n",
        effective_search_match_bg, effective_search_match_fg,
        effective_diag_warn_bg, effective_diag_warn_fg);
    append_font_rule(css, ".graptos-terminal", effective_terminal_font);

    if (css->len == 0u) {
        g_string_free(css, TRUE);
        return;
    }

    dynamic_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(dynamic_provider, css->str);
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(dynamic_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 10u);
    g_string_free(css, TRUE);
}
