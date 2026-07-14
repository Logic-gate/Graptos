/**
 * @file src/ui/ui_editor_css.inc.c
 * @brief Cleaf ui editor css module.
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
 */
static void append_fg_rule(GString *css, const char *selector,
                           const char *color) {
    if (!css || !selector || !color || color[0] != '#') return;
    g_string_append_printf(css, "%s { color: %s; }\n", selector, color);
}

/*
 * Cleaf applies color config with CSS because many GTK widgets involved here
 * are constructed before file-specific SourceView schemes are known.  The CSS
 * covers container chrome and popovers; GtkSourceView token colors are handled
 * separately through generated style schemes.
 */
void cleaf_apply_editor_css(const char *editor_bg_color,
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
                            const char *scroll_preview_bg_color,
                            const char *scroll_preview_fg_color,
                            const char *popover_bg_color,
                            const char *ref_popover_bg_color,
                            const char *ref_popover_fg_color,
                            const char *ref_popover_heading_color,
                            const char *ref_popover_title_color,
                            const char *ref_popover_kind_color,
                            const char *ref_popover_snippet_color,
                            const char *completion_popover_bg_color,
                            const char *completion_popover_fg_color,
                            const char *completion_popover_detail_color,
                            const char *completion_selection_bg_color,
                            const char *completion_selection_fg_color,
                            const char *dialog_bg_color,
                            const char *dialog_fg_color,
                            const char *dialog_border_color,
                            const char *dialog_title_color,
                            const char *search_match_bg_color,
                            const char *search_match_fg_color,
                            const char *diagnostic_warning_bg_color,
                            const char *diagnostic_warning_fg_color,
                            const char *codex_preview_bg_color,
                            const char *codex_preview_fg_color,
                            const char *codex_prompt_bg_color,
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
    const char *effective_preview =
        (scroll_preview_bg_color && scroll_preview_bg_color[0] == '#') ?
            scroll_preview_bg_color : effective_bg;
    const char *effective_preview_fg =
        (scroll_preview_fg_color && scroll_preview_fg_color[0] == '#') ?
            scroll_preview_fg_color : "#8b949e";
    const char *effective_popover =
        (popover_bg_color && popover_bg_color[0] == '#') ? popover_bg_color : effective_bg;
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
        (dialog_border_color && dialog_border_color[0] == '#') ? dialog_border_color : "#f1e0a8";
    const char *effective_dialog_title =
        (dialog_title_color && dialog_title_color[0] == '#') ? dialog_title_color : "#ffffff";
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

    g_string_append_printf(css,
        "window.cleaf-window, window.cleaf-window > contents, "
        "window.cleaf-window > contents > *, "
        "window.cleaf-window box, window.cleaf-window paned, "
        "window.cleaf-window stack, window.cleaf-window revealer, "
        "window.cleaf-window scrolledwindow, window.cleaf-window viewport, "
        ".cleaf-root, .cleaf-root > box, .cleaf-tab-page, "
        ".cleaf-editor-area, .cleaf-editor-overlay, "
        ".cleaf-root paned, .cleaf-root revealer, "
        ".cleaf-root frame, .cleaf-root viewport, "
        ".cleaf-root scrolledwindow, .cleaf-root stack, "
        ".cleaf-root notebook, .cleaf-root notebook > stack, "
        ".cleaf-root notebook > stack > box { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_bg, effective_bg, effective_fg);

    g_string_append_printf(css,
        ".cleaf-root entry, .cleaf-root entry text, "
        ".cleaf-root spinbutton, .cleaf-root spinbutton text, "
        ".cleaf-root dropdown, .cleaf-root combobox { "
        "background: %s; background-color: %s; color: %s; "
        "border-color: %s; }\n",
        effective_input_bg, effective_input_bg, effective_input_fg,
        effective_input_border);
    g_string_append_printf(css,
        ".cleaf-root button { "
        "background: %s; background-color: %s; color: %s; }\n"
        ".cleaf-root button:hover { "
        "background: %s; background-color: %s; color: %s; }\n"
        ".cleaf-root button:checked, .cleaf-root button:active { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_button_bg, effective_button_bg, effective_button_fg,
        effective_button_hover_bg, effective_button_hover_bg, effective_button_fg,
        effective_button_active_bg, effective_button_active_bg, effective_button_fg);
    g_string_append_printf(css,
        ".cleaf-root list, .cleaf-root listview, .cleaf-root columnview, "
        ".cleaf-root treeview, .cleaf-root row { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_bg, effective_bg, effective_fg);

    g_string_append_printf(css,
        ".cleaf-editor, .cleaf-editor text, "
        "textview.cleaf-editor, textview.cleaf-editor text, "
        "sourceview.cleaf-editor, sourceview.cleaf-editor text { "
        "background: %s; background-color: %s; color: %s; caret-color: %s; }\n"
        "sourceview.cleaf-editor gutter, sourceview.cleaf-editor gutter * { "
        "background: %s; background-color: %s; color: %s; }\n"
        "sourceview.cleaf-editor gutter renderer, "
        "sourceview.cleaf-editor gutter lines, "
        "sourceview.cleaf-editor gutter marks { "
        "background: %s; background-color: %s; color: %s; }\n"
        "sourceview.cleaf-editor text selection { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_bg, effective_bg, effective_fg, effective_cursor,
        effective_gutter_bg, effective_gutter_bg, effective_gutter_fg,
        effective_gutter_bg, effective_gutter_bg, effective_gutter_fg,
        effective_selection_bg, effective_selection_bg, effective_selection_fg);
    if (!use_system_interface_font) {
        g_string_append(css,
            ".cleaf-editor, .cleaf-editor text, "
            "textview.cleaf-editor, textview.cleaf-editor text, "
            "sourceview.cleaf-editor, sourceview.cleaf-editor text { "
            "font-family: monospace; }\n");
    }
    g_string_append_printf(css,
        ".cleaf-current-line { background: %s; background-color: %s; }\n",
        effective_current_line, effective_current_line);
    append_bg_rule(css, ".cleaf-top", effective_topbar_bg);
    append_fg_rule(css, ".cleaf-top", effective_topbar_fg);
    append_fg_rule(css, ".cleaf-top label", effective_topbar_fg);
    append_bg_rule(css, ".cleaf-search-panel", effective_bottombar_bg);
    append_fg_rule(css, ".cleaf-search-panel label", effective_bottombar_fg);
    append_bg_rule(css, ".cleaf-tool-panel", effective_bottombar_bg);
    append_fg_rule(css, ".cleaf-tool-panel label", effective_bottombar_fg);
    append_bg_rule(css, ".cleaf-bottom", effective_bottombar_bg);
    append_fg_rule(css, ".cleaf-bottom", effective_bottombar_fg);
    append_fg_rule(css, ".cleaf-bottom label", effective_bottombar_fg);
    append_fg_rule(css, ".cleaf-root", effective_fg);
    append_fg_rule(css, ".cleaf-root label", effective_fg);
    g_string_append_printf(css,
        ".cleaf-project-pane, .cleaf-project-tree, .cleaf-project-tree row { "
        "background: %s; background-color: %s; color: %s; }\n"
        ".cleaf-project-tree row:selected, .cleaf-project-tree row:hover { "
        "background: %s; background-color: %s; color: %s; }\n"
        ".cleaf-project-tree label { color: %s; }\n",
        effective_sidebar, effective_sidebar, effective_project_fg,
        effective_project_selected_bg, effective_project_selected_bg,
        effective_project_selected_fg,
        effective_project_fg);
    g_string_append(css,
        ".cleaf-project-tree label.cleaf-git-status { "
        "font-weight: 800; opacity: 1.0; }\n"
        ".cleaf-project-tree label.cleaf-git-status-modified { color: #f9c74f; }\n"
        ".cleaf-project-tree label.cleaf-git-status-added { color: #57cc99; }\n"
        ".cleaf-project-tree label.cleaf-git-status-deleted { color: #ff6b6b; }\n"
        ".cleaf-project-tree label.cleaf-git-status-renamed { color: #4cc9f0; }\n"
        ".cleaf-project-tree label.cleaf-git-status-conflict { color: #ff4d6d; }\n"
        ".cleaf-project-tree label.cleaf-git-status-untracked { color: #77bdfb; }\n"
        ".cleaf-project-tree label.cleaf-git-status-staged { color: #cba6f7; }\n");
    g_string_append_printf(css,
        ".cleaf-root notebook > header { background: %s; background-color: %s; color: %s; }\n"
        ".cleaf-root notebook > header.top > tabs > tab { background: %s; background-color: %s; color: %s; }\n"
        ".cleaf-root notebook > header.top > tabs > tab:checked { background: %s; background-color: %s; color: %s; }\n"
        ".cleaf-tab label { color: %s; opacity: 0.68; }\n"
        ".cleaf-root notebook > header.top > tabs > tab:checked .cleaf-tab label { color: %s; opacity: 1.0; }\n",
        effective_tabbar, effective_tabbar, effective_tabbar_fg,
        effective_tabbar, effective_tabbar, effective_tabbar_fg,
        effective_tab_active_bg, effective_tab_active_bg, effective_tab_active_fg,
        effective_tabbar_fg,
        effective_tab_active_fg);
    g_string_append_printf(css,
        ".cleaf-minimap, .cleaf-minimap text, "
        "textview.cleaf-minimap, textview.cleaf-minimap text, "
        "sourceview.cleaf-minimap, sourceview.cleaf-minimap text { "
        "background: %s; background-color: %s; color: %s; }\n"
        "sourceview.cleaf-minimap gutter, sourceview.cleaf-minimap gutter * { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_preview, effective_preview, effective_preview_fg,
        effective_preview, effective_preview, effective_preview_fg);
    append_bg_rule(css, ".cleaf-preview", effective_preview);
    append_bg_rule(css, ".cleaf-preview text", effective_preview);
    append_fg_rule(css, ".cleaf-preview", effective_fg);
    append_fg_rule(css, ".cleaf-preview text", effective_fg);
    append_bg_rule(css, ".cleaf-codex-preview", effective_codex_preview);
    append_bg_rule(css, ".cleaf-codex-preview text", effective_codex_preview);
    append_fg_rule(css, ".cleaf-codex-preview", effective_codex_preview_fg);
    append_fg_rule(css, ".cleaf-codex-preview text", effective_codex_preview_fg);
    append_bg_rule(css, ".cleaf-codex-prompt", effective_codex_prompt);
    append_bg_rule(css, ".cleaf-codex-prompt text", effective_codex_prompt);
    append_bg_rule(css, "popover.cleaf-tools-popover contents", effective_popover);

    g_string_append_printf(css,
        "popover.cleaf-completion-popover, "
        "popover.cleaf-completion-popover contents, "
        ".cleaf-completion-scroll, .cleaf-completion-list, "
        ".cleaf-completion-list row { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_completion_bg, effective_completion_bg,
        effective_completion_fg);
    g_string_append_printf(css,
        ".cleaf-completion-list label, .cleaf-completion-title { "
        "color: %s; }\n",
        effective_completion_fg);
    g_string_append_printf(css,
        ".cleaf-completion-detail { color: %s; }\n",
        effective_completion_detail);
    g_string_append_printf(css,
        ".cleaf-completion-list row:selected, "
        ".cleaf-completion-list row:selected label, "
        ".cleaf-completion-list row:selected .cleaf-completion-title, "
        ".cleaf-completion-list row:selected .cleaf-completion-detail { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_completion_selection_bg, effective_completion_selection_bg,
        effective_completion_selection_fg);
    g_string_append_printf(css,
        ".cleaf-completion-list row:hover { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_completion_selection_bg, effective_completion_selection_bg,
        effective_completion_selection_fg);

    g_string_append_printf(css,
        "popover.cleaf-hover-popover, popover.cleaf-hover-popover contents, "
        ".cleaf-ref-list, .cleaf-ref-list row { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_ref_bg, effective_ref_bg, effective_ref_fg);
    g_string_append_printf(css,
        ".cleaf-ref-list label { color: %s; }\n", effective_ref_fg);
    g_string_append_printf(css,
        ".cleaf-ref-heading { color: %s; }\n", effective_ref_heading);
    g_string_append_printf(css,
        ".cleaf-ref-title { color: %s; }\n", effective_ref_title);
    g_string_append_printf(css,
        ".cleaf-ref-kind { color: %s; }\n", effective_ref_kind);
    g_string_append_printf(css,
        ".cleaf-ref-snippet { color: %s; }\n", effective_ref_snippet);
    g_string_append_printf(css,
        ".cleaf-ref-list row:hover { "
        "background: %s; background-color: %s; color: %s; }\n",
        effective_ref_title, effective_ref_title, effective_ref_fg);

    g_string_append_printf(css,
        "window.cleaf-window dialog, window.cleaf-window .cleaf-dialog, "
        "window.cleaf-window .cleaf-root.cleaf-dialog-root { "
        "background: %s; background-color: %s; color: %s; "
        "border-color: %s; }\n"
        ".cleaf-window .cleaf-menu-title { color: %s; }\n",
        effective_dialog_bg, effective_dialog_bg, effective_dialog_fg,
        effective_dialog_border, effective_dialog_title);
    g_string_append_printf(css,
        ".cleaf-search-match { background: %s; color: %s; }\n"
        ".cleaf-diagnostic-warning { background: %s; color: %s; }\n",
        effective_search_match_bg, effective_search_match_fg,
        effective_diag_warn_bg, effective_diag_warn_fg);

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
