/**
 * @file src/app_state.c
 * @brief Application preference, policy, and timer state helpers.
 */

#include "app_private.h"

#include <string.h>


/**
 * @brief App window apply css.
 */
void app_window_apply_css(EditorWindow *win) {
    if (!win) return;
    cleaf_apply_editor_css(win->editor_bg_color, win->editor_fg_color,
                           win->editor_gutter_bg_color,
                           win->editor_gutter_fg_color,
                           win->editor_current_line_bg_color,
                           win->editor_selection_bg_color,
                           win->editor_selection_fg_color,
                           win->editor_cursor_color,
                           win->sidebar_bg_color, win->tabbar_bg_color,
                           win->tabbar_fg_color,
                           win->tab_active_bg_color,
                           win->tab_active_fg_color,
                           win->topbar_bg_color, win->topbar_fg_color,
                           win->bottombar_bg_color, win->bottombar_fg_color,
                           win->button_bg_color, win->button_fg_color,
                           win->button_hover_bg_color,
                           win->button_active_bg_color,
                           win->input_bg_color, win->input_fg_color,
                           win->input_border_color,
                           win->project_tree_fg_color,
                           win->project_tree_selected_bg_color,
                           win->project_tree_selected_fg_color,
                           win->scroll_preview_bg_color,
                           win->scroll_preview_fg_color,
                           win->popover_bg_color,
                           win->ref_popover_bg_color, win->ref_popover_fg_color,
                           win->ref_popover_heading_color,
                           win->ref_popover_title_color,
                           win->ref_popover_kind_color,
                           win->ref_popover_snippet_color,
                           win->completion_popover_bg_color,
                           win->completion_popover_fg_color,
                           win->completion_popover_detail_color,
                           win->completion_selection_bg_color,
                           win->completion_selection_fg_color,
                           win->dialog_bg_color, win->dialog_fg_color,
                           win->dialog_border_color,
                           win->dialog_title_color,
                           win->search_match_bg_color,
                           win->search_match_fg_color,
                           win->diagnostic_warning_bg_color,
                           win->diagnostic_warning_fg_color,
                           win->codex_preview_bg_color,
                           win->codex_preview_fg_color,
                           win->codex_prompt_bg_color,
                           win->use_system_interface_font);
}

/**
 * @brief Set switch active safely.
 */
static void set_switch_active_safely(GtkWidget *widget, gboolean active) {
    if (!widget || !GTK_IS_SWITCH(widget)) return;
    gtk_switch_set_active(GTK_SWITCH(widget), active);
}

/**
 * @brief Update indent dropdown.
 */
static void update_indent_dropdown(EditorWindow *win) {
    if (!win || !win->indent_dropdown || !GTK_IS_DROP_DOWN(win->indent_dropdown)) return;

    guint selected = 1u; /* 4 spaces */
    if (!win->insert_spaces) selected = 3u;
    else if (win->tab_width == 2u) selected = 0u;
    else if (win->tab_width == 8u) selected = 2u;
    gtk_drop_down_set_selected(GTK_DROP_DOWN(win->indent_dropdown), selected);
}

/**
 * @brief Update policy buttons.
 */
void update_policy_buttons(EditorWindow *win) {
    if (!win) return;

    win->controls_updating = TRUE;

    if (win->indent_status_label) {
        char *label = win->insert_spaces ?
            g_strdup_printf("Indent: %u spaces", win->tab_width) :
            g_strdup("Indent: hard tabs");
        gtk_label_set_text(GTK_LABEL(win->indent_status_label), label);
        g_free(label);
    }

    update_indent_dropdown(win);
    set_switch_active_safely(win->autocomplete_button, win->autocomplete_enabled);
    set_switch_active_safely(win->autosave_button, win->auto_save_enabled);
    set_switch_active_safely(win->backup_button, win->backup_enabled);
    set_switch_active_safely(win->minimap_button, win->minimap_enabled);
    set_switch_active_safely(win->preview_button, win->preview_enabled);
    set_switch_active_safely(win->syntax_override_button, win->use_yaml_style_overrides);

    win->controls_updating = FALSE;
}


/**
 * @brief Apply tab policy to all tabs.
 */
void apply_tab_policy_to_all_tabs(EditorWindow *win) {
    if (!win || !win->notebook) return;
    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        EditorTab *tab = child ? g_object_get_data(G_OBJECT(child), "cleaf-tab") : NULL;
        if (tab) {
            editor_tab_set_tab_policy(tab, win->tab_width, win->insert_spaces);
            tab->autocomplete_enabled = win->autocomplete_enabled;
            editor_tab_set_backup_enabled(tab, win->backup_enabled);
            if (!win->autocomplete_enabled) editor_tab_hide_completion(tab);
        }
    }
    update_policy_buttons(win);
}


/**
 * @brief Apply preferences to all tabs.
 */
void apply_preferences_to_all_tabs(EditorWindow *win) {
    if (!win || !win->notebook) return;
    app_window_apply_css(win);
    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        EditorTab *tab = child ? g_object_get_data(G_OBJECT(child), "cleaf-tab") : NULL;
        if (tab) {
            editor_tab_apply_preferences(tab);
            editor_tab_update_highlight_engine(tab);
            editor_tab_set_backup_enabled(tab, win->backup_enabled);
            editor_tab_set_minimap_visible(tab, win->minimap_enabled);
            editor_tab_set_preview_visible(tab, win->preview_enabled);
        }
    }
    update_policy_buttons(win);
}


/**
 * @brief Auto save timeout cb.
 */
gboolean auto_save_timeout_cb(gpointer user_data) {
    EditorWindow *win = user_data;
    if (!win || !win->auto_save_enabled || !win->notebook) return G_SOURCE_CONTINUE;
    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    guint saved = 0u;
    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->notebook), i);
        EditorTab *tab = child ? g_object_get_data(G_OBJECT(child), "cleaf-tab") : NULL;
        if (tab && tab->modified && editor_tab_auto_save(tab)) saved++;
    }
    if (saved > 0u) {
        char *msg = g_strdup_printf("Auto-saved %u document%s", saved, saved == 1u ? "" : "s");
        app_window_set_status(win, msg);
        g_free(msg);
    }
    return G_SOURCE_CONTINUE;
}


/**
 * @brief Restart auto save timer.
 */
void restart_auto_save_timer(EditorWindow *win) {
    if (!win) return;
    if (win->auto_save_timeout != 0u) {
        cleaf_source_cancel(&win->auto_save_timeout);
    }
    if (win->auto_save_enabled) win->auto_save_timeout = g_timeout_add_seconds(30u, auto_save_timeout_cb, win);
    update_policy_buttons(win);
}
