/**
 * @file src/app_layout.c
 * @brief Top, bottom, and tool panel layout construction.
 */

#include "app_private.h"

#include <string.h>

/**
 * @brief Menu small label.
 */
GtkWidget *menu_small_label(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "cleaf-menu-small");
    return label;
}


/**
 * @brief Build top bar.
 */
GtkWidget *build_top_bar(EditorWindow *win) {
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(top, "cleaf-top");

    GtkWidget *new_button = cleaf_flat_button_new("New", "New tab (Ctrl+N / Ctrl+T)", G_CALLBACK(action_new), win);
    GtkWidget *open_button = cleaf_flat_button_new("Open", "Open file in new tab (Ctrl+O)", G_CALLBACK(action_open), win);
    GtkWidget *folder_button = cleaf_flat_button_new("Folder", "Add project folder", G_CALLBACK(action_open_folder), win);
    gtk_box_append(GTK_BOX(top), new_button);
    gtk_box_append(GTK_BOX(top), open_button);
    gtk_box_append(GTK_BOX(top), folder_button);

    win->top_title_label = gtk_label_new("Untitled");
    gtk_label_set_xalign(GTK_LABEL(win->top_title_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(win->top_title_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_add_css_class(win->top_title_label, "cleaf-title");
    gtk_widget_set_hexpand(win->top_title_label, TRUE);
    gtk_box_append(GTK_BOX(top), win->top_title_label);

    return top;
}


/**
 * @brief Build search panel.
 */
GtkWidget *build_search_panel(EditorWindow *win) {
    win->search_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(win->search_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration(GTK_REVEALER(win->search_revealer), 120u);

    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(panel, "cleaf-search-panel");

    GtkWidget *find_label = gtk_label_new("Find");
    win->find_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(win->find_entry), "Text to find");
    gtk_widget_set_size_request(win->find_entry, 220, -1);
    GtkWidget *prev_button = cleaf_flat_button_new("Prev", "Find previous (Shift+F3)", G_CALLBACK(action_find_prev), win);
    GtkWidget *next_button = cleaf_flat_button_new("Next", "Find next (F3)", G_CALLBACK(action_find_next), win);

    win->replace_label = gtk_label_new("Replace");
    win->replace_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(win->replace_entry), "Replacement text");
    gtk_widget_set_size_request(win->replace_entry, 220, -1);
    win->replace_button = cleaf_flat_button_new("Replace", "Replace current match", G_CALLBACK(action_replace), win);
    win->replace_all_button = cleaf_flat_button_new("All", "Replace all", G_CALLBACK(action_replace_all), win);
    GtkWidget *close_button = cleaf_flat_button_new("×", "Close find/replace panel (Esc)", G_CALLBACK(action_hide_search), win);

    gtk_box_append(GTK_BOX(panel), find_label);
    gtk_box_append(GTK_BOX(panel), win->find_entry);
    gtk_box_append(GTK_BOX(panel), prev_button);
    gtk_box_append(GTK_BOX(panel), next_button);
    gtk_box_append(GTK_BOX(panel), cleaf_separator_new());
    gtk_box_append(GTK_BOX(panel), win->replace_label);
    gtk_box_append(GTK_BOX(panel), win->replace_entry);
    gtk_box_append(GTK_BOX(panel), win->replace_button);
    gtk_box_append(GTK_BOX(panel), win->replace_all_button);
    gtk_box_append(GTK_BOX(panel), close_button);

    gtk_revealer_set_child(GTK_REVEALER(win->search_revealer), panel);
    return win->search_revealer;
}


/**
 * @brief Tool button new.
 */
static GtkWidget *tool_button_new(const char *label,
                                  const char *tooltip,
                                  GCallback callback,
                                  gpointer data) {
    return cleaf_flat_button_new(label, tooltip, callback, data);
}

/**
 * @brief Action toggle codex panel.
 */
void action_toggle_codex_panel(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (win) codex_panel_toggle(win->codex_panel);
}

/**
 * @brief Tool title new.
 */
static GtkWidget *tool_title_new(const char *title) {
    GtkWidget *label = gtk_label_new(title ? title : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "cleaf-tool-title");
    return label;
}

/**
 * @brief Tool panel reset widget pointers.
 */
static void tool_panel_reset_widget_pointers(EditorWindow *win) {
    if (!win) return;
    win->syntax_combo = NULL;
    win->indent_status_label = NULL;
    win->indent_dropdown = NULL;
    win->autocomplete_button = NULL;
    win->autosave_button = NULL;
    win->backup_button = NULL;
    win->minimap_button = NULL;
    win->preview_button = NULL;
    win->syntax_override_button = NULL;
}

/**
 * @brief Tool panel clear.
 */
static void tool_panel_clear(EditorWindow *win) {
    if (!win || !win->tool_panel) return;
    tool_panel_reset_widget_pointers(win);
    GtkWidget *child = gtk_widget_get_first_child(win->tool_panel);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(win->tool_panel), child);
        child = next;
    }
}

/**
 * @brief Tool panel begin.
 */
static void tool_panel_begin(EditorWindow *win, const char *title) {
    if (!win || !win->tool_panel) return;
    tool_panel_clear(win);
    gtk_box_append(GTK_BOX(win->tool_panel), tool_title_new(title));
    gtk_box_append(GTK_BOX(win->tool_panel), cleaf_separator_new());
}

/**
 * @brief Hide tool panel.
 */
static void hide_tool_panel(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (win && win->tool_revealer) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(win->tool_revealer), FALSE);
    }
}

/**
 * @brief Tool panel show ready.
 */
static void tool_panel_show_ready(EditorWindow *win) {
    if (!win || !win->tool_panel || !win->tool_revealer) return;
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(win->tool_panel), spacer);
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("×", "Close option bar", G_CALLBACK(hide_tool_panel), win));
    gtk_revealer_set_reveal_child(GTK_REVEALER(win->tool_revealer), TRUE);
}

/**
 * @brief Tool switch new.
 */
static GtkWidget *tool_switch_new(const char *label_text,
                                  gboolean active,
                                  GCallback notify_cb,
                                  gpointer data,
                                  GtkWidget **switch_out) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_widget_add_css_class(row, "cleaf-switch-row");

    GtkWidget *label = gtk_label_new(label_text ? label_text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "cleaf-switch-label");
    gtk_box_append(GTK_BOX(row), label);

    GtkWidget *sw = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(sw), active);
    gtk_widget_set_valign(sw, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(sw, label_text ? label_text : "Toggle option");
    g_signal_connect(sw, "notify::active", notify_cb, data);
    gtk_box_append(GTK_BOX(row), sw);

    if (switch_out) *switch_out = sw;
    return row;
}

/**
 * @brief On indent dropdown changed.
 */
static void on_indent_dropdown_changed(GtkDropDown *drop_down,
                                       GParamSpec *pspec,
                                       gpointer user_data) {
    (void)pspec;
    EditorWindow *win = user_data;
    if (!win || win->controls_updating || !drop_down) return;

    switch (gtk_drop_down_get_selected(drop_down)) {
    case 0u:
        set_tab_policy(win, 2u, TRUE);
        break;
    case 1u:
        set_tab_policy(win, 4u, TRUE);
        break;
    case 2u:
        set_tab_policy(win, 8u, TRUE);
        break;
    case 3u:
        set_tab_policy(win, 4u, FALSE);
        break;
    default:
        break;
    }
}

/**
 * @brief On autocomplete switch changed.
 */
static void on_autocomplete_switch_changed(GtkSwitch *sw,
                                           GParamSpec *pspec,
                                           gpointer user_data) {
    (void)pspec;
    EditorWindow *win = user_data;
    if (!win || win->controls_updating || !sw) return;
    win->autocomplete_enabled = gtk_switch_get_active(sw);
    apply_tab_policy_to_all_tabs(win);
    cleaf_config_save(win);
}

/**
 * @brief On autosave switch changed.
 */
static void on_autosave_switch_changed(GtkSwitch *sw,
                                       GParamSpec *pspec,
                                       gpointer user_data) {
    (void)pspec;
    EditorWindow *win = user_data;
    if (!win || win->controls_updating || !sw) return;
    win->auto_save_enabled = gtk_switch_get_active(sw);
    restart_auto_save_timer(win);
    cleaf_config_save(win);
}

/**
 * @brief On backup switch changed.
 */
static void on_backup_switch_changed(GtkSwitch *sw,
                                     GParamSpec *pspec,
                                     gpointer user_data) {
    (void)pspec;
    EditorWindow *win = user_data;
    if (!win || win->controls_updating || !sw) return;
    win->backup_enabled = gtk_switch_get_active(sw);
    apply_tab_policy_to_all_tabs(win);
    cleaf_config_save(win);
}

/**
 * @brief On minimap switch changed.
 */
static void on_minimap_switch_changed(GtkSwitch *sw,
                                      GParamSpec *pspec,
                                      gpointer user_data) {
    (void)pspec;
    EditorWindow *win = user_data;
    if (!win || win->controls_updating || !sw) return;
    win->minimap_enabled = gtk_switch_get_active(sw);
    apply_preferences_to_all_tabs(win);
    cleaf_config_save(win);
}

/**
 * @brief On preview switch changed.
 */
static void on_preview_switch_changed(GtkSwitch *sw,
                                      GParamSpec *pspec,
                                      gpointer user_data) {
    (void)pspec;
    EditorWindow *win = user_data;
    if (!win || win->controls_updating || !sw) return;
    win->preview_enabled = gtk_switch_get_active(sw);
    apply_preferences_to_all_tabs(win);
    cleaf_config_save(win);
}

/**
 * @brief On yaml override switch changed.
 */
static void on_yaml_override_switch_changed(GtkSwitch *sw,
                                            GParamSpec *pspec,
                                            gpointer user_data) {
    (void)pspec;
    EditorWindow *win = user_data;
    if (!win || win->controls_updating || !sw) return;
    win->use_yaml_style_overrides = gtk_switch_get_active(sw);
    if (win->notebook) {
        int pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
        for (int i = 0; i < pages; i++) {
            GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
            EditorTab *tab = page ? g_object_get_data(G_OBJECT(page), "cleaf-tab") : NULL;
            if (tab) {
                editor_tab_update_highlight_engine(tab);
                gtk_widget_queue_draw(tab->text_view);
                if (tab->minimap_view) gtk_widget_queue_draw(tab->minimap_view);
            }
        }
    }
    cleaf_config_save(win);
}

/**
 * @brief Show file tools.
 */
static void show_file_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "File");
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Save", "Save current tab (Ctrl+S)", G_CALLBACK(action_save), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Save As", "Save current tab as (Ctrl+Shift+S)", G_CALLBACK(action_save_as), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Folder +", "Open folder in a new Cleaf instance", G_CALLBACK(action_open_folder_new_instance), win));
    tool_panel_show_ready(win);
}

/**
 * @brief Show edit tools.
 */
static void show_edit_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "Edit");
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Replace", "Open replace panel (Ctrl+H)", G_CALLBACK(action_show_replace), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Project Search", "Search and replace across the project", G_CALLBACK(action_project_search), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Comment", "Toggle line comment (Ctrl+/)", G_CALLBACK(action_comment), win));
    tool_panel_show_ready(win);
}

/**
 * @brief Show indent tools.
 */
static void show_indent_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "Indent");
    const char *indent_options[] = { "2 spaces", "4 spaces", "8 spaces", "Tabs", NULL };
    win->indent_dropdown = gtk_drop_down_new_from_strings(indent_options);
    gtk_widget_set_size_request(win->indent_dropdown, 120, -1);
    gtk_widget_set_tooltip_text(win->indent_dropdown, "Tab insertion policy");
    g_signal_connect(win->indent_dropdown, "notify::selected", G_CALLBACK(on_indent_dropdown_changed), win);
    gtk_box_append(GTK_BOX(win->tool_panel), win->indent_dropdown);
    win->indent_status_label = menu_small_label("Indent: 4 spaces");
    gtk_box_append(GTK_BOX(win->tool_panel), win->indent_status_label);
    update_policy_buttons(win);
    tool_panel_show_ready(win);
}

/**
 * @brief Show syntax tools.
 */
static void show_syntax_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "Syntax");
    win->syntax_combo = gtk_drop_down_new(NULL, NULL);
    gtk_widget_set_tooltip_text(win->syntax_combo, "Syntax mode");
    gtk_widget_set_size_request(win->syntax_combo, 190, -1);
    gtk_box_append(GTK_BOX(win->tool_panel), win->syntax_combo);
    g_signal_connect(win->syntax_combo, "notify::selected", G_CALLBACK(on_syntax_changed), win);
    populate_syntax_combo(win, app_window_current_tab(win));
    gtk_box_append(GTK_BOX(win->tool_panel),
                   tool_switch_new("YAML Overrides", win->use_yaml_style_overrides,
                                   G_CALLBACK(on_yaml_override_switch_changed),
                                   win, &win->syntax_override_button));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Reload", "Reload YAML syntax definitions", G_CALLBACK(action_reload_syntax), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Info", "Show loaded syntax definitions", G_CALLBACK(action_syntax_diagnostics), win));
    tool_panel_show_ready(win);
}

/**
 * @brief Show intelligence tools.
 */
static void show_intelligence_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "Intelligence");
    gtk_box_append(GTK_BOX(win->tool_panel), tool_switch_new("Auto", win->autocomplete_enabled, G_CALLBACK(on_autocomplete_switch_changed), win, &win->autocomplete_button));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_switch_new("Scroll", win->minimap_enabled, G_CALLBACK(on_minimap_switch_changed), win, &win->minimap_button));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_switch_new("Preview", win->preview_enabled, G_CALLBACK(on_preview_switch_changed), win, &win->preview_button));
    update_policy_buttons(win);
    tool_panel_show_ready(win);
}

/**
 * @brief Show safety tools.
 */
static void show_safety_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "Safety");
    gtk_box_append(GTK_BOX(win->tool_panel), tool_switch_new("Auto Save", win->auto_save_enabled, G_CALLBACK(on_autosave_switch_changed), win, &win->autosave_button));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_switch_new("Backup", win->backup_enabled, G_CALLBACK(on_backup_switch_changed), win, &win->backup_button));
    update_policy_buttons(win);
    tool_panel_show_ready(win);
}


/**
 * @brief Show git tools.
 */
static void show_git_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "Git");
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Status", "Show Git status", G_CALLBACK(action_git_status), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Diff", "Show diff for current file or repository", G_CALLBACK(action_git_diff), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Stage", "Stage current file", G_CALLBACK(action_git_stage), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Unstage", "Unstage current file", G_CALLBACK(action_git_unstage), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Stage All", "Stage all repository changes", G_CALLBACK(action_git_stage_all), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Commit", "Commit staged changes", G_CALLBACK(action_git_commit), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Pull", "Pull with --ff-only", G_CALLBACK(action_git_pull), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Push", "Push to upstream", G_CALLBACK(action_git_push), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Run", "Run arbitrary git arguments in the active repository", G_CALLBACK(action_git_run), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Credentials", "Save HTTPS Git credentials via Git credential helper", G_CALLBACK(action_git_credentials), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("Refresh", "Refresh Git status indicators", G_CALLBACK(action_git_refresh), win));
    tool_panel_show_ready(win);
}

/**
 * @brief Show view tools.
 */
static void show_view_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "View");
    gtk_box_append(GTK_BOX(win->tool_panel),
                   tool_switch_new("Markdown Preview", win->preview_enabled,
                                   G_CALLBACK(on_preview_switch_changed), win,
                                   &win->preview_button));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("LaTeX PDF", "Render LaTeX and open the PDF", G_CALLBACK(action_render_latex), win));
    gtk_box_append(GTK_BOX(win->tool_panel), tool_button_new("About", "Show Cleaf version and build information", G_CALLBACK(action_about), win));
    tool_panel_show_ready(win);
}

/**
 * @brief Build tool panel.
 */
GtkWidget *build_tool_panel(EditorWindow *win) {
    win->tool_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(win->tool_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration(GTK_REVEALER(win->tool_revealer), 120u);

    win->tool_panel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(win->tool_panel, "cleaf-tool-panel");
    gtk_widget_set_hexpand(win->tool_panel, TRUE);
    gtk_revealer_set_child(GTK_REVEALER(win->tool_revealer), win->tool_panel);
    gtk_revealer_set_reveal_child(GTK_REVEALER(win->tool_revealer), FALSE);
    return win->tool_revealer;
}

/**
 * @brief On status label clicked.
 */
static void on_status_label_clicked(GtkGestureClick *gesture,
                                    int n_press,
                                    double x,
                                    double y,
                                    gpointer user_data) {
    (void)gesture;
    (void)n_press;
    (void)x;
    (void)y;
    app_window_show_status_error(user_data);
}

/**
 * @brief Build bottom bar.
 */
GtkWidget *build_bottom_bar(EditorWindow *win) {
    GtkWidget *bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(bottom, "cleaf-bottom");

    win->status_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(win->status_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(win->status_label), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(win->status_label, "cleaf-status");
    gtk_widget_set_size_request(win->status_label, 180, -1);
    GtkGesture *status_click = gtk_gesture_click_new();
    g_signal_connect(status_click, "pressed",
                     G_CALLBACK(on_status_label_clicked), win);
    gtk_widget_add_controller(win->status_label,
                              GTK_EVENT_CONTROLLER(status_click));
    gtk_box_append(GTK_BOX(bottom), win->status_label);

    gtk_box_append(GTK_BOX(bottom), tool_button_new("Find", "Open find panel (Ctrl+F)", G_CALLBACK(action_show_find), win));
    gtk_box_append(GTK_BOX(bottom), cleaf_separator_new());
    gtk_box_append(GTK_BOX(bottom), tool_button_new("File", "Show file commands", G_CALLBACK(show_file_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_button_new("Edit", "Show edit commands", G_CALLBACK(show_edit_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_button_new("Indent", "Show indentation controls", G_CALLBACK(show_indent_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_button_new("Syntax", "Show syntax controls", G_CALLBACK(show_syntax_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_button_new("Intelligence", "Show code intelligence controls", G_CALLBACK(show_intelligence_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_button_new("Safety", "Show autosave and backup controls", G_CALLBACK(show_safety_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_button_new("View", "Show view commands", G_CALLBACK(show_view_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_button_new("Git", "Show Git commands", G_CALLBACK(show_git_tools), win));
    gtk_box_append(GTK_BOX(bottom), cleaf_separator_new());
    gtk_box_append(GTK_BOX(bottom), tool_button_new("AI", "Toggle Codex panel", G_CALLBACK(action_toggle_codex_panel), win));

    return bottom;
}
