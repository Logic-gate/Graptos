/**
 * @file src/app/app_about_view_actions.inc.c
 * @brief Graptoς app about view actions module.
 * @details These actions are presentation glue. We keep them away from the editor core
 *          because dialogs and theme controls change often, while the buffer and project
 *          code should not care how those controls are displayed.
 */

/**
 * @brief Create a section title label for preference pages.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *pref_section(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "graptos-menu-title");
    return label;
}

/**
 * @brief Pref tab label.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *pref_tab_label(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    /**
     * @brief Keep preference tabs visually wider than their raw text labels.
     */
    gtk_widget_set_margin_start(label, 14);
    gtk_widget_set_margin_end(label, 14);
    gtk_widget_set_margin_top(label, 6);
    gtk_widget_set_margin_bottom(label, 6);
    gtk_widget_add_css_class(label, "graptos-pref-tab");
    return label;
}


/**
 * @brief Graptoς about label new.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param text The text fragment supplied by the caller.
 * @param title The title supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *graptos_about_label_new(const char *text, gboolean title) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.5f);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    if (title) gtk_widget_add_css_class(label, "graptos-menu-title");
    return label;
}

/**
 * @brief Create the Graptoς Inconsolata wordmark for the About dialog.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *graptos_about_wordmark_new(void) {
    GtkWidget *label = gtk_label_new(GRAPTOS_DISPLAY_NAME);
    gtk_label_set_xalign(GTK_LABEL(label), 0.5f);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(label, "graptos-about-wordmark");
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    return label;
}

/**
 * @brief Create the italic Graptoς definition subtitle for the About dialog.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *graptos_about_subtitle_new(void) {
    GtkWidget *label =
        gtk_label_new("γραπτός: Grap-Tos; adj, written or inscribed");
    gtk_label_set_xalign(GTK_LABEL(label), 0.5f);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(label, "graptos-about-subtitle");
    gtk_widget_set_halign(label, GTK_ALIGN_CENTER);
    return label;
}

/**
 * @brief Action about.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_about(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    GtkWindow *parent = app_window_gtk(win);
    /**
     * @brief Use Graptoς's themed window shell instead of GtkAboutDialog.
     */
    GtkWidget *dialog = gtk_window_new();
    gtk_widget_add_css_class(dialog, "graptos-window");
    gtk_widget_add_css_class(dialog, "graptos-about-dialog");
    gtk_window_set_title(GTK_WINDOW(dialog), "About " GRAPTOS_DISPLAY_NAME);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 440, 330);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    if (parent) gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(box, "graptos-root");
    gtk_widget_add_css_class(box, "graptos-dialog-root");
    gtk_widget_add_css_class(box, "graptos-flat-dialog");
    gtk_widget_add_css_class(box, "graptos-about-dialog-root");
    graptos_set_all_margins(box, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    gtk_box_append(GTK_BOX(box), graptos_about_wordmark_new());
    gtk_box_append(GTK_BOX(box), graptos_about_subtitle_new());
    gtk_box_append(GTK_BOX(box), graptos_about_label_new(APP_VERSION, FALSE));
    gtk_box_append(GTK_BOX(box), graptos_about_label_new(
        "A compact C/GTK text editor with YAML syntax definitions and project-aware completion.",
        FALSE));
    gtk_box_append(GTK_BOX(box), graptos_about_label_new(
        "This program comes with absolutely no warranty.\nLicense: AGPL-3.0 license.",
        FALSE));
    gtk_box_append(GTK_BOX(box), graptos_about_label_new(
        "Bundled UI font: Inconsolata, licensed under the SIL Open Font License 1.1.\n"
        "Bundled theme palettes are sourced from https://terminalcolors.com.",
        FALSE));
    (void)graptos_modal_window_run(GTK_WINDOW(dialog), GTK_RESPONSE_CLOSE);
    graptos_widget_destroy(dialog);
}

/**
 * @brief One keyboard shortcut row for the shortcuts dialog.
 */
typedef struct {
    const char *keys; /**< Human-readable shortcut key sequence. */
    const char *action; /**< Human-readable shortcut action. */
} GraptosShortcutRow;

/**
 * @brief Add one shortcut row to a grid.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param grid The grid supplied by the caller.
 * @param row The row supplied by the caller.
 * @param shortcut The shortcut supplied by the caller.
 */
static void graptos_shortcuts_add_row(GtkGrid *grid,
                                      int row,
                                      const GraptosShortcutRow *shortcut) {
    if (!grid || !shortcut) return;

    GtkWidget *keys = gtk_label_new(shortcut->keys ? shortcut->keys : "");
    gtk_label_set_xalign(GTK_LABEL(keys), 0.0f);
    gtk_widget_add_css_class(keys, "graptos-shortcuts-key");
    gtk_grid_attach(grid, keys, 0, row, 1, 1);

    GtkWidget *action = gtk_label_new(shortcut->action ? shortcut->action : "");
    gtk_label_set_xalign(GTK_LABEL(action), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(action), TRUE);
    gtk_widget_add_css_class(action, "graptos-shortcuts-action");
    gtk_grid_attach(grid, action, 1, row, 1, 1);
}

/**
 * @brief Create a shortcuts section with a title and rows.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param title The title supplied by the caller.
 * @param rows The rows supplied by the caller.
 * @param n_rows The n rows supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *graptos_shortcuts_section_new(const char *title,
                                                const GraptosShortcutRow *rows,
                                                gsize n_rows) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

    GtkWidget *heading = graptos_about_label_new(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(heading), 0.0f);
    gtk_widget_set_halign(heading, GTK_ALIGN_FILL);
    gtk_box_append(GTK_BOX(box), heading);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_widget_set_hexpand(grid, TRUE);
    for (gsize i = 0u; i < n_rows; i++) {
        graptos_shortcuts_add_row(GTK_GRID(grid), (int)i, &rows[i]);
    }
    gtk_box_append(GTK_BOX(box), grid);
    return box;
}

/**
 * @brief Action shortcuts.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_shortcuts(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    GtkWindow *parent = app_window_gtk(win);

    static const GraptosShortcutRow file_rows[] = {
        {"Ctrl+N", "New tab"},
        {"Ctrl+T", "New tab"},
        {"Ctrl+O", "Open file"},
        {"Ctrl+Shift+O", "Open folder"},
        {"Ctrl+S", "Save current file"},
        {"Ctrl+Shift+S", "Save current file as"},
        {"Ctrl+W", "Close active tab"},
        {"Ctrl+PageDown", "Next editor tab"},
        {"Ctrl+PageUp", "Previous editor tab"},
    };
    static const GraptosShortcutRow view_rows[] = {
        {"Ctrl+B", "Toggle project tree"},
        {"Ctrl+`", "Toggle terminal panel"},
        {"Ctrl+Shift+I", "Toggle AI panel"},
    };
    static const GraptosShortcutRow search_rows[] = {
        {"Ctrl+F", "Open find"},
        {"Ctrl+H", "Open replace"},
        {"F3", "Find next"},
        {"Shift+F3", "Find previous"},
        {"Escape", "Close find/replace panel when visible"},
    };
    static const GraptosShortcutRow edit_rows[] = {
        {"Ctrl+G", "Go to line"},
        {"Ctrl+J", "Format code"},
        {"Ctrl+K", "Cut line"},
        {"Ctrl+U", "Paste cut line"},
        {"Ctrl+/", "Comment or uncomment line"},
        {"Tab", "Indent selection, accept completion, or insert indentation"},
        {"Shift+Tab", "Unindent selection or line"},
        {"Enter", "Insert newline with editor indentation handling"},
    };
    static const GraptosShortcutRow intelligence_rows[] = {
        {"Ctrl+Space", "Show completion"},
        {"Down / Up", "Move through visible completion items"},
        {"Tab", "Accept selected completion item"},
        {"Escape", "Hide visible completion"},
        {"Alt", "Show reference/color information at pointer or cursor"},
    };

    GtkWidget *dialog = gtk_window_new();
    gtk_widget_add_css_class(dialog, "graptos-window");
    gtk_widget_add_css_class(dialog, "graptos-about-dialog");
    gtk_window_set_title(GTK_WINDOW(dialog), GRAPTOS_DISPLAY_NAME " Shortcuts");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 620, 560);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    if (parent) gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(root, "graptos-root");
    gtk_widget_add_css_class(root, "graptos-dialog-root");
    gtk_widget_add_css_class(root, "graptos-flat-dialog");
    gtk_widget_add_css_class(root, "graptos-about-dialog-root");
    graptos_set_all_margins(root, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), root);

    GtkWidget *title = graptos_about_label_new("Keyboard Shortcuts", TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_set_halign(title, GTK_ALIGN_FILL);
    gtk_box_append(GTK_BOX(root), title);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_add_css_class(scroll, "graptos-shortcuts-scroll");
    gtk_box_append(GTK_BOX(root), scroll);

    GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    graptos_set_all_margins(content, 2);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), content);

    gtk_box_append(GTK_BOX(content),
                   graptos_shortcuts_section_new("File and tabs",
                                                 file_rows,
                                                 G_N_ELEMENTS(file_rows)));
    gtk_box_append(GTK_BOX(content),
                   graptos_shortcuts_section_new("View",
                                                 view_rows,
                                                 G_N_ELEMENTS(view_rows)));
    gtk_box_append(GTK_BOX(content),
                   graptos_shortcuts_section_new("Search",
                                                 search_rows,
                                                 G_N_ELEMENTS(search_rows)));
    gtk_box_append(GTK_BOX(content),
                   graptos_shortcuts_section_new("Editing",
                                                 edit_rows,
                                                 G_N_ELEMENTS(edit_rows)));
    gtk_box_append(GTK_BOX(content),
                   graptos_shortcuts_section_new("Completion and inspection",
                                                 intelligence_rows,
                                                 G_N_ELEMENTS(intelligence_rows)));

    (void)graptos_modal_window_run(GTK_WINDOW(dialog), GTK_RESPONSE_CLOSE);
    graptos_widget_destroy(dialog);
}


/**
 * @brief Theme editor response code for apply.
 */
#define GRAPTOS_THEME_RESPONSE_APPLY 1001
/**
 * @brief Theme editor response code for cancel.
 */
#define GRAPTOS_THEME_RESPONSE_CANCEL 1002
/**
 * @brief Theme editor response code for keep editing.
 */
#define GRAPTOS_THEME_RESPONSE_KEEP_EDITING 1003
/**
 * @brief Theme editor response code for reloading config.ini from disk.
 */
#define GRAPTOS_THEME_RESPONSE_RELOAD_CONFIG 1004

/**
 * @brief Bundled theme preset shown in the Theme dialog.
 * @details Presets are stored as normal Graptoς config fragments so loading a
 *          theme uses the same keys as manual `config.ini` editing.
 */
typedef struct {
    const char *label; /**< Human-readable preset name. */
    const char *filename; /**< Bundled theme file name under data/themes. */
} ThemePreset;

/**
 * @brief Bundled theme presets available without network access.
 */
static const ThemePreset GRAPTOS_THEME_PRESETS[] = {
    { "Dracula", "dracula.ini" },
    { "GitHub Dark", "github.ini" },
    { "Apprentice", "apprentice.ini" },
    { "Gruvbox Dark", "gruvbox.ini" },
    { "Tokyo Night", "tokyonight.ini" },
    { "Srcery", "srcery.ini" },
    { "Moonfly", "moonfly.ini" },
};

/**
 * @brief Editable theme state used by the preview dialog.
 * @details The theme dialog edits this copy instead of the window directly.
 *          That gives us live preview and still lets the user discard a session
 *          without leaving half-applied colors or fonts in the real config.
 */
typedef struct {
    char *editor_bg_color; /**< Editor background color. */
    char *editor_fg_color; /**< Editor foreground color. */
    char *editor_gutter_bg_color; /**< Editor gutter background color. */
    char *editor_gutter_fg_color; /**< Editor gutter foreground color. */
    char *editor_current_line_bg_color; /**< Editor current line background. */
    char *editor_selection_bg_color; /**< Editor selection background. */
    char *editor_selection_fg_color; /**< Editor selection foreground. */
    char *editor_cursor_color; /**< Editor cursor color. */
    char *sidebar_bg_color; /**< Sidebar background. */
    char *tabbar_bg_color; /**< Tabbar background. */
    char *tabbar_fg_color; /**< Tabbar foreground. */
    char *tab_active_bg_color; /**< Active tab background. */
    char *tab_active_fg_color; /**< Active tab foreground. */
    char *topbar_bg_color; /**< Topbar background. */
    char *topbar_fg_color; /**< Topbar foreground. */
    char *bottombar_bg_color; /**< Bottombar background. */
    char *bottombar_fg_color; /**< Bottombar foreground. */
    char *status_error_color; /**< Status error color. */
    char *button_bg_color; /**< Button background. */
    char *button_fg_color; /**< Button foreground. */
    char *button_hover_bg_color; /**< Button hover background. */
    char *button_active_bg_color; /**< Button active background. */
    char *input_bg_color; /**< Input background. */
    char *input_fg_color; /**< Input foreground. */
    char *input_border_color; /**< Input border. */
    char *project_tree_fg_color; /**< Project tree foreground. */
    char *project_tree_selected_bg_color; /**< Project tree selected background. */
    char *project_tree_selected_fg_color; /**< Project tree selected foreground. */
    char *git_status_modified_color; /**< Git modified indicator. */
    char *git_status_added_color; /**< Git added indicator. */
    char *git_status_deleted_color; /**< Git deleted indicator. */
    char *git_status_renamed_color; /**< Git renamed indicator. */
    char *git_status_conflict_color; /**< Git conflict indicator. */
    char *git_status_untracked_color; /**< Git untracked indicator. */
    char *git_status_staged_color; /**< Git staged indicator. */
    char *scroll_preview_bg_color; /**< Preview/minimap background. */
    char *scroll_preview_fg_color; /**< Preview/minimap foreground. */
    char *popover_bg_color; /**< Generic popover background. */
    char *popover_border_color; /**< Generic popover border. */
    char *tooltip_bg_color; /**< Tooltip background. */
    char *tooltip_fg_color; /**< Tooltip foreground. */
    char *tooltip_border_color; /**< Tooltip border. */
    char *ref_popover_bg_color; /**< Reference popover background. */
    char *ref_popover_fg_color; /**< Reference popover foreground. */
    char *ref_popover_heading_color; /**< Reference popover heading. */
    char *ref_popover_title_color; /**< Reference popover title. */
    char *ref_popover_kind_color; /**< Reference popover kind. */
    char *ref_popover_snippet_color; /**< Reference popover snippet. */
    char *ref_popover_hover_bg_color; /**< Reference popover hover background. */
    char *ref_popover_hover_fg_color; /**< Reference popover hover foreground. */
    char *completion_popover_bg_color; /**< Completion popover background. */
    char *completion_popover_fg_color; /**< Completion popover foreground. */
    char *completion_popover_detail_color; /**< Completion popover detail. */
    char *completion_selection_bg_color; /**< Completion selection background. */
    char *completion_selection_fg_color; /**< Completion selection foreground. */
    char *dialog_bg_color; /**< Dialog background. */
    char *dialog_fg_color; /**< Dialog foreground. */
    char *dialog_border_color; /**< Dialog border. */
    char *dialog_title_color; /**< Dialog title color. */
    char *dialog_body_color; /**< Dialog body text color. */
    char *dialog_muted_color; /**< Dialog muted text color. */
    char *dialog_output_color; /**< Dialog output text color. */
    char *git_output_bg_color; /**< Git command output background. */
    char *dialog_action_color; /**< Dialog action text color. */
    char *dialog_destructive_action_color; /**< Dialog destructive action text color. */
    char *dialog_input_fg_color; /**< Dialog input foreground. */
    char *dialog_input_bg_color; /**< Dialog input background. */
    char *search_match_bg_color; /**< Search match background. */
    char *search_match_fg_color; /**< Search match foreground. */
    char *diagnostic_warning_bg_color; /**< Diagnostic warning background. */
    char *diagnostic_warning_fg_color; /**< Diagnostic warning foreground. */
    char *codex_preview_bg_color; /**< Codex preview background. */
    char *codex_preview_fg_color; /**< Codex preview foreground. */
    char *codex_prompt_bg_color; /**< Codex prompt background. */
    char *ui_font; /**< UI font. */
    char *editor_font; /**< Editor font. */
    char *preview_font; /**< Preview font. */
    char *terminal_font; /**< Terminal font. */
    char *code_font; /**< Code/snippet font. */
} ThemeState;

/**
 * @brief Theme dialog state.
 * @details Dialog state carries both the scratch theme and the temporary CSS
 *          provider used for rendered previews. Keeping those together makes
 *          Apply, Save, Discard, and Reload operate on the same snapshot.
 */
typedef struct {
    EditorWindow *win; /**< Edited window. */
    ThemeState *state; /**< Preview state. */
    GtkCssProvider *preview_provider; /**< CSS provider for preview widgets. */
    GtkWidget *controls_root; /**< Theme controls that mirror the scratch state. */
    GtkWidget *preview_root; /**< Preview root widget. */
    gboolean dirty; /**< Whether preview differs from applied state. */
    gboolean loading; /**< Whether controls are being initialized. */
} ThemeDialogState;

/**
 * @brief Theme color control binding.
 */
typedef struct {
    ThemeDialogState *dialog; /**< Theme dialog state. */
    char **slot; /**< Theme color slot. */
    GtkWidget *entry; /**< Hex color entry. */
    GtkWidget *button; /**< Visual color selector button. */
    gboolean updating; /**< Whether the control is synchronizing widgets. */
} ThemeColorControl;

/**
 * @brief Theme font control binding.
 */
typedef struct {
    ThemeDialogState *dialog; /**< Theme dialog state. */
    char **slot; /**< Theme font slot. */
    GtkWidget *drop_down; /**< Font family dropdown. */
    GtkWidget *size_spin; /**< Font size spin button. */
    gboolean updating; /**< Whether the control is synchronizing widgets. */
} ThemeFontControl;

/**
 * @brief Duplicate a nullable string.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param value The value being parsed, stored, or applied.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *theme_strdup(const char *value) {
    return g_strdup(value ? value : "");
}

/**
 * @brief Replace a string slot.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param slot The slot supplied by the caller.
 * @param value The value being parsed, stored, or applied.
 */
static void theme_replace(char **slot, const char *value) {
    if (!slot) return;
    g_free(*slot);
    *slot = theme_strdup(value);
}

/**
 * @brief Copy all theme values from the window into a theme state.
 */
static ThemeState *theme_state_from_window(EditorWindow *win) {
    if (!win) return NULL;
    ThemeState *state = g_new0(ThemeState, 1);
    if (!state) return NULL;
/**
 * @brief Copy one string member from the editor window into ThemeState.
 */
#define COPY_FIELD(name) state->name = theme_strdup(win->name)
    COPY_FIELD(editor_bg_color);
    COPY_FIELD(editor_fg_color);
    COPY_FIELD(editor_gutter_bg_color);
    COPY_FIELD(editor_gutter_fg_color);
    COPY_FIELD(editor_current_line_bg_color);
    COPY_FIELD(editor_selection_bg_color);
    COPY_FIELD(editor_selection_fg_color);
    COPY_FIELD(editor_cursor_color);
    COPY_FIELD(sidebar_bg_color);
    COPY_FIELD(tabbar_bg_color);
    COPY_FIELD(tabbar_fg_color);
    COPY_FIELD(tab_active_bg_color);
    COPY_FIELD(tab_active_fg_color);
    COPY_FIELD(topbar_bg_color);
    COPY_FIELD(topbar_fg_color);
    COPY_FIELD(bottombar_bg_color);
    COPY_FIELD(bottombar_fg_color);
    COPY_FIELD(status_error_color);
    COPY_FIELD(button_bg_color);
    COPY_FIELD(button_fg_color);
    COPY_FIELD(button_hover_bg_color);
    COPY_FIELD(button_active_bg_color);
    COPY_FIELD(input_bg_color);
    COPY_FIELD(input_fg_color);
    COPY_FIELD(input_border_color);
    COPY_FIELD(project_tree_fg_color);
    COPY_FIELD(project_tree_selected_bg_color);
    COPY_FIELD(project_tree_selected_fg_color);
    COPY_FIELD(git_status_modified_color);
    COPY_FIELD(git_status_added_color);
    COPY_FIELD(git_status_deleted_color);
    COPY_FIELD(git_status_renamed_color);
    COPY_FIELD(git_status_conflict_color);
    COPY_FIELD(git_status_untracked_color);
    COPY_FIELD(git_status_staged_color);
    COPY_FIELD(scroll_preview_bg_color);
    COPY_FIELD(scroll_preview_fg_color);
    COPY_FIELD(popover_bg_color);
    COPY_FIELD(popover_border_color);
    COPY_FIELD(tooltip_bg_color);
    COPY_FIELD(tooltip_fg_color);
    COPY_FIELD(tooltip_border_color);
    COPY_FIELD(ref_popover_bg_color);
    COPY_FIELD(ref_popover_fg_color);
    COPY_FIELD(ref_popover_heading_color);
    COPY_FIELD(ref_popover_title_color);
    COPY_FIELD(ref_popover_kind_color);
    COPY_FIELD(ref_popover_snippet_color);
    COPY_FIELD(ref_popover_hover_bg_color);
    COPY_FIELD(ref_popover_hover_fg_color);
    COPY_FIELD(completion_popover_bg_color);
    COPY_FIELD(completion_popover_fg_color);
    COPY_FIELD(completion_popover_detail_color);
    COPY_FIELD(completion_selection_bg_color);
    COPY_FIELD(completion_selection_fg_color);
    COPY_FIELD(dialog_bg_color);
    COPY_FIELD(dialog_fg_color);
    COPY_FIELD(dialog_border_color);
    COPY_FIELD(dialog_title_color);
    COPY_FIELD(dialog_body_color);
    COPY_FIELD(dialog_muted_color);
    COPY_FIELD(dialog_output_color);
    COPY_FIELD(git_output_bg_color);
    COPY_FIELD(dialog_action_color);
    COPY_FIELD(dialog_destructive_action_color);
    COPY_FIELD(dialog_input_fg_color);
    COPY_FIELD(dialog_input_bg_color);
    COPY_FIELD(search_match_bg_color);
    COPY_FIELD(search_match_fg_color);
    COPY_FIELD(diagnostic_warning_bg_color);
    COPY_FIELD(diagnostic_warning_fg_color);
    COPY_FIELD(codex_preview_bg_color);
    COPY_FIELD(codex_preview_fg_color);
    COPY_FIELD(codex_prompt_bg_color);
    COPY_FIELD(ui_font);
    COPY_FIELD(editor_font);
    COPY_FIELD(preview_font);
    COPY_FIELD(terminal_font);
    COPY_FIELD(code_font);
#undef COPY_FIELD
    return state;
}

/**
 * @brief Free a theme state.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param state The state supplied by the caller.
 */
static void theme_state_free(ThemeState *state) {
    if (!state) return;
/**
 * @brief Free one owned string member from ThemeState.
 */
#define FREE_FIELD(name) g_free(state->name)
    FREE_FIELD(editor_bg_color);
    FREE_FIELD(editor_fg_color);
    FREE_FIELD(editor_gutter_bg_color);
    FREE_FIELD(editor_gutter_fg_color);
    FREE_FIELD(editor_current_line_bg_color);
    FREE_FIELD(editor_selection_bg_color);
    FREE_FIELD(editor_selection_fg_color);
    FREE_FIELD(editor_cursor_color);
    FREE_FIELD(sidebar_bg_color);
    FREE_FIELD(tabbar_bg_color);
    FREE_FIELD(tabbar_fg_color);
    FREE_FIELD(tab_active_bg_color);
    FREE_FIELD(tab_active_fg_color);
    FREE_FIELD(topbar_bg_color);
    FREE_FIELD(topbar_fg_color);
    FREE_FIELD(bottombar_bg_color);
    FREE_FIELD(bottombar_fg_color);
    FREE_FIELD(status_error_color);
    FREE_FIELD(button_bg_color);
    FREE_FIELD(button_fg_color);
    FREE_FIELD(button_hover_bg_color);
    FREE_FIELD(button_active_bg_color);
    FREE_FIELD(input_bg_color);
    FREE_FIELD(input_fg_color);
    FREE_FIELD(input_border_color);
    FREE_FIELD(project_tree_fg_color);
    FREE_FIELD(project_tree_selected_bg_color);
    FREE_FIELD(project_tree_selected_fg_color);
    FREE_FIELD(git_status_modified_color);
    FREE_FIELD(git_status_added_color);
    FREE_FIELD(git_status_deleted_color);
    FREE_FIELD(git_status_renamed_color);
    FREE_FIELD(git_status_conflict_color);
    FREE_FIELD(git_status_untracked_color);
    FREE_FIELD(git_status_staged_color);
    FREE_FIELD(scroll_preview_bg_color);
    FREE_FIELD(scroll_preview_fg_color);
    FREE_FIELD(popover_bg_color);
    FREE_FIELD(popover_border_color);
    FREE_FIELD(tooltip_bg_color);
    FREE_FIELD(tooltip_fg_color);
    FREE_FIELD(tooltip_border_color);
    FREE_FIELD(ref_popover_bg_color);
    FREE_FIELD(ref_popover_fg_color);
    FREE_FIELD(ref_popover_heading_color);
    FREE_FIELD(ref_popover_title_color);
    FREE_FIELD(ref_popover_kind_color);
    FREE_FIELD(ref_popover_snippet_color);
    FREE_FIELD(ref_popover_hover_bg_color);
    FREE_FIELD(ref_popover_hover_fg_color);
    FREE_FIELD(completion_popover_bg_color);
    FREE_FIELD(completion_popover_fg_color);
    FREE_FIELD(completion_popover_detail_color);
    FREE_FIELD(completion_selection_bg_color);
    FREE_FIELD(completion_selection_fg_color);
    FREE_FIELD(dialog_bg_color);
    FREE_FIELD(dialog_fg_color);
    FREE_FIELD(dialog_border_color);
    FREE_FIELD(dialog_title_color);
    FREE_FIELD(dialog_body_color);
    FREE_FIELD(dialog_muted_color);
    FREE_FIELD(dialog_output_color);
    FREE_FIELD(git_output_bg_color);
    FREE_FIELD(dialog_action_color);
    FREE_FIELD(dialog_destructive_action_color);
    FREE_FIELD(dialog_input_fg_color);
    FREE_FIELD(dialog_input_bg_color);
    FREE_FIELD(search_match_bg_color);
    FREE_FIELD(search_match_fg_color);
    FREE_FIELD(diagnostic_warning_bg_color);
    FREE_FIELD(diagnostic_warning_fg_color);
    FREE_FIELD(codex_preview_bg_color);
    FREE_FIELD(codex_preview_fg_color);
    FREE_FIELD(codex_prompt_bg_color);
    FREE_FIELD(ui_font);
    FREE_FIELD(editor_font);
    FREE_FIELD(preview_font);
    FREE_FIELD(terminal_font);
    FREE_FIELD(code_font);
#undef FREE_FIELD
    g_free(state);
}

/**
 * @brief Apply a theme state to the editor window.
 * @details The dialog edits a scratch ThemeState first. Applying is the moment
 *          we copy that scratch state into the live window, which lets preview
 *          changes be immediate while still supporting discard before save.
 * @param state The state supplied by the caller.
 * @param win The win supplied by the caller.
 */
static void theme_state_apply_to_window(ThemeState *state, EditorWindow *win) {
    if (!state || !win) return;
/**
 * @brief Replace one editor window string member from ThemeState.
 */
#define APPLY_FIELD(name) theme_replace(&win->name, state->name)
    APPLY_FIELD(editor_bg_color);
    APPLY_FIELD(editor_fg_color);
    APPLY_FIELD(editor_gutter_bg_color);
    APPLY_FIELD(editor_gutter_fg_color);
    APPLY_FIELD(editor_current_line_bg_color);
    APPLY_FIELD(editor_selection_bg_color);
    APPLY_FIELD(editor_selection_fg_color);
    APPLY_FIELD(editor_cursor_color);
    APPLY_FIELD(sidebar_bg_color);
    APPLY_FIELD(tabbar_bg_color);
    APPLY_FIELD(tabbar_fg_color);
    APPLY_FIELD(tab_active_bg_color);
    APPLY_FIELD(tab_active_fg_color);
    APPLY_FIELD(topbar_bg_color);
    APPLY_FIELD(topbar_fg_color);
    APPLY_FIELD(bottombar_bg_color);
    APPLY_FIELD(bottombar_fg_color);
    APPLY_FIELD(status_error_color);
    APPLY_FIELD(button_bg_color);
    APPLY_FIELD(button_fg_color);
    APPLY_FIELD(button_hover_bg_color);
    APPLY_FIELD(button_active_bg_color);
    APPLY_FIELD(input_bg_color);
    APPLY_FIELD(input_fg_color);
    APPLY_FIELD(input_border_color);
    APPLY_FIELD(project_tree_fg_color);
    APPLY_FIELD(project_tree_selected_bg_color);
    APPLY_FIELD(project_tree_selected_fg_color);
    APPLY_FIELD(git_status_modified_color);
    APPLY_FIELD(git_status_added_color);
    APPLY_FIELD(git_status_deleted_color);
    APPLY_FIELD(git_status_renamed_color);
    APPLY_FIELD(git_status_conflict_color);
    APPLY_FIELD(git_status_untracked_color);
    APPLY_FIELD(git_status_staged_color);
    APPLY_FIELD(scroll_preview_bg_color);
    APPLY_FIELD(scroll_preview_fg_color);
    APPLY_FIELD(popover_bg_color);
    APPLY_FIELD(popover_border_color);
    APPLY_FIELD(tooltip_bg_color);
    APPLY_FIELD(tooltip_fg_color);
    APPLY_FIELD(tooltip_border_color);
    APPLY_FIELD(ref_popover_bg_color);
    APPLY_FIELD(ref_popover_fg_color);
    APPLY_FIELD(ref_popover_heading_color);
    APPLY_FIELD(ref_popover_title_color);
    APPLY_FIELD(ref_popover_kind_color);
    APPLY_FIELD(ref_popover_snippet_color);
    APPLY_FIELD(ref_popover_hover_bg_color);
    APPLY_FIELD(ref_popover_hover_fg_color);
    APPLY_FIELD(completion_popover_bg_color);
    APPLY_FIELD(completion_popover_fg_color);
    APPLY_FIELD(completion_popover_detail_color);
    APPLY_FIELD(completion_selection_bg_color);
    APPLY_FIELD(completion_selection_fg_color);
    APPLY_FIELD(dialog_bg_color);
    APPLY_FIELD(dialog_fg_color);
    APPLY_FIELD(dialog_border_color);
    APPLY_FIELD(dialog_title_color);
    APPLY_FIELD(dialog_body_color);
    APPLY_FIELD(dialog_muted_color);
    APPLY_FIELD(dialog_output_color);
    APPLY_FIELD(git_output_bg_color);
    APPLY_FIELD(dialog_action_color);
    APPLY_FIELD(dialog_destructive_action_color);
    APPLY_FIELD(dialog_input_fg_color);
    APPLY_FIELD(dialog_input_bg_color);
    APPLY_FIELD(search_match_bg_color);
    APPLY_FIELD(search_match_fg_color);
    APPLY_FIELD(diagnostic_warning_bg_color);
    APPLY_FIELD(diagnostic_warning_fg_color);
    APPLY_FIELD(codex_preview_bg_color);
    APPLY_FIELD(codex_preview_fg_color);
    APPLY_FIELD(codex_prompt_bg_color);
    APPLY_FIELD(ui_font);
    APPLY_FIELD(editor_font);
    APPLY_FIELD(preview_font);
    APPLY_FIELD(terminal_font);
    APPLY_FIELD(code_font);
#undef APPLY_FIELD
}

/**
 * @brief Load one color key from an INI theme into the scratch state.
 * @details Theme files intentionally use the same `[Editor]` keys as
 *          `config.ini`. Invalid or absent colors are ignored so a custom theme
 *          can override only the roles it cares about.
 * @param key_file The parsed theme file.
 * @param key The config key to read.
 * @param slot The scratch theme color slot to replace.
 */
static void theme_state_load_color_key(GKeyFile *key_file,
                                       const char *key,
                                       char **slot) {
    if (!key_file || !key || !slot) return;
    g_autofree char *value = g_key_file_get_string(key_file, "Editor", key, NULL);
    GdkRGBA rgba;
    if (!value || !gdk_rgba_parse(&rgba, value)) return;
    theme_replace(slot, value);
}

/**
 * @brief Apply a parsed theme file to a ThemeState.
 * @details This maps the complete current Graptoς theme surface. Using one
 *          explicit list is verbose, but it makes missing or duplicate config
 *          keys easy to catch during review.
 * @param state The scratch theme state being edited.
 * @param key_file Parsed INI data with an `[Editor]` group.
 */
static void theme_state_load_key_file(ThemeState *state, GKeyFile *key_file) {
    if (!state || !key_file) return;
#define LOAD_THEME_COLOR(key, field) \
    theme_state_load_color_key(key_file, key, &state->field)
    LOAD_THEME_COLOR("background_color", editor_bg_color);
    LOAD_THEME_COLOR("foreground_color", editor_fg_color);
    LOAD_THEME_COLOR("editor_gutter_background_color", editor_gutter_bg_color);
    LOAD_THEME_COLOR("editor_gutter_foreground_color", editor_gutter_fg_color);
    LOAD_THEME_COLOR("editor_current_line_background_color", editor_current_line_bg_color);
    LOAD_THEME_COLOR("editor_selection_background_color", editor_selection_bg_color);
    LOAD_THEME_COLOR("editor_selection_foreground_color", editor_selection_fg_color);
    LOAD_THEME_COLOR("editor_cursor_color", editor_cursor_color);
    LOAD_THEME_COLOR("sidebar_background_color", sidebar_bg_color);
    LOAD_THEME_COLOR("tabbar_background_color", tabbar_bg_color);
    LOAD_THEME_COLOR("tabbar_foreground_color", tabbar_fg_color);
    LOAD_THEME_COLOR("tab_active_background_color", tab_active_bg_color);
    LOAD_THEME_COLOR("tab_active_foreground_color", tab_active_fg_color);
    LOAD_THEME_COLOR("topbar_background_color", topbar_bg_color);
    LOAD_THEME_COLOR("topbar_foreground_color", topbar_fg_color);
    LOAD_THEME_COLOR("bottombar_background_color", bottombar_bg_color);
    LOAD_THEME_COLOR("bottombar_foreground_color", bottombar_fg_color);
    LOAD_THEME_COLOR("status_error_color", status_error_color);
    LOAD_THEME_COLOR("button_background_color", button_bg_color);
    LOAD_THEME_COLOR("button_foreground_color", button_fg_color);
    LOAD_THEME_COLOR("button_hover_background_color", button_hover_bg_color);
    LOAD_THEME_COLOR("button_active_background_color", button_active_bg_color);
    LOAD_THEME_COLOR("input_background_color", input_bg_color);
    LOAD_THEME_COLOR("input_foreground_color", input_fg_color);
    LOAD_THEME_COLOR("input_border_color", input_border_color);
    LOAD_THEME_COLOR("project_tree_foreground_color", project_tree_fg_color);
    LOAD_THEME_COLOR("project_tree_selected_background_color", project_tree_selected_bg_color);
    LOAD_THEME_COLOR("project_tree_selected_foreground_color", project_tree_selected_fg_color);
    LOAD_THEME_COLOR("git_status_modified_color", git_status_modified_color);
    LOAD_THEME_COLOR("git_status_added_color", git_status_added_color);
    LOAD_THEME_COLOR("git_status_deleted_color", git_status_deleted_color);
    LOAD_THEME_COLOR("git_status_renamed_color", git_status_renamed_color);
    LOAD_THEME_COLOR("git_status_conflict_color", git_status_conflict_color);
    LOAD_THEME_COLOR("git_status_untracked_color", git_status_untracked_color);
    LOAD_THEME_COLOR("git_status_staged_color", git_status_staged_color);
    LOAD_THEME_COLOR("scroll_preview_background_color", scroll_preview_bg_color);
    LOAD_THEME_COLOR("scroll_preview_foreground_color", scroll_preview_fg_color);
    LOAD_THEME_COLOR("popover_background_color", popover_bg_color);
    LOAD_THEME_COLOR("popover_border_color", popover_border_color);
    LOAD_THEME_COLOR("tooltip_background_color", tooltip_bg_color);
    LOAD_THEME_COLOR("tooltip_foreground_color", tooltip_fg_color);
    LOAD_THEME_COLOR("tooltip_border_color", tooltip_border_color);
    LOAD_THEME_COLOR("ref_popover_background_color", ref_popover_bg_color);
    LOAD_THEME_COLOR("ref_popover_foreground_color", ref_popover_fg_color);
    LOAD_THEME_COLOR("ref_popover_heading_color", ref_popover_heading_color);
    LOAD_THEME_COLOR("ref_popover_title_color", ref_popover_title_color);
    LOAD_THEME_COLOR("ref_popover_kind_color", ref_popover_kind_color);
    LOAD_THEME_COLOR("ref_popover_snippet_color", ref_popover_snippet_color);
    LOAD_THEME_COLOR("ref_popover_hover_background_color", ref_popover_hover_bg_color);
    LOAD_THEME_COLOR("ref_popover_hover_foreground_color", ref_popover_hover_fg_color);
    LOAD_THEME_COLOR("autocomplete_popover_background_color", completion_popover_bg_color);
    LOAD_THEME_COLOR("autocomplete_popover_foreground_color", completion_popover_fg_color);
    LOAD_THEME_COLOR("autocomplete_popover_detail_color", completion_popover_detail_color);
    LOAD_THEME_COLOR("autocomplete_selection_background_color", completion_selection_bg_color);
    LOAD_THEME_COLOR("autocomplete_selection_foreground_color", completion_selection_fg_color);
    LOAD_THEME_COLOR("dialog_background_color", dialog_bg_color);
    LOAD_THEME_COLOR("dialog_foreground_color", dialog_fg_color);
    LOAD_THEME_COLOR("dialog_border_color", dialog_border_color);
    LOAD_THEME_COLOR("dialog_title_color", dialog_title_color);
    LOAD_THEME_COLOR("dialog_body_color", dialog_body_color);
    LOAD_THEME_COLOR("dialog_muted_color", dialog_muted_color);
    LOAD_THEME_COLOR("dialog_output_color", dialog_output_color);
    LOAD_THEME_COLOR("git_output_background_color", git_output_bg_color);
    LOAD_THEME_COLOR("dialog_action_color", dialog_action_color);
    LOAD_THEME_COLOR("dialog_destructive_action_color", dialog_destructive_action_color);
    LOAD_THEME_COLOR("dialog_input_foreground_color", dialog_input_fg_color);
    LOAD_THEME_COLOR("dialog_input_background_color", dialog_input_bg_color);
    LOAD_THEME_COLOR("search_match_background_color", search_match_bg_color);
    LOAD_THEME_COLOR("search_match_foreground_color", search_match_fg_color);
    LOAD_THEME_COLOR("diagnostic_warning_background_color", diagnostic_warning_bg_color);
    LOAD_THEME_COLOR("diagnostic_warning_foreground_color", diagnostic_warning_fg_color);
    LOAD_THEME_COLOR("codex_preview_background_color", codex_preview_bg_color);
    LOAD_THEME_COLOR("codex_preview_foreground_color", codex_preview_fg_color);
    LOAD_THEME_COLOR("codex_prompt_background_color", codex_prompt_bg_color);
#undef LOAD_THEME_COLOR
}

/**
 * @brief Return a valid color or fallback.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param value The value being parsed, stored, or applied.
 * @param fallback The fallback supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *theme_color(const char *value, const char *fallback) {
    return (value && value[0] == '#') ? value : fallback;
}

/**
 * @brief Append a preview font rule.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param css The css supplied by the caller.
 * @param selector The selector supplied by the caller.
 * @param font_desc The font desc supplied by the caller.
 */
static void theme_preview_append_font(GString *css,
                                      const char *selector,
                                      const char *font_desc) {
    if (!css || !selector || !font_desc || font_desc[0] == '\0') return;
    PangoFontDescription *desc =
        pango_font_description_from_string(font_desc);
    if (!desc) return;
    const char *family = pango_font_description_get_family(desc);
    if (family && family[0] != '\0') {
        g_autofree char *escaped = g_strescape(family, NULL);
        g_string_append_printf(css, "%s { font-family: \"%s\"; }\n",
                               selector, escaped ? escaped : family);
    }
    pango_font_description_free(desc);
}

/**
 * @brief Update the rendered theme preview CSS.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param dialog The dialog supplied by the caller.
 */
static void theme_preview_update(ThemeDialogState *dialog) {
    if (!dialog || !dialog->state || !dialog->preview_root) return;
    ThemeState *s = dialog->state;
    GdkDisplay *display = gdk_display_get_default();
    if (!display) return;

    if (dialog->preview_provider) {
        gtk_style_context_remove_provider_for_display(
            display, GTK_STYLE_PROVIDER(dialog->preview_provider));
        g_clear_object(&dialog->preview_provider);
    }

    GString *css = g_string_new(NULL);
    if (!css) return;

    const char *bg = theme_color(s->editor_bg_color, "#181a1f");
    const char *fg = theme_color(s->editor_fg_color, "#d4d4d4");
    const char *popover = theme_color(s->popover_bg_color, bg);
    const char *popover_border = theme_color(s->popover_border_color, "#00000000");
    const char *tooltip = theme_color(s->tooltip_bg_color, popover);
    const char *tooltip_fg = theme_color(s->tooltip_fg_color, fg);
    const char *tooltip_border = theme_color(s->tooltip_border_color, popover_border);
    g_string_append_printf(css,
        ".graptos-theme-preview-root { background: %s; color: %s; }\n"
        ".graptos-theme-preview-top { background: %s; color: %s; padding: 6px; }\n"
        ".graptos-theme-preview-bottom { background: %s; color: %s; padding: 6px; }\n"
        ".graptos-theme-preview-editor { background: %s; color: %s; padding: 8px; }\n"
        ".graptos-theme-preview-gutter { background: %s; color: %s; padding: 8px; }\n"
        ".graptos-theme-preview-current { background: %s; padding: 2px; }\n"
        ".graptos-theme-preview-selection { background: %s; color: %s; padding: 2px; }\n"
        ".graptos-theme-preview-project { background: %s; color: %s; padding: 8px; }\n"
        ".graptos-theme-preview-project-selected { background: %s; color: %s; padding: 2px; }\n"
        ".graptos-theme-preview-popover { background: %s; color: %s; padding: 8px; border: 1px solid %s; }\n"
        ".graptos-theme-preview-tooltip { background: %s; color: %s; padding: 6px; border: 1px solid %s; }\n"
        ".graptos-theme-preview-completion { background: %s; color: %s; padding: 8px; }\n"
        ".graptos-theme-preview-completion-detail { color: %s; }\n"
        ".graptos-theme-preview-completion-selected { background: %s; color: %s; padding: 2px; }\n"
        ".graptos-theme-preview-ref { background: %s; color: %s; padding: 8px; }\n"
        ".graptos-theme-preview-ref-heading { color: %s; }\n"
        ".graptos-theme-preview-ref-title { color: %s; }\n"
        ".graptos-theme-preview-ref-kind { color: %s; }\n"
        ".graptos-theme-preview-ref-snippet { color: %s; }\n"
        ".graptos-theme-preview-ref-hover { background: %s; color: %s; padding: 2px; }\n"
        ".graptos-theme-preview-dialog { background: %s; color: %s; border: 1px solid %s; padding: 8px; }\n"
        ".graptos-theme-preview-dialog-title { color: %s; font-weight: 700; }\n"
        ".graptos-theme-preview-dialog-body { color: %s; }\n"
        ".graptos-theme-preview-dialog-muted { color: %s; }\n"
        ".graptos-theme-preview-dialog-output { background: %s; color: %s; padding: 4px; }\n"
        ".graptos-theme-preview-dialog-action { color: %s; }\n"
        ".graptos-theme-preview-dialog-destructive { color: %s; }\n"
        ".graptos-theme-preview-dialog-input { background: %s; color: %s; padding: 3px; }\n"
        ".graptos-theme-preview-codex { background: %s; color: %s; padding: 8px; }\n"
        ".graptos-theme-preview-codex-prompt { background: %s; padding: 4px; }\n"
        ".graptos-theme-preview-search { background: %s; color: %s; padding: 2px; }\n"
        ".graptos-theme-preview-warning { background: %s; color: %s; padding: 2px; }\n",
        bg, fg,
        theme_color(s->topbar_bg_color, bg), theme_color(s->topbar_fg_color, fg),
        theme_color(s->bottombar_bg_color, bg), theme_color(s->bottombar_fg_color, fg),
        bg, fg,
        theme_color(s->editor_gutter_bg_color, bg), theme_color(s->editor_gutter_fg_color, "#8b949e"),
        theme_color(s->editor_current_line_bg_color, "#20232b"),
        theme_color(s->editor_selection_bg_color, "#3a405c"), theme_color(s->editor_selection_fg_color, "#ffffff"),
        theme_color(s->sidebar_bg_color, bg), theme_color(s->project_tree_fg_color, fg),
        theme_color(s->project_tree_selected_bg_color, "#2a2e3d"), theme_color(s->project_tree_selected_fg_color, "#ffffff"),
        popover, fg, popover_border,
        tooltip, tooltip_fg, tooltip_border,
        theme_color(s->completion_popover_bg_color, popover), theme_color(s->completion_popover_fg_color, fg),
        theme_color(s->completion_popover_detail_color, "#a6adc8"),
        theme_color(s->completion_selection_bg_color, "#89b4fa"), theme_color(s->completion_selection_fg_color, "#11111b"),
        theme_color(s->ref_popover_bg_color, popover), theme_color(s->ref_popover_fg_color, fg),
        theme_color(s->ref_popover_heading_color, "#cba6f7"),
        theme_color(s->ref_popover_title_color, "#89dceb"),
        theme_color(s->ref_popover_kind_color, "#a6adc8"),
        theme_color(s->ref_popover_snippet_color, fg),
        theme_color(s->ref_popover_hover_bg_color, "#2a2e3d"), theme_color(s->ref_popover_hover_fg_color, "#ffffff"),
        theme_color(s->dialog_bg_color, "#1b1f24"), theme_color(s->dialog_fg_color, fg),
        theme_color(s->dialog_border_color, "#00000000"), theme_color(s->dialog_title_color, "#ffffff"),
        theme_color(s->dialog_body_color, fg),
        theme_color(s->dialog_muted_color, "#a6adc8"),
        theme_color(s->git_output_bg_color, "#1b1f24"),
        theme_color(s->dialog_output_color, fg),
        theme_color(s->dialog_action_color, fg),
        theme_color(s->dialog_destructive_action_color, "#ff6b6b"),
        theme_color(s->dialog_input_bg_color, "#181a1f"),
        theme_color(s->dialog_input_fg_color, fg),
        theme_color(s->codex_preview_bg_color, bg), theme_color(s->codex_preview_fg_color, fg),
        theme_color(s->codex_prompt_bg_color, bg),
        theme_color(s->search_match_bg_color, "#515c7a"), theme_color(s->search_match_fg_color, "#ffffff"),
        theme_color(s->diagnostic_warning_bg_color, "#5f4b24"), theme_color(s->diagnostic_warning_fg_color, "#ffd166"));
    g_string_append_printf(css,
        ".graptos-theme-preview-git-modified { color: %s; font-weight: 800; }\n"
        ".graptos-theme-preview-git-added { color: %s; font-weight: 800; }\n"
        ".graptos-theme-preview-git-deleted { color: %s; font-weight: 800; }\n"
        ".graptos-theme-preview-git-renamed { color: %s; font-weight: 800; }\n"
        ".graptos-theme-preview-git-conflict { color: %s; font-weight: 800; }\n"
        ".graptos-theme-preview-git-untracked { color: %s; font-weight: 800; }\n"
        ".graptos-theme-preview-git-staged { color: %s; font-weight: 800; }\n",
        theme_color(s->git_status_modified_color, "#f9c74f"),
        theme_color(s->git_status_added_color, "#57cc99"),
        theme_color(s->git_status_deleted_color, "#ff6b6b"),
        theme_color(s->git_status_renamed_color, "#4cc9f0"),
        theme_color(s->git_status_conflict_color, "#ff4d6d"),
        theme_color(s->git_status_untracked_color, "#77bdfb"),
        theme_color(s->git_status_staged_color, "#cba6f7"));
    theme_preview_append_font(css, ".graptos-theme-preview-root", s->ui_font);
    theme_preview_append_font(css, ".graptos-theme-preview-editor", s->editor_font);
    theme_preview_append_font(css, ".graptos-theme-preview-terminal", s->terminal_font);
    theme_preview_append_font(css, ".graptos-theme-preview-code", s->code_font);
    theme_preview_append_font(css, ".graptos-theme-preview-rendered", s->preview_font);

    dialog->preview_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(dialog->preview_provider, css->str);
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(dialog->preview_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 20u);
    g_string_free(css, TRUE);
}

/**
 * @brief Mark the theme dialog dirty and refresh preview.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param dialog The dialog supplied by the caller.
 */
static void theme_dialog_changed(ThemeDialogState *dialog) {
    if (!dialog || dialog->loading) return;
    dialog->dirty = TRUE;
    theme_preview_update(dialog);
}

/**
 * @brief Convert RGBA to config color text.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param rgba The rgba supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *theme_rgba_to_string(const GdkRGBA *rgba) {
    if (!rgba) return g_strdup("#000000");
    return g_strdup_printf("#%02x%02x%02x",
                           (unsigned int)CLAMP((int)(rgba->red * 255.0 + 0.5), 0, 255),
                           (unsigned int)CLAMP((int)(rgba->green * 255.0 + 0.5), 0, 255),
                           (unsigned int)CLAMP((int)(rgba->blue * 255.0 + 0.5), 0, 255));
}

/**
 * @brief Compare Pango font families case-insensitively for font sorting.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param a The first Pango font family supplied by the model sorter.
 * @param b The second Pango font family supplied by the model sorter.
 * @return The computed value requested by the caller.
 */
static int theme_font_family_compare(const void *a, const void *b) {
    PangoFontFamily * const *left = a;
    PangoFontFamily * const *right = b;
    const char *left_name = left && *left ? pango_font_family_get_name(*left) : "";
    const char *right_name = right && *right ? pango_font_family_get_name(*right) : "";
    return g_ascii_strcasecmp(left_name ? left_name : "",
                              right_name ? right_name : "");
}

/**
 * @brief Return the font family portion of a Pango font description.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param font_desc The font desc supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *theme_font_family_from_desc(const char *font_desc) {
    if (!font_desc || font_desc[0] == '\0') return g_strdup("");

    PangoFontDescription *desc =
        pango_font_description_from_string(font_desc);
    if (!desc) return g_strdup(font_desc);

    const char *family = pango_font_description_get_family(desc);
    char *result = g_strdup(family && family[0] != '\0' ? family : font_desc);
    pango_font_description_free(desc);
    return result;
}

/**
 * @brief Return the point size portion of a Pango font description.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param font_desc The font desc supplied by the caller.
 * @return The computed value requested by the caller.
 */
static double theme_font_size_from_desc(const char *font_desc) {
    if (!font_desc || font_desc[0] == '\0') return 11.0;

    PangoFontDescription *desc =
        pango_font_description_from_string(font_desc);
    if (!desc) return 11.0;

    int size = pango_font_description_get_size(desc);
    double result = size > 0 ? (double)size / (double)PANGO_SCALE : 11.0;
    pango_font_description_free(desc);
    return CLAMP(result, 6.0, 96.0);
}

/**
 * @brief Find a string inside a GTK string list.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param list The list supplied by the caller.
 * @param value The value being parsed, stored, or applied.
 * @param position_out Output storage filled when the operation can provide a value.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean theme_string_list_find(GtkStringList *list,
                                       const char *value,
                                       guint *position_out) {
    if (!list || !value) return FALSE;
    guint count = g_list_model_get_n_items(G_LIST_MODEL(list));
    for (guint i = 0; i < count; i++) {
        const char *item = gtk_string_list_get_string(list, i);
        if (item && g_strcmp0(item, value) == 0) {
            if (position_out) *position_out = i;
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief Build a lightweight font-family model for theme font controls.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param font_desc The font desc supplied by the caller.
 * @param selected_out Output storage filled when the operation can provide a value.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkStringList *theme_font_model_new(const char *font_desc,
                                           guint *selected_out) {
    GtkStringList *list = gtk_string_list_new(NULL);
    guint selected = 0u;
    gtk_string_list_append(list, "System default");
    gtk_string_list_append(list, "Inconsolata");

    g_autofree char *current_family = theme_font_family_from_desc(font_desc);
    if (current_family && g_strcmp0(current_family, "Inconsolata") == 0) {
        selected = 1u;
    }

    PangoFontMap *font_map = pango_cairo_font_map_get_default();
    PangoFontFamily **families = NULL;
    int family_count = 0;
    if (font_map) {
        pango_font_map_list_families(font_map, &families, &family_count);
    }

    if (families && family_count > 0) {
        qsort(families,
              (size_t)family_count,
              sizeof(PangoFontFamily *),
              theme_font_family_compare);
        const char *last_name = NULL;
        for (int i = 0; i < family_count; i++) {
            const char *name = pango_font_family_get_name(families[i]);
            if (!name || name[0] == '\0' ||
                g_strcmp0(name, "Inconsolata") == 0 ||
                g_strcmp0(name, last_name) == 0) {
                continue;
            }
            gtk_string_list_append(list, name);
            if (current_family && g_strcmp0(current_family, name) == 0) {
                selected = g_list_model_get_n_items(G_LIST_MODEL(list)) - 1u;
            }
            last_name = name;
        }
    }
    g_free(families);

    if (current_family && current_family[0] != '\0' &&
        !theme_string_list_find(list, current_family, &selected)) {
        gtk_string_list_append(list, current_family);
        selected = g_list_model_get_n_items(G_LIST_MODEL(list)) - 1u;
    }

    if (selected_out) *selected_out = selected;
    return list;
}

/**
 * @brief Set color control widgets from a parsed color.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param control The control supplied by the caller.
 * @param rgba The rgba supplied by the caller.
 */
static void theme_color_control_set_rgba(ThemeColorControl *control,
                                         const GdkRGBA *rgba) {
    if (!control || !rgba) return;
    control->updating = TRUE;
    if (control->entry) {
        g_autofree char *text = theme_rgba_to_string(rgba);
        gtk_editable_set_text(GTK_EDITABLE(control->entry), text);
    }
    if (control->button) {
        gtk_color_dialog_button_set_rgba(
            GTK_COLOR_DIALOG_BUTTON(control->button), rgba);
    }
    control->updating = FALSE;
}

/**
 * @brief Store a parsed color and refresh live preview.
 * @details Text entries and color buttons both land here after validation. We
 *          normalize the color once so the config, preview, and later saves do
 *          not preserve a mix of user-entered spellings for the same color.
 * @param control The control supplied by the caller.
 * @param rgba The rgba supplied by the caller.
 */
static void theme_color_control_store(ThemeColorControl *control,
                                      const GdkRGBA *rgba) {
    if (!control || !control->slot || !rgba) return;
    char *normalized = theme_rgba_to_string(rgba);
    if (g_strcmp0(*control->slot, normalized) == 0) {
        g_free(normalized);
        return;
    }

    theme_replace(control->slot, normalized);
    g_free(normalized);
    theme_dialog_changed(control->dialog);
}

/**
 * @brief Handle a hex color entry change.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param editable The editable supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void theme_color_entry_changed(GtkEditable *editable,
                                      gpointer user_data) {
    ThemeColorControl *control = user_data;
    if (!control || !control->slot || !editable) return;
    if (control->updating) return;

    const char *text = gtk_editable_get_text(editable);
    GdkRGBA rgba;
    if (!text || !gdk_rgba_parse(&rgba, text)) return;

    theme_color_control_store(control, &rgba);
    theme_color_control_set_rgba(control, &rgba);
}

/**
 * @brief Handle a visual color selector change.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param button The button that emitted the action signal.
 * @param pspec The pspec supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void theme_color_button_changed(GtkColorDialogButton *button,
                                       GParamSpec *pspec,
                                       gpointer user_data) {
    (void)pspec;
    ThemeColorControl *control = user_data;
    if (!control || control->updating || !button) return;

    const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba(button);
    if (!rgba) return;
    theme_color_control_store(control, rgba);
    theme_color_control_set_rgba(control, rgba);
}

/**
 * @brief Write the selected font family and size into the config slot.
 * @details GTK gives us family and size as separate controls, but the rest of
 *          Graptoς uses a normal Pango font description. This is the conversion
 *          point, including the empty string that means inherit the system font.
 * @param control The control supplied by the caller.
 */
static void theme_font_control_store(ThemeFontControl *control) {
    if (!control || !control->slot || !control->drop_down) return;
    if (control->updating) return;

    GListModel *model =
        gtk_drop_down_get_model(GTK_DROP_DOWN(control->drop_down));
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(control->drop_down));
    if (!GTK_IS_STRING_LIST(model) || selected == GTK_INVALID_LIST_POSITION) {
        return;
    }

    const char *text = gtk_string_list_get_string(GTK_STRING_LIST(model),
                                                  selected);
    if (!text) return;
    if (g_strcmp0(text, "System default") == 0) {
        theme_replace(control->slot, "");
        theme_dialog_changed(control->dialog);
        return;
    }

    double size = control->size_spin
        ? gtk_spin_button_get_value(GTK_SPIN_BUTTON(control->size_spin))
        : 11.0;
    g_autofree char *font_desc = g_strdup_printf("%s %.0f", text, size);
    theme_replace(control->slot, font_desc);
    theme_dialog_changed(control->dialog);
}

/**
 * @brief Handle a font dropdown selection change.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param drop_down The drop down supplied by the caller.
 * @param pspec The pspec supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void theme_font_changed(GtkDropDown *drop_down,
                               GParamSpec *pspec,
                               gpointer user_data) {
    (void)drop_down;
    (void)pspec;
    theme_font_control_store(user_data);
}

/**
 * @brief Handle a font size spin change.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param spin_button The spin button supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void theme_font_size_changed(GtkSpinButton *spin_button,
                                    gpointer user_data) {
    (void)spin_button;
    theme_font_control_store(user_data);
}

/**
 * @brief Select a font family and size in the font controls.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param control The control supplied by the caller.
 * @param font_desc The font desc supplied by the caller.
 */
static void theme_font_control_set(ThemeFontControl *control,
                                   const char *font_desc) {
    if (!control || !control->drop_down || !control->slot) return;
    control->updating = TRUE;
    theme_replace(control->slot, font_desc ? font_desc : "");

    g_autofree char *family = theme_font_family_from_desc(font_desc);
    const char *selection = family && family[0] != '\0'
        ? family
        : "System default";

    GListModel *model =
        gtk_drop_down_get_model(GTK_DROP_DOWN(control->drop_down));
    guint position = 0u;
    if (GTK_IS_STRING_LIST(model) &&
        theme_string_list_find(GTK_STRING_LIST(model), selection, &position)) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(control->drop_down), position);
    }
    if (control->size_spin) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(control->size_spin),
                                  theme_font_size_from_desc(font_desc));
    }
    control->updating = FALSE;
    theme_dialog_changed(control->dialog);
}

/**
 * @brief Synchronize one color control after a theme file changed its slot.
 * @details Presets update the scratch state directly. The visible entry and
 *          color button need to follow that state without marking another
 *          artificial edit.
 * @param control The color control bound to a ThemeState slot.
 */
static void theme_color_control_sync(ThemeColorControl *control) {
    if (!control || !control->slot) return;
    GdkRGBA rgba;
    if (*control->slot && gdk_rgba_parse(&rgba, *control->slot)) {
        theme_color_control_set_rgba(control, &rgba);
    } else if (control->entry) {
        control->updating = TRUE;
        gtk_editable_set_text(GTK_EDITABLE(control->entry), "");
        control->updating = FALSE;
    }
}

/**
 * @brief Synchronize one font control after a theme load.
 * @details Theme presets currently target colors, but custom INI files may
 *          eventually include font keys. This helper keeps the control updater
 *          complete without changing dirty state.
 * @param control The font control bound to a ThemeState slot.
 */
static void theme_font_control_sync(ThemeFontControl *control) {
    if (!control || !control->slot || !control->drop_down) return;
    control->updating = TRUE;
    g_autofree char *family = theme_font_family_from_desc(*control->slot);
    const char *selection = family && family[0] != '\0'
        ? family
        : "System default";
    GListModel *model =
        gtk_drop_down_get_model(GTK_DROP_DOWN(control->drop_down));
    guint position = 0u;
    if (GTK_IS_STRING_LIST(model) &&
        theme_string_list_find(GTK_STRING_LIST(model), selection, &position)) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(control->drop_down), position);
    }
    if (control->size_spin) {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(control->size_spin),
                                  theme_font_size_from_desc(*control->slot));
    }
    control->updating = FALSE;
}

/**
 * @brief Recursively synchronize theme controls with the scratch state.
 * @details Loading a preset must update both preview and the visible fields.
 *          Traversing widgets avoids keeping a second registry of controls that
 *          could drift from the actual dialog contents.
 * @param widget The root widget to inspect.
 */
static void theme_controls_sync_recursive(GtkWidget *widget) {
    if (!widget) return;
    ThemeColorControl *color_control =
        g_object_get_data(G_OBJECT(widget), "graptos-theme-color-control");
    if (color_control) theme_color_control_sync(color_control);
    ThemeFontControl *font_control =
        g_object_get_data(G_OBJECT(widget), "graptos-theme-font-control");
    if (font_control) theme_font_control_sync(font_control);

    for (GtkWidget *child = gtk_widget_get_first_child(widget);
         child;
         child = gtk_widget_get_next_sibling(child)) {
        theme_controls_sync_recursive(child);
    }
}

/**
 * @brief Synchronize all visible theme controls.
 * @details This is called after presets and custom files mutate ThemeState.
 * @param dialog The theme dialog state owning the control tree.
 */
static void theme_controls_sync(ThemeDialogState *dialog) {
    if (!dialog || !dialog->controls_root) return;
    dialog->loading = TRUE;
    theme_controls_sync_recursive(dialog->controls_root);
    dialog->loading = FALSE;
}

/**
 * @brief Return the development-tree path for a bundled theme file.
 * @details Running from the build tree should see the same presets as an
 *          installed application, so the loader tries data/themes before the
 *          installed DATADIR copy.
 * @param filename The bundled theme file name.
 * @return An owned path.
 */
static char *theme_dev_path(const char *filename) {
    return g_build_filename("data", "themes", filename, NULL);
}

/**
 * @brief Return the installed path for a bundled theme file.
 * @details Installed builds load presets from DATADIR, matching syntax and
 *          project-template discovery.
 * @param filename The bundled theme file name.
 * @return An owned path.
 */
static char *theme_installed_path(const char *filename) {
    return g_build_filename(DATADIR, "themes", filename, NULL);
}

/**
 * @brief Resolve a bundled theme preset to a readable path.
 * @details Development files win over installed files so local theme edits can
 *          be tested without running `make install`.
 * @param preset The preset metadata selected by the user.
 * @return An owned path, or NULL when the preset cannot be found.
 */
static char *theme_preset_path(const ThemePreset *preset) {
    if (!preset || !preset->filename) return NULL;
    g_autofree char *dev = theme_dev_path(preset->filename);
    if (g_file_test(dev, G_FILE_TEST_IS_REGULAR)) return g_strdup(dev);
    g_autofree char *installed = theme_installed_path(preset->filename);
    if (g_file_test(installed, G_FILE_TEST_IS_REGULAR)) return g_strdup(installed);
    return NULL;
}

/**
 * @brief Load a theme INI file into the dialog scratch state.
 * @details Loading changes only the dialog preview state. The real application
 *          config is updated later when the user presses Apply.
 * @param dialog The theme dialog state.
 * @param path The theme INI path.
 * @param label Name used in status/error messages.
 * @return TRUE when the theme loaded.
 */
static gboolean theme_dialog_load_file(ThemeDialogState *dialog,
                                       const char *path,
                                       const char *label) {
    if (!dialog || !dialog->state || !path) return FALSE;
    g_autoptr(GKeyFile) key_file = g_key_file_new();
    g_autoptr(GError) error = NULL;
    if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, &error)) {
        app_window_set_error_status(dialog->win,
                                    "Theme load failed",
                                    error ? error->message : path);
        return FALSE;
    }
    if (!g_key_file_has_group(key_file, "Editor")) {
        app_window_set_error_status(dialog->win,
                                    "Theme load failed",
                                    "Theme file must contain an [Editor] group.");
        return FALSE;
    }

    theme_state_load_key_file(dialog->state, key_file);
    theme_controls_sync(dialog);
    dialog->dirty = TRUE;
    theme_preview_update(dialog);
    app_window_set_status(dialog->win, label ? label : "Theme loaded");
    return TRUE;
}

/**
 * @brief Handle a bundled theme preset selection.
 * @details Preset index zero is a placeholder. Real presets are loaded into
 *          the scratch state and previewed immediately.
 * @param drop_down The preset dropdown.
 * @param pspec The changed property, unused.
 * @param user_data The ThemeDialogState.
 */
static void theme_preset_changed(GtkDropDown *drop_down,
                                 GParamSpec *pspec,
                                 gpointer user_data) {
    (void)pspec;
    ThemeDialogState *dialog = user_data;
    if (!dialog || dialog->loading || !drop_down) return;
    guint selected = gtk_drop_down_get_selected(drop_down);
    if (selected == GTK_INVALID_LIST_POSITION || selected == 0u) return;
    guint preset_index = selected - 1u;
    if (preset_index >= G_N_ELEMENTS(GRAPTOS_THEME_PRESETS)) return;

    const ThemePreset *preset = &GRAPTOS_THEME_PRESETS[preset_index];
    g_autofree char *path = theme_preset_path(preset);
    if (!path) {
        app_window_set_error_status(dialog->win,
                                    "Theme preset missing",
                                    preset->filename);
        return;
    }
    g_autofree char *message = g_strdup_printf("Loaded theme: %s",
                                               preset->label);
    (void)theme_dialog_load_file(dialog, path, message);
}

/**
 * @brief Load a custom Graptoς theme INI file.
 * @details Custom themes use the same `[Editor]` keys as `config.ini`, making
 *          them easy to create by copying a saved config or bundled preset.
 * @param widget The button that triggered the load.
 * @param user_data The ThemeDialogState.
 */
static void theme_load_file_clicked(GtkWidget *widget, gpointer user_data) {
    ThemeDialogState *dialog = user_data;
    if (!dialog) return;
    GtkRoot *root = widget ? gtk_widget_get_root(widget) : NULL;
    GtkWindow *parent = GTK_IS_WINDOW(root)
        ? GTK_WINDOW(root)
        : app_window_gtk(dialog->win);
    g_autofree char *path =
        graptos_open_file_dialog(parent, "Load Graptoς Theme");
    if (!path) return;
    (void)theme_dialog_load_file(dialog, path, "Loaded custom theme");
}

/**
 * @brief Handle system font quick action.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void theme_font_system_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    theme_font_control_set(user_data, "");
}

/**
 * @brief Handle Inconsolata font quick action.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void theme_font_inconsolata_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ThemeFontControl *control = user_data;
    double size = control && control->size_spin
        ? gtk_spin_button_get_value(GTK_SPIN_BUTTON(control->size_spin))
        : 11.0;
    g_autofree char *font_desc = g_strdup_printf("Inconsolata %.0f", size);
    theme_font_control_set(control, font_desc);
}

/**
 * @brief Create a section heading for the theme dialog.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *theme_section_label(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "graptos-menu-title");
    return label;
}

/**
 * @brief Create the preset-loading controls for the theme dialog.
 * @details Presets are a quick starting point, not a separate theme mode. The
 *          selected preset mutates the same scratch ThemeState that manual
 *          color controls edit.
 * @param dialog The theme dialog state.
 * @return A GTK row containing preset and custom-file loading controls.
 */
static GtkWidget *theme_preset_row(ThemeDialogState *dialog) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label = gtk_label_new("Preset");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(row), label);

    GtkStringList *model = gtk_string_list_new(NULL);
    gtk_string_list_append(model, "Choose bundled theme…");
    for (guint i = 0u; i < G_N_ELEMENTS(GRAPTOS_THEME_PRESETS); i++) {
        gtk_string_list_append(model, GRAPTOS_THEME_PRESETS[i].label);
    }

    GtkWidget *drop_down = gtk_drop_down_new(G_LIST_MODEL(model), NULL);
    gtk_widget_set_tooltip_text(drop_down,
                                "Load a bundled theme into the live preview");
    g_signal_connect(drop_down, "notify::selected",
                     G_CALLBACK(theme_preset_changed), dialog);
    gtk_box_append(GTK_BOX(row), drop_down);

    gtk_box_append(GTK_BOX(row),
                   graptos_flat_button_new("Load .ini",
                                         "Load a custom Graptoς theme file",
                                         G_CALLBACK(theme_load_file_clicked),
                                         dialog));
    return row;
}

/**
 * @brief Create a row containing a color selector.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param dialog The dialog supplied by the caller.
 * @param label_text The label text supplied by the caller.
 * @param slot The slot supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *theme_color_row(ThemeDialogState *dialog,
                                  const char *label_text,
                                  char **slot) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label = gtk_label_new(label_text ? label_text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(row), label);

    ThemeColorControl *control = g_new0(ThemeColorControl, 1);
    control->dialog = dialog;
    control->slot = slot;

    GdkRGBA rgba;
    gboolean has_rgba = slot && *slot && gdk_rgba_parse(&rgba, *slot);

    GtkWidget *entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry), slot && *slot ? *slot : "");
    gtk_widget_set_size_request(entry, 110, -1);
    gtk_widget_set_tooltip_text(entry, "Hex color, for example #1e1e2e");
    control->entry = entry;

    GtkColorDialog *color_dialog = gtk_color_dialog_new();
    gtk_color_dialog_set_title(color_dialog, label_text ? label_text : "Color");
    gtk_color_dialog_set_with_alpha(color_dialog, FALSE);
    /**
     * @brief Hand color dialog ownership to GtkColorDialogButton.
     */
    GtkWidget *button = gtk_color_dialog_button_new(color_dialog);
    gtk_widget_set_tooltip_text(button, "Open visual color selector");
    control->button = button;

    if (has_rgba) {
        theme_color_control_set_rgba(control, &rgba);
    }

    g_object_set_data_full(G_OBJECT(row), "graptos-theme-color-control",
                           control, g_free);
    g_signal_connect(entry, "changed",
                     G_CALLBACK(theme_color_entry_changed), control);
    g_signal_connect(button, "notify::rgba",
                     G_CALLBACK(theme_color_button_changed), control);
    gtk_box_append(GTK_BOX(row), entry);
    gtk_box_append(GTK_BOX(row), button);
    return row;
}

/**
 * @brief Create a row containing a font selector.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param dialog The dialog supplied by the caller.
 * @param label_text The label text supplied by the caller.
 * @param slot The slot supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *theme_font_row(ThemeDialogState *dialog,
                                 const char *label_text,
                                 char **slot) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label = gtk_label_new(label_text ? label_text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(row), label);

    guint selected = 0u;
    GtkStringList *model = theme_font_model_new(slot ? *slot : "",
                                                &selected);
    /**
     * @brief Hand font model ownership to GtkDropDown.
     */
    GtkWidget *drop_down = gtk_drop_down_new(G_LIST_MODEL(model), NULL);
    gtk_drop_down_set_enable_search(GTK_DROP_DOWN(drop_down), TRUE);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(drop_down), selected);

    GtkWidget *size_spin = gtk_spin_button_new_with_range(6.0, 96.0, 1.0);
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(size_spin), 0u);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(size_spin),
                              theme_font_size_from_desc(slot ? *slot : ""));
    gtk_widget_set_tooltip_text(size_spin, "Font size in points");

    ThemeFontControl *control = g_new0(ThemeFontControl, 1);
    control->dialog = dialog;
    control->slot = slot;
    control->drop_down = drop_down;
    control->size_spin = size_spin;
    g_object_set_data_full(G_OBJECT(row), "graptos-theme-font-control",
                           control, g_free);
    g_signal_connect(drop_down, "notify::selected",
                     G_CALLBACK(theme_font_changed), control);
    g_signal_connect(size_spin, "value-changed",
                     G_CALLBACK(theme_font_size_changed), control);
    gtk_box_append(GTK_BOX(row), drop_down);
    gtk_box_append(GTK_BOX(row), size_spin);

    gtk_box_append(GTK_BOX(row),
                   graptos_flat_button_new("System", "Inherit system font",
                                         G_CALLBACK(theme_font_system_clicked),
                                         control));
    gtk_box_append(GTK_BOX(row),
                   graptos_flat_button_new("Inconsolata", "Use bundled Inconsolata",
                                         G_CALLBACK(theme_font_inconsolata_clicked),
                                         control));
    return row;
}

/**
 * @brief Append controls to a theme section.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param box The box supplied by the caller.
 * @param dialog The dialog supplied by the caller.
 * @param label The label supplied by the caller.
 * @param slot The slot supplied by the caller.
 */
static void theme_append_color(GtkWidget *box,
                               ThemeDialogState *dialog,
                               const char *label,
                               char **slot) {
    gtk_box_append(GTK_BOX(box), theme_color_row(dialog, label, slot));
}

/**
 * @brief Append a preview label.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param box The box supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param css_class The css class supplied by the caller.
 */
static void theme_preview_label(GtkWidget *box,
                                const char *text,
                                const char *css_class) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    if (css_class) gtk_widget_add_css_class(label, css_class);
    gtk_box_append(GTK_BOX(box), label);
}

/**
 * @brief Build the rendered theme preview.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *theme_preview_new(void) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(box, "graptos-theme-preview-root");
    graptos_set_all_margins(box, 10);

    theme_preview_label(box, "Top bar  New  Open  Folder", "graptos-theme-preview-top");
    theme_preview_label(box, "Tab: main.c   Tab: README.md", "graptos-theme-preview-bottom");

    GtkWidget *editor = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *gutter = gtk_label_new(" 1\n 2\n 3");
    gtk_widget_add_css_class(gutter, "graptos-theme-preview-gutter");
    gtk_box_append(GTK_BOX(editor), gutter);
    GtkWidget *code = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(code, "graptos-theme-preview-editor");
    theme_preview_label(code, "int main(void) {", "graptos-theme-preview-code");
    theme_preview_label(code, "    return 0;   ← current line", "graptos-theme-preview-current");
    theme_preview_label(code, "selected text", "graptos-theme-preview-selection");
    gtk_box_append(GTK_BOX(editor), code);
    gtk_box_append(GTK_BOX(box), editor);

    theme_preview_label(box, "Project Tree\nM  src/app.c\n?  notes.md", "graptos-theme-preview-project");
    theme_preview_label(box, "M modified", "graptos-theme-preview-git-modified");
    theme_preview_label(box, "A added", "graptos-theme-preview-git-added");
    theme_preview_label(box, "D deleted", "graptos-theme-preview-git-deleted");
    theme_preview_label(box, "R renamed", "graptos-theme-preview-git-renamed");
    theme_preview_label(box, "U conflict", "graptos-theme-preview-git-conflict");
    theme_preview_label(box, "? untracked", "graptos-theme-preview-git-untracked");
    theme_preview_label(box, "S staged", "graptos-theme-preview-git-staged");
    theme_preview_label(box, "Selected project row", "graptos-theme-preview-project-selected");
    theme_preview_label(box, "Popover demo\nDummy popover text", "graptos-theme-preview-popover");
    theme_preview_label(box, "Tooltip demo", "graptos-theme-preview-tooltip");
    theme_preview_label(box, "Completion\nprintf", "graptos-theme-preview-completion");
    theme_preview_label(box, "detail: stdio function", "graptos-theme-preview-completion-detail");
    theme_preview_label(box, "Selected completion row", "graptos-theme-preview-completion-selected");
    theme_preview_label(box, "Reference Popover", "graptos-theme-preview-ref-heading");
    theme_preview_label(box, "symbol_name", "graptos-theme-preview-ref-title");
    theme_preview_label(box, "function", "graptos-theme-preview-ref-kind");
    theme_preview_label(box, "return symbol_name();", "graptos-theme-preview-ref-snippet");
    theme_preview_label(box, "Hovered reference row", "graptos-theme-preview-ref-hover");
    theme_preview_label(box, "Native dialog preview\nSave changes?", "graptos-theme-preview-dialog");
    theme_preview_label(box, "Dialog title", "graptos-theme-preview-dialog-title");
    theme_preview_label(box, "Dialog body text explains the choice.", "graptos-theme-preview-dialog-body");
    theme_preview_label(box, "Muted helper text", "graptos-theme-preview-dialog-muted");
    theme_preview_label(box, "Git command output", "graptos-theme-preview-dialog-output");
    theme_preview_label(box, "Save action", "graptos-theme-preview-dialog-action");
    theme_preview_label(box, "Discard action", "graptos-theme-preview-dialog-destructive");
    theme_preview_label(box, "Dialog input text", "graptos-theme-preview-dialog-input");
    theme_preview_label(box, "Codex response preview", "graptos-theme-preview-codex");
    theme_preview_label(box, "Ask Codex…", "graptos-theme-preview-codex-prompt");
    theme_preview_label(box, "Search match", "graptos-theme-preview-search");
    theme_preview_label(box, "Diagnostic warning", "graptos-theme-preview-warning");
    theme_preview_label(box, "$ make check", "graptos-theme-preview-terminal");
    theme_preview_label(box, "Rendered Markdown Preview", "graptos-theme-preview-rendered");
    return box;
}

/**
 * @brief Build the theme control list.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param dialog The dialog supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *theme_controls_new(ThemeDialogState *dialog) {
    ThemeState *s = dialog->state;
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    graptos_set_all_margins(box, 8);
    dialog->controls_root = box;

    gtk_box_append(GTK_BOX(box), theme_section_label("Themes"));
    gtk_box_append(GTK_BOX(box), theme_preset_row(dialog));

    gtk_box_append(GTK_BOX(box), theme_section_label("Fonts"));
    gtk_box_append(GTK_BOX(box), theme_font_row(dialog, "UI", &s->ui_font));
    gtk_box_append(GTK_BOX(box), theme_font_row(dialog, "Editor", &s->editor_font));
    gtk_box_append(GTK_BOX(box), theme_font_row(dialog, "Preview", &s->preview_font));
    gtk_box_append(GTK_BOX(box), theme_font_row(dialog, "Terminal", &s->terminal_font));
    gtk_box_append(GTK_BOX(box), theme_font_row(dialog, "Code / snippets", &s->code_font));

    gtk_box_append(GTK_BOX(box), theme_section_label("Editor"));
    theme_append_color(box, dialog, "Editor background", &s->editor_bg_color);
    theme_append_color(box, dialog, "Editor foreground", &s->editor_fg_color);
    theme_append_color(box, dialog, "Gutter background", &s->editor_gutter_bg_color);
    theme_append_color(box, dialog, "Gutter foreground", &s->editor_gutter_fg_color);
    theme_append_color(box, dialog, "Current line", &s->editor_current_line_bg_color);
    theme_append_color(box, dialog, "Selection background", &s->editor_selection_bg_color);
    theme_append_color(box, dialog, "Selection foreground", &s->editor_selection_fg_color);
    theme_append_color(box, dialog, "Cursor", &s->editor_cursor_color);

    gtk_box_append(GTK_BOX(box), theme_section_label("Chrome"));
    theme_append_color(box, dialog, "Topbar background", &s->topbar_bg_color);
    theme_append_color(box, dialog, "Topbar foreground", &s->topbar_fg_color);
    theme_append_color(box, dialog, "Bottombar background", &s->bottombar_bg_color);
    theme_append_color(box, dialog, "Bottombar foreground", &s->bottombar_fg_color);
    theme_append_color(box, dialog, "Button background", &s->button_bg_color);
    theme_append_color(box, dialog, "Button foreground", &s->button_fg_color);
    theme_append_color(box, dialog, "Button hover", &s->button_hover_bg_color);
    theme_append_color(box, dialog, "Button active", &s->button_active_bg_color);
    theme_append_color(box, dialog, "Status error", &s->status_error_color);

    gtk_box_append(GTK_BOX(box), theme_section_label("Tabs and Project Tree"));
    theme_append_color(box, dialog, "Tabbar background", &s->tabbar_bg_color);
    theme_append_color(box, dialog, "Tabbar foreground", &s->tabbar_fg_color);
    theme_append_color(box, dialog, "Active tab background", &s->tab_active_bg_color);
    theme_append_color(box, dialog, "Active tab foreground", &s->tab_active_fg_color);
    theme_append_color(box, dialog, "Project background", &s->sidebar_bg_color);
    theme_append_color(box, dialog, "Project foreground", &s->project_tree_fg_color);
    theme_append_color(box, dialog, "Project selected background", &s->project_tree_selected_bg_color);
    theme_append_color(box, dialog, "Project selected foreground", &s->project_tree_selected_fg_color);
    theme_append_color(box, dialog, "Git modified indicator", &s->git_status_modified_color);
    theme_append_color(box, dialog, "Git added indicator", &s->git_status_added_color);
    theme_append_color(box, dialog, "Git deleted indicator", &s->git_status_deleted_color);
    theme_append_color(box, dialog, "Git renamed indicator", &s->git_status_renamed_color);
    theme_append_color(box, dialog, "Git conflict indicator", &s->git_status_conflict_color);
    theme_append_color(box, dialog, "Git untracked indicator", &s->git_status_untracked_color);
    theme_append_color(box, dialog, "Git staged indicator", &s->git_status_staged_color);

    gtk_box_append(GTK_BOX(box), theme_section_label("Inputs and Preview"));
    theme_append_color(box, dialog, "Input background", &s->input_bg_color);
    theme_append_color(box, dialog, "Input foreground", &s->input_fg_color);
    theme_append_color(box, dialog, "Input border", &s->input_border_color);
    theme_append_color(box, dialog, "Preview background", &s->scroll_preview_bg_color);
    theme_append_color(box, dialog, "Preview foreground", &s->scroll_preview_fg_color);

    gtk_box_append(GTK_BOX(box), theme_section_label("Popovers"));
    theme_append_color(box, dialog, "Generic popover background", &s->popover_bg_color);
    theme_append_color(box, dialog, "Generic popover border", &s->popover_border_color);
    theme_append_color(box, dialog, "Tooltip background", &s->tooltip_bg_color);
    theme_append_color(box, dialog, "Tooltip foreground", &s->tooltip_fg_color);
    theme_append_color(box, dialog, "Tooltip border", &s->tooltip_border_color);
    theme_append_color(box, dialog, "Reference background", &s->ref_popover_bg_color);
    theme_append_color(box, dialog, "Reference foreground", &s->ref_popover_fg_color);
    theme_append_color(box, dialog, "Reference heading", &s->ref_popover_heading_color);
    theme_append_color(box, dialog, "Reference title", &s->ref_popover_title_color);
    theme_append_color(box, dialog, "Reference kind", &s->ref_popover_kind_color);
    theme_append_color(box, dialog, "Reference snippet", &s->ref_popover_snippet_color);
    theme_append_color(box, dialog, "Reference hover background", &s->ref_popover_hover_bg_color);
    theme_append_color(box, dialog, "Reference hover foreground", &s->ref_popover_hover_fg_color);

    gtk_box_append(GTK_BOX(box), theme_section_label("Completion"));
    theme_append_color(box, dialog, "Completion background", &s->completion_popover_bg_color);
    theme_append_color(box, dialog, "Completion foreground", &s->completion_popover_fg_color);
    theme_append_color(box, dialog, "Completion detail", &s->completion_popover_detail_color);
    theme_append_color(box, dialog, "Completion selected background", &s->completion_selection_bg_color);
    theme_append_color(box, dialog, "Completion selected foreground", &s->completion_selection_fg_color);

    gtk_box_append(GTK_BOX(box), theme_section_label("Dialogs, Search, Codex"));
    theme_append_color(box, dialog, "Dialog background", &s->dialog_bg_color);
    theme_append_color(box, dialog, "Dialog foreground", &s->dialog_fg_color);
    theme_append_color(box, dialog, "Dialog border", &s->dialog_border_color);
    theme_append_color(box, dialog, "Dialog title", &s->dialog_title_color);
    theme_append_color(box, dialog, "Dialog body", &s->dialog_body_color);
    theme_append_color(box, dialog, "Dialog muted text", &s->dialog_muted_color);
    theme_append_color(box, dialog, "Dialog output text", &s->dialog_output_color);
    theme_append_color(box, dialog, "Git output background", &s->git_output_bg_color);
    theme_append_color(box, dialog, "Dialog action text", &s->dialog_action_color);
    theme_append_color(box, dialog, "Dialog destructive action",
                       &s->dialog_destructive_action_color);
    theme_append_color(box, dialog, "Dialog input foreground",
                       &s->dialog_input_fg_color);
    theme_append_color(box, dialog, "Dialog input background",
                       &s->dialog_input_bg_color);
    theme_append_color(box, dialog, "Search match background", &s->search_match_bg_color);
    theme_append_color(box, dialog, "Search match foreground", &s->search_match_fg_color);
    theme_append_color(box, dialog, "Diagnostic warning background", &s->diagnostic_warning_bg_color);
    theme_append_color(box, dialog, "Diagnostic warning foreground", &s->diagnostic_warning_fg_color);
    theme_append_color(box, dialog, "Codex preview background", &s->codex_preview_bg_color);
    theme_append_color(box, dialog, "Codex preview foreground", &s->codex_preview_fg_color);
    theme_append_color(box, dialog, "Codex prompt background", &s->codex_prompt_bg_color);
    return box;
}

/**
 * @brief Prompt for unsaved theme changes.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param parent The parent supplied by the caller.
 * @return The computed value requested by the caller.
 */
static int theme_unsaved_prompt(GtkWindow *parent) {
    GraptosDialogAction actions[] = {
        { "Discard", "Discard preview changes", GRAPTOS_THEME_RESPONSE_CANCEL,
          TRUE, FALSE },
        { "Keep Editing", "Return to theme editor",
          GRAPTOS_THEME_RESPONSE_KEEP_EDITING, FALSE, TRUE },
        { "Save", "Apply and save changes", GRAPTOS_THEME_RESPONSE_APPLY,
          FALSE, FALSE },
    };
    return dialog_run_message(
        parent,
        "Unsaved theme changes",
        "Save theme changes before closing?",
        "You can save the live preview to config.ini, discard it, or keep editing.",
        460,
        180,
        actions,
        G_N_ELEMENTS(actions),
        GRAPTOS_THEME_RESPONSE_KEEP_EDITING);
}

/**
 * @brief Action show preferences.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_show_preferences(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    ThemeDialogState dialog_state;
    memset(&dialog_state, 0, sizeof(dialog_state));
    dialog_state.win = win;
    dialog_state.state = theme_state_from_window(win);
    if (!dialog_state.state) return;
    dialog_state.loading = TRUE;

    GtkWidget *dialog = gtk_window_new();
    gtk_widget_add_css_class(dialog, "graptos-window");
    gtk_window_set_title(GTK_WINDOW(dialog), GRAPTOS_DISPLAY_NAME " Theme");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 1180, 760);
    GtkWindow *parent = app_window_gtk(win);
    if (parent) gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(root, "graptos-root");
    graptos_set_all_margins(root, 10);
    gtk_window_set_child(GTK_WINDOW(dialog), root);

    GtkWidget *content = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(content, TRUE);
    gtk_box_append(GTK_BOX(root), content);

    GtkWidget *controls_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(controls_scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(controls_scroll, 580, -1);
    GtkWidget *controls = theme_controls_new(&dialog_state);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(controls_scroll),
                                  controls);
    gtk_paned_set_start_child(GTK_PANED(content), controls_scroll);
    gtk_paned_set_resize_start_child(GTK_PANED(content), FALSE);

    GtkWidget *preview_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(preview_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    dialog_state.preview_root = theme_preview_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(preview_scroll),
                                  dialog_state.preview_root);
    gtk_paned_set_end_child(GTK_PANED(content), preview_scroll);
    gtk_paned_set_resize_end_child(GTK_PANED(content), TRUE);
    gtk_paned_set_position(GTK_PANED(content), 600);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(buttons),
                   graptos_flat_button_new("Cancel", "Close without applying",
                                         G_CALLBACK(graptos_modal_window_respond),
                                         GINT_TO_POINTER(GRAPTOS_THEME_RESPONSE_CANCEL)));
    gtk_box_append(GTK_BOX(buttons),
                   graptos_flat_button_new("Reload config.ini",
                                         "Apply the manually edited config.ini file",
                                         G_CALLBACK(graptos_modal_window_respond),
                                         GINT_TO_POINTER(GRAPTOS_THEME_RESPONSE_RELOAD_CONFIG)));
    gtk_box_append(GTK_BOX(buttons),
                   graptos_flat_button_new("Apply", "Apply theme and save config",
                                         G_CALLBACK(graptos_modal_window_respond),
                                         GINT_TO_POINTER(GRAPTOS_THEME_RESPONSE_APPLY)));
    gtk_box_append(GTK_BOX(root), buttons);

    dialog_state.loading = FALSE;
    theme_preview_update(&dialog_state);

    gboolean running = TRUE;
    while (running) {
        int response = graptos_modal_window_run(GTK_WINDOW(dialog),
                                              GRAPTOS_THEME_RESPONSE_CANCEL);
        if (response == GRAPTOS_THEME_RESPONSE_APPLY) {
            theme_state_apply_to_window(dialog_state.state, win);
            app_window_apply_css(win);
            apply_preferences_to_all_tabs(win);
            graptos_config_save(win);
            dialog_state.dirty = FALSE;
            running = FALSE;
        } else if (response == GRAPTOS_THEME_RESPONSE_RELOAD_CONFIG) {
            g_autofree char *path = graptos_config_path();
            if (!path || !g_file_test(path, G_FILE_TEST_EXISTS)) {
                dialog_error(GTK_WINDOW(dialog),
                             "config.ini not found",
                             path ? path : "Unable to resolve config path.");
                continue;
            }
            graptos_config_load(win);
            app_window_apply_css(win);
            apply_preferences_to_all_tabs(win);
            app_window_set_status(win, "Reloaded config.ini");
            dialog_state.dirty = FALSE;
            running = FALSE;
        } else if (dialog_state.dirty) {
            int unsaved = theme_unsaved_prompt(GTK_WINDOW(dialog));
            if (unsaved == GRAPTOS_THEME_RESPONSE_APPLY) {
                theme_state_apply_to_window(dialog_state.state, win);
                app_window_apply_css(win);
                apply_preferences_to_all_tabs(win);
                graptos_config_save(win);
                running = FALSE;
            } else if (unsaved == GRAPTOS_THEME_RESPONSE_CANCEL) {
                running = FALSE;
            }
        } else {
            running = FALSE;
        }
    }

    if (dialog_state.preview_provider) {
        GdkDisplay *display = gdk_display_get_default();
        if (display) {
            gtk_style_context_remove_provider_for_display(
                display, GTK_STYLE_PROVIDER(dialog_state.preview_provider));
        }
        g_clear_object(&dialog_state.preview_provider);
    }
    theme_state_free(dialog_state.state);
    graptos_widget_destroy(dialog);
}


/**
 * @brief Action toggle minimap.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_minimap(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    /**
     * @brief Update the global window preference before reapplying tab state.
     */
    win->minimap_enabled = !win->minimap_enabled;
    apply_preferences_to_all_tabs(win);
    graptos_config_save(win);
}


/**
 * @brief Action toggle preview.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_preview(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    win->preview_enabled = !win->preview_enabled;
    apply_preferences_to_all_tabs(win);
    graptos_config_save(win);
}


/**
 * @brief Action render latex.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_render_latex(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = app_window_current_tab(user_data);
    /**
     * @brief Keep LaTeX rendering tab-scoped because each tab owns preview state.
     */
    if (tab) editor_tab_render_latex(tab);
}
