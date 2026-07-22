/**
 * @file src/app_layout.c
 * @brief Top, bottom, and tool panel layout construction.
 * @details The layout code describes the shape of the app. Tool grouping, side panels,
 *          status widgets, and bottom-bar controls are product decisions, so keeping them
 *          together makes it easier to tell what is an action and what is just display.
 */

#include "app_private.h"
#include "editor_tab_private.h"

#include <string.h>

/**
 * @brief Menu small label.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *menu_small_label(const char *text) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "graptos-menu-small");
    return label;
}


/**
 * @brief Build top bar.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *build_top_bar(EditorWindow *win) {
    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(top, "graptos-top");

    GtkWidget *new_button = graptos_flat_button_new("New", "New tab (Ctrl+N / Ctrl+T)", G_CALLBACK(action_new), win);
    GtkWidget *open_button = graptos_flat_button_new("Open", "Open file in new tab (Ctrl+O)", G_CALLBACK(action_open), win);
    GtkWidget *folder_button = graptos_flat_button_new("Folder", "Add project folder", G_CALLBACK(action_open_folder), win);
    gtk_box_append(GTK_BOX(top), new_button);
    gtk_box_append(GTK_BOX(top), open_button);
    gtk_box_append(GTK_BOX(top), folder_button);

    win->top_title_label = gtk_label_new("Untitled");
    gtk_label_set_xalign(GTK_LABEL(win->top_title_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(win->top_title_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_add_css_class(win->top_title_label, "graptos-title");
    gtk_widget_set_hexpand(win->top_title_label, TRUE);
    gtk_box_append(GTK_BOX(top), win->top_title_label);

    return top;
}


/**
 * @brief Build search panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *build_search_panel(EditorWindow *win) {
    win->search_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(win->search_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration(GTK_REVEALER(win->search_revealer), 120u);

    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(panel, "graptos-search-panel");

    GtkWidget *find_label = gtk_label_new("Find");
    win->find_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(win->find_entry), "Text to find");
    gtk_widget_set_size_request(win->find_entry, 220, -1);
    GtkWidget *prev_button = graptos_flat_button_new("Prev", "Find previous (Shift+F3)", G_CALLBACK(action_find_prev), win);
    GtkWidget *next_button = graptos_flat_button_new("Next", "Find next (F3)", G_CALLBACK(action_find_next), win);

    win->replace_label = gtk_label_new("Replace");
    win->replace_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(win->replace_entry), "Replacement text");
    gtk_widget_set_size_request(win->replace_entry, 220, -1);
    win->replace_button = graptos_flat_button_new("Replace", "Replace current match", G_CALLBACK(action_replace), win);
    win->replace_all_button = graptos_flat_button_new("All", "Replace all", G_CALLBACK(action_replace_all), win);
    GtkWidget *close_button = graptos_flat_button_new("×", "Close find/replace panel (Esc)", G_CALLBACK(action_hide_search), win);

    gtk_box_append(GTK_BOX(panel), find_label);
    gtk_box_append(GTK_BOX(panel), win->find_entry);
    gtk_box_append(GTK_BOX(panel), prev_button);
    gtk_box_append(GTK_BOX(panel), next_button);
    gtk_box_append(GTK_BOX(panel), graptos_separator_new());
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
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param label The label supplied by the caller.
 * @param tooltip The tooltip supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param data The callback context passed by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *tool_button_new(const char *label,
                                  const char *tooltip,
                                  GCallback callback,
                                  gpointer data) {
    return graptos_flat_button_new(label, tooltip, callback, data);
}

/**
 * @brief Create a tool button from an owned label string.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param label The label supplied by the caller.
 * @param tooltip The tooltip supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param data The callback context passed by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *tool_button_new_owned_label(char *label,
                                              const char *tooltip,
                                              GCallback callback,
                                              gpointer data) {
    GtkWidget *button = tool_button_new(label, tooltip, callback, data);
    g_free(label);
    return button;
}

/**
 * @brief Tool icon button new.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param icon_name The icon name supplied by the caller.
 * @param tooltip The tooltip supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param data The callback context passed by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *tool_icon_button_new(const char *icon_name,
                                       const char *tooltip,
                                       GCallback callback,
                                       gpointer data) {
    GtkWidget *button = gtk_button_new();
    GtkWidget *picture = gtk_picture_new();
    gtk_widget_set_size_request(picture, 16, 16);
    gtk_widget_set_halign(picture, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(picture, GTK_ALIGN_CENTER);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);

    GdkDisplay *display = gdk_display_get_default();
    GtkIconTheme *theme = display ? gtk_icon_theme_get_for_display(display) : NULL;
    GtkIconPaintable *paintable = theme
        ? gtk_icon_theme_lookup_icon(theme,
                                     icon_name ? icon_name : "",
                                     NULL,
                                     16,
                                     1,
                                     GTK_TEXT_DIR_NONE,
                                     GTK_ICON_LOOKUP_PRELOAD)
        : NULL;
    if (paintable) {
        gtk_picture_set_paintable(GTK_PICTURE(picture),
                                  GDK_PAINTABLE(paintable));
        g_object_unref(paintable);
    }

    gtk_button_set_child(GTK_BUTTON(button), picture);
    gtk_widget_add_css_class(button, "graptos-flat");
    gtk_widget_add_css_class(button, "graptos-icon-tool");
    gtk_widget_set_tooltip_text(button, tooltip ? tooltip : "");
    if (callback) g_signal_connect(button, "clicked", callback, data);
    return button;
}

/**
 * @brief Action toggle codex panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_codex_panel(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (win) codex_panel_toggle(win->codex_panel);
}

/**
 * @brief Action toggle terminal panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_terminal_panel(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (win) terminal_panel_toggle(win->terminal_panel);
}

/**
 * @brief Action toggle project tree.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
void action_toggle_project_tree(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->project_revealer) return;

    gboolean visible =
        gtk_revealer_get_reveal_child(GTK_REVEALER(win->project_revealer));
    if (visible) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(win->project_revealer),
                                      FALSE);
    } else {
        gtk_widget_set_visible(win->project_revealer, TRUE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(win->project_revealer),
                                      TRUE);
    }
}

/**
 * @brief Return the current tab for a tool command.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static EditorTab *tool_current_tab(EditorWindow *win) {
    return win ? app_window_current_tab(win) : NULL;
}

/**
 * @brief Close the active editor tab from the tool panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_close_tab(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    EditorTab *tab = tool_current_tab(win);
    if (win && tab) app_window_close_tab(win, tab);
}

/**
 * @brief Clear the current project folder tree from the tool panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_close_folder(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win) return;
    project_tree_clear(win);
    app_window_set_status(win, "Project folders closed");
}

/**
 * @brief Request a normal application window close.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_quit(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (win && win->window) gtk_window_close(GTK_WINDOW(win->window));
}

/**
 * @brief Undo in the active editor tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_undo(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = tool_current_tab(user_data);
    if (tab) editor_tab_undo(tab);
}

/**
 * @brief Redo in the active editor tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_redo(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = tool_current_tab(user_data);
    if (tab) editor_tab_redo(tab);
}

/**
 * @brief Cut the active editor selection.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_cut(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = tool_current_tab(user_data);
    if (tab) editor_tab_cut_clipboard(tab);
}

/**
 * @brief Copy the active editor selection.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_copy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = tool_current_tab(user_data);
    if (tab) editor_tab_copy_clipboard(tab);
}

/**
 * @brief Paste clipboard text into the active editor tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_paste(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = tool_current_tab(user_data);
    if (tab) editor_tab_paste_clipboard(tab);
}

/**
 * @brief Select all text in the active editor tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_select_all(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorTab *tab = tool_current_tab(user_data);
    if (tab) editor_tab_select_all(tab);
}

/**
 * @brief Tool title new.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param title The title supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *tool_title_new(const char *title) {
    GtkWidget *label = gtk_label_new(title ? title : "");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "graptos-tool-title");
    return label;
}

/**
 * @brief Tool panel reset widget pointers.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
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
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
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
 * @brief Append a centered child to the tool panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param child The child supplied by the caller.
 */
static void tool_panel_append(EditorWindow *win, GtkWidget *child) {
    if (!win || !win->tool_panel || !child) return;
    gtk_widget_set_valign(child, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(win->tool_panel), child);
}

/**
 * @brief Tool panel begin.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param title The title supplied by the caller.
 */
static void tool_panel_begin(EditorWindow *win, const char *title) {
    if (!win || !win->tool_panel) return;
    tool_panel_clear(win);
    tool_panel_append(win, tool_title_new(title));
    tool_panel_append(win, graptos_separator_new());
}

/**
 * @brief Hide tool panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
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
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
static void tool_panel_show_ready(EditorWindow *win) {
    if (!win || !win->tool_panel || !win->tool_revealer) return;
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    tool_panel_append(win, spacer);
    tool_panel_append(win, tool_button_new("×", "Close option bar", G_CALLBACK(hide_tool_panel), win));
    gtk_revealer_set_reveal_child(GTK_REVEALER(win->tool_revealer), TRUE);
}

/**
 * @brief Tool syntax choice state.
 */
typedef struct {
    EditorWindow *win; /**< Owning editor window. */
    SyntaxDef *syntax; /**< Syntax to apply, or NULL for plain text. */
    GtkWidget *button; /**< Button whose label mirrors the current syntax. */
    GtkWidget *popover; /**< Popover containing syntax choices. */
} ToolSyntaxChoice;

/**
 * @brief Free a tool syntax choice.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param data The callback context passed by the caller.
 */
static void tool_syntax_choice_free(gpointer data) {
    g_free(data);
}

/**
 * @brief Format the current indentation policy for the tool button.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *tool_indent_label(EditorWindow *win) {
    if (!win) return g_strdup("Indent");
    return win->insert_spaces
        ? g_strdup_printf("Indent: %u spaces", win->tab_width)
        : g_strdup("Indent: tabs");
}

/**
 * @brief Format a boolean tool toggle label.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param name The name supplied by the caller.
 * @param active The active supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *tool_toggle_label(const char *name, gboolean active) {
    return g_strdup_printf("%s: %s", name ? name : "Option",
                           active ? "On" : "Off");
}

/**
 * @brief Format the active syntax label for the tool button.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *tool_syntax_label(EditorWindow *win) {
    EditorTab *tab = app_window_current_tab(win);
    const char *name = tab && tab->active_syntax && tab->active_syntax->name
        ? tab->active_syntax->name
        : "Plain Text";
    return g_strdup_printf("Language: %s", name);
}

/**
 * @brief Set a button label from an owned string.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param button The button that emitted the action signal.
 * @param label The label supplied by the caller.
 */
static void tool_button_set_owned_label(GtkWidget *button, char *label) {
    if (button && GTK_IS_BUTTON(button)) {
        gtk_button_set_label(GTK_BUTTON(button), label ? label : "");
    }
    g_free(label);
}

/**
 * @brief Refresh all tab highlighting after toggling YAML style overrides.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
static void tool_refresh_highlighting(EditorWindow *win) {
    if (!win) return;
    guint count = app_window_tab_count(win);
    for (guint i = 0u; i < count; i++) {
        EditorTab *tab = app_window_tab_at(win, i);
        if (tab) {
            editor_tab_update_highlight_engine(tab);
            gtk_widget_queue_draw(tab->text_view);
            if (tab->minimap_view) gtk_widget_queue_draw(tab->minimap_view);
        }
    }
}

/**
 * @brief Cycle through available indentation policies.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_cycle_indent(GtkWidget *widget, gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win) return;

    if (win->insert_spaces && win->tab_width == 2u) {
        set_tab_policy(win, 4u, TRUE);
    } else if (win->insert_spaces && win->tab_width == 4u) {
        set_tab_policy(win, 8u, TRUE);
    } else if (win->insert_spaces && win->tab_width == 8u) {
        set_tab_policy(win, 4u, FALSE);
    } else {
        set_tab_policy(win, 2u, TRUE);
    }
    tool_button_set_owned_label(widget, tool_indent_label(win));
}

/**
 * @brief Toggle syntax highlighting style overrides from a text button.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_toggle_syntax_highlighting(GtkWidget *widget,
                                                   gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win) return;
    win->use_yaml_style_overrides = !win->use_yaml_style_overrides;
    tool_refresh_highlighting(win);
    graptos_config_save(win);
    tool_button_set_owned_label(widget,
                                tool_toggle_label("Syntax Highlighting",
                                                  win->use_yaml_style_overrides));
}

/**
 * @brief Toggle whether the terminal follows active file directories.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_toggle_terminal_mode(GtkWidget *widget,
                                             gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win) return;

    win->terminal_dynamic_directory = !win->terminal_dynamic_directory;
    graptos_config_save(win);
    tool_button_set_owned_label(widget,
                                g_strdup_printf("Terminal Mode: %s",
                                                win->terminal_dynamic_directory
                                                    ? "Dynamic Directory"
                                                    : "Single"));
    terminal_panel_sync_to_active_file(win->terminal_panel);
}

/**
 * @brief Toggle preview from a text button.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_toggle_preview(GtkWidget *widget, gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win) return;
    win->preview_enabled = !win->preview_enabled;
    apply_preferences_to_all_tabs(win);
    graptos_config_save(win);
    tool_button_set_owned_label(widget,
                                tool_toggle_label("Preview",
                                                  win->preview_enabled));
}

/**
 * @brief Toggle minimap from a text button.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_toggle_minimap(GtkWidget *widget, gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win) return;
    win->minimap_enabled = !win->minimap_enabled;
    apply_preferences_to_all_tabs(win);
    graptos_config_save(win);
    tool_button_set_owned_label(widget,
                                tool_toggle_label("Minimap",
                                                  win->minimap_enabled));
}

/**
 * @brief Cancel regex tester state in every open tab.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
static void tool_cancel_regex_testers(EditorWindow *win) {
    if (!win) return;

    guint count = app_window_tab_count(win);
    for (guint i = 0u; i < count; i++) {
        EditorTab *tab = app_window_tab_at(win, i);
        if (!tab) continue;

        graptos_source_cancel(&tab->regex_tester_timeout);
        if (tab->regex_tester_active) hide_hover_preview(tab);
    }
}

/**
 * @brief Toggle regex tester from the Code tool menu.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_toggle_regex_tester(GtkWidget *widget,
                                            gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win) return;

    win->regex_tester_enabled = !win->regex_tester_enabled;
    if (!win->regex_tester_enabled) tool_cancel_regex_testers(win);
    graptos_config_save(win);
    tool_button_set_owned_label(widget,
                                tool_toggle_label("Regex Tester",
                                                  win->regex_tester_enabled));
}

/**
 * @brief Toggle Auto Save from the File tool menu and refresh its label.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_toggle_autosave(GtkWidget *widget,
                                        gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win) return;
    action_toggle_autosave(widget, user_data);
    tool_button_set_owned_label(widget,
                                tool_toggle_label("Auto Save",
                                                  win->auto_save_enabled));
}

/**
 * @brief Close and unparent a transient tool popover.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param popover The popover supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void tool_popover_closed(GtkPopover *popover, gpointer user_data) {
    (void)user_data;
    if (popover && gtk_widget_get_parent(GTK_WIDGET(popover))) {
        gtk_widget_unparent(GTK_WIDGET(popover));
    }
}

/**
 * @brief Apply a syntax choice from the language popover.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param box The box supplied by the caller.
 * @param row The row supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void tool_syntax_choice_activated(GtkListBox *box,
                                         GtkListBoxRow *row,
                                         gpointer user_data) {
    (void)box;
    (void)user_data;
    ToolSyntaxChoice *choice = row
        ? g_object_get_data(G_OBJECT(row), "graptos-tool-syntax-choice")
        : NULL;
    if (!choice || !choice->win) return;

    EditorTab *tab = app_window_current_tab(choice->win);
    if (!tab) return;

    codex_panel_refresh_context(choice->win->codex_panel);
    editor_tab_set_syntax(tab, choice->syntax, TRUE);
    editor_tab_update_title(tab);
    editor_tab_update_status(tab);
    tool_button_set_owned_label(choice->button, tool_syntax_label(choice->win));
    if (choice->popover) gtk_popover_popdown(GTK_POPOVER(choice->popover));
}

/**
 * @brief Add a syntax choice row to a language popover list.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param list The list supplied by the caller.
 * @param win The win supplied by the caller.
 * @param button The button that emitted the action signal.
 * @param popover The popover supplied by the caller.
 * @param label_text The label text supplied by the caller.
 * @param syntax The syntax definition used by the editor path.
 */
static void tool_syntax_add_row(GtkWidget *list,
                                EditorWindow *win,
                                GtkWidget *button,
                                GtkWidget *popover,
                                const char *label_text,
                                SyntaxDef *syntax) {
    if (!list || !win || !label_text) return;

    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *label = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    graptos_set_all_margins(label, 6);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);

    ToolSyntaxChoice *choice = g_new0(ToolSyntaxChoice, 1);
    choice->win = win;
    choice->syntax = syntax;
    choice->button = button;
    choice->popover = popover;
    g_object_set_data_full(G_OBJECT(row), "graptos-tool-syntax-choice",
                           choice, tool_syntax_choice_free);
    gtk_list_box_append(GTK_LIST_BOX(list), row);
}

/**
 * @brief Show the language selection popover.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void action_tool_show_syntax_popover(GtkWidget *widget,
                                            gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win || !widget) return;

    GtkWidget *popover = gtk_popover_new();
    graptos_popover_attach(popover, widget);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "graptos-context-popover");

    GtkWidget *list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(list), GTK_SELECTION_NONE);
    tool_syntax_add_row(list, win, widget, popover, "Plain Text", NULL);
    for (guint i = 0u; win->syntaxes && i < win->syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(win->syntaxes, i);
        if (syntax && syntax->name) {
            tool_syntax_add_row(list, win, widget, popover, syntax->name,
                                syntax);
        }
    }

    g_signal_connect(list, "row-activated",
                     G_CALLBACK(tool_syntax_choice_activated), NULL);
    g_signal_connect(popover, "closed", G_CALLBACK(tool_popover_closed), NULL);
    gtk_popover_set_child(GTK_POPOVER(popover), list);
    graptos_popover_show(popover);
}

/**
 * @brief Show file tools.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void show_file_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "File");
    tool_panel_append(win, tool_button_new("New", "New tab (Ctrl+N / Ctrl+T)", G_CALLBACK(action_new), win));
    tool_panel_append(win, tool_button_new("Open", "Open file in new tab (Ctrl+O)", G_CALLBACK(action_open), win));
    tool_panel_append(win, tool_button_new("Open Folder", "Add project folder", G_CALLBACK(action_open_folder), win));
    tool_panel_append(win, tool_button_new("Init Project", "Create a new project from a template", G_CALLBACK(action_init_project), win));
    tool_panel_append(win, tool_button_new("Save", "Save current tab (Ctrl+S)", G_CALLBACK(action_save), win));
    tool_panel_append(win, tool_button_new("Save As", "Save current tab as (Ctrl+Shift+S)", G_CALLBACK(action_save_as), win));
    tool_panel_append(win,
                      tool_button_new_owned_label(tool_toggle_label("Auto Save",
                                                                    win->auto_save_enabled),
                                                  "Toggle automatic background saves",
                                                  G_CALLBACK(action_tool_toggle_autosave),
                                                  win));
    tool_panel_append(win, tool_button_new("Close", "Close the active tab", G_CALLBACK(action_tool_close_tab), win));
    tool_panel_append(win, tool_button_new("Close Folder", "Clear the project tree", G_CALLBACK(action_tool_close_folder), win));
    tool_panel_append(win, tool_button_new("Theme", "Open theme editor", G_CALLBACK(action_show_preferences), win));
    tool_panel_append(win, tool_button_new("About", "Show Graptoς version and build information", G_CALLBACK(action_about), win));
    tool_panel_append(win, tool_button_new("Shortcuts", "Show Graptoς keyboard shortcuts", G_CALLBACK(action_shortcuts), win));
    tool_panel_append(win, tool_button_new("Quit", "Close Graptoς", G_CALLBACK(action_tool_quit), win));
    tool_panel_show_ready(win);
}

/**
 * @brief Show edit tools.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void show_edit_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "Edit");
    tool_panel_append(win, tool_button_new("Undo", "Undo in the active tab", G_CALLBACK(action_tool_undo), win));
    tool_panel_append(win, tool_button_new("Redo", "Redo in the active tab", G_CALLBACK(action_tool_redo), win));
    tool_panel_append(win, tool_button_new("Cut", "Cut selected text", G_CALLBACK(action_tool_cut), win));
    tool_panel_append(win, tool_button_new("Copy", "Copy selected text", G_CALLBACK(action_tool_copy), win));
    tool_panel_append(win, tool_button_new("Paste", "Paste clipboard text", G_CALLBACK(action_tool_paste), win));
    tool_panel_append(win, tool_button_new("Select All", "Select all text", G_CALLBACK(action_tool_select_all), win));
    tool_panel_append(win, tool_button_new("Find", "Open find panel (Ctrl+F)", G_CALLBACK(action_show_find), win));
    tool_panel_append(win, tool_button_new("Replace", "Open replace panel (Ctrl+H)", G_CALLBACK(action_show_replace), win));
    tool_panel_append(win, tool_button_new("Project Search", "Search and replace across the project", G_CALLBACK(action_project_search), win));
    tool_panel_show_ready(win);
}

/**
 * @brief Show code tools.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void show_code_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "Code");
    tool_panel_append(win, tool_button_new_owned_label(tool_indent_label(win),
                                                       "Cycle tab insertion policy",
                                                       G_CALLBACK(action_tool_cycle_indent),
                                                       win));
    tool_panel_append(win, tool_button_new_owned_label(tool_syntax_label(win),
                                                       "Select syntax mode",
                                                       G_CALLBACK(action_tool_show_syntax_popover),
                                                       win));
    tool_panel_append(win,
                      tool_button_new_owned_label(tool_toggle_label("Syntax Highlighting",
                                                                    win->use_yaml_style_overrides),
                                                  "Toggle YAML syntax style overrides",
                                                  G_CALLBACK(action_tool_toggle_syntax_highlighting),
                                                  win));
    tool_panel_append(win,
                      tool_button_new_owned_label(tool_toggle_label("Regex Tester",
                                                                    win->regex_tester_enabled),
                                                  "Toggle selected-regex tester popover",
                                                  G_CALLBACK(action_tool_toggle_regex_tester),
                                                  win));
    tool_panel_append(win, tool_button_new("Diagnostics", "Show loaded syntax definitions", G_CALLBACK(action_syntax_diagnostics), win));
    tool_panel_append(win, tool_button_new("Reload Syntax", "Reload YAML syntax definitions", G_CALLBACK(action_reload_syntax), win));
    update_policy_buttons(win);
    tool_panel_show_ready(win);
}

/**
 * @brief Show AI tools.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void show_ai_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "AI");
    tool_panel_append(win, tool_button_new("Chat", "Toggle Codex panel", G_CALLBACK(action_toggle_codex_panel), win));
    tool_panel_show_ready(win);
}


/**
 * @brief Show git tools.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void show_git_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "Git");
    tool_panel_append(win, tool_button_new("Status", "Show Git status", G_CALLBACK(action_git_status), win));
    tool_panel_append(win, tool_button_new("Stage", "Stage current file", G_CALLBACK(action_git_stage), win));
    tool_panel_append(win, tool_button_new("Unstage", "Unstage current file", G_CALLBACK(action_git_unstage), win));
    tool_panel_append(win, tool_button_new("Stage All", "Stage all repository changes", G_CALLBACK(action_git_stage_all), win));
    tool_panel_append(win, tool_button_new("Commit", "Commit staged changes", G_CALLBACK(action_git_commit), win));
    tool_panel_append(win, tool_button_new("Push", "Push to upstream", G_CALLBACK(action_git_push), win));
    tool_panel_append(win, tool_button_new("Pull", "Pull with --ff-only", G_CALLBACK(action_git_pull), win));
    tool_panel_append(win, tool_button_new("Diff", "Show diff for current file or repository", G_CALLBACK(action_git_diff), win));
    tool_panel_append(win, tool_button_new("Credentials", "Save HTTPS Git credentials via Git credential helper", G_CALLBACK(action_git_credentials), win));
    tool_panel_append(win, tool_button_new("Refresh", "Refresh Git status indicators", G_CALLBACK(action_git_refresh), win));
    tool_panel_show_ready(win);
}

/**
 * @brief Show view tools.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void show_view_tools(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    EditorWindow *win = user_data;
    if (!win || !win->tool_panel) return;
    tool_panel_begin(win, "View");
    tool_panel_append(win, tool_button_new("Project Tree", "Toggle project tree (Ctrl+B)", G_CALLBACK(action_toggle_project_tree), win));
    tool_panel_append(win, tool_button_new("Terminal", "Toggle terminal panel (Ctrl+`)", G_CALLBACK(action_toggle_terminal_panel), win));
    tool_panel_append(win,
                      tool_button_new_owned_label(g_strdup_printf("Terminal Mode: %s",
                                                                  win->terminal_dynamic_directory
                                                                      ? "Dynamic Directory"
                                                                      : "Single"),
                                                  "Switch between one terminal and one terminal per active file directory",
                                                  G_CALLBACK(action_tool_toggle_terminal_mode),
                                                  win));
    tool_panel_append(win,
                      tool_button_new_owned_label(tool_toggle_label("Preview",
                                                                    win->preview_enabled),
                                                  "Toggle Markdown and LaTeX preview",
                                                  G_CALLBACK(action_tool_toggle_preview),
                                                  win));
    tool_panel_append(win,
                      tool_button_new_owned_label(tool_toggle_label("Minimap",
                                                                    win->minimap_enabled),
                                                  "Toggle minimap",
                                                  G_CALLBACK(action_tool_toggle_minimap),
                                                  win));
    tool_panel_show_ready(win);
}

/**
 * @brief Build tool panel.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *build_tool_panel(EditorWindow *win) {
    win->tool_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(win->tool_revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration(GTK_REVEALER(win->tool_revealer), 120u);

    win->tool_panel = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(win->tool_panel, "graptos-tool-panel");
    gtk_widget_set_hexpand(win->tool_panel, TRUE);
    gtk_revealer_set_child(GTK_REVEALER(win->tool_revealer), win->tool_panel);
    gtk_revealer_set_reveal_child(GTK_REVEALER(win->tool_revealer), FALSE);
    return win->tool_revealer;
}

/**
 * @brief On status label clicked.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param gesture The gesture supplied by the caller.
 * @param n_press The n press supplied by the caller.
 * @param x The x supplied by the caller.
 * @param y The y supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
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
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *build_bottom_bar(EditorWindow *win) {
    GtkWidget *bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(bottom, "graptos-bottom");

    win->status_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(win->status_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(win->status_label), PANGO_ELLIPSIZE_END);
    gtk_widget_add_css_class(win->status_label, "graptos-status");
    gtk_widget_set_size_request(win->status_label, 180, -1);
    GtkGesture *status_click = gtk_gesture_click_new();
    g_signal_connect(status_click, "pressed",
                     G_CALLBACK(on_status_label_clicked), win);
    gtk_widget_add_controller(win->status_label,
                              GTK_EVENT_CONTROLLER(status_click));
    gtk_box_append(GTK_BOX(bottom), win->status_label);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(bottom), spacer);

    gtk_box_append(GTK_BOX(bottom), tool_icon_button_new("document-open-symbolic", "Show file commands", G_CALLBACK(show_file_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_icon_button_new("document-edit-symbolic", "Show edit commands", G_CALLBACK(show_edit_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_icon_button_new("code-context-symbolic", "Show code commands", G_CALLBACK(show_code_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_icon_button_new("applications-science-symbolic", "Show AI commands", G_CALLBACK(show_ai_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_icon_button_new("vcs-branch-symbolic", "Show Git commands", G_CALLBACK(show_git_tools), win));
    gtk_box_append(GTK_BOX(bottom), tool_icon_button_new("view-grid-symbolic", "Show view commands", G_CALLBACK(show_view_tools), win));

    return bottom;
}
