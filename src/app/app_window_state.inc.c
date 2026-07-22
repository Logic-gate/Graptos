/**
 * @file src/app/app_window_state.inc.c
 * @brief Graptoς app window state module.
 * @details The app window is the meeting point for most features. We keep that coordination
 *          here so editor tabs, project state, dialogs, terminals, Git, and AI do not all
 *          learn about each other directly.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */

GtkWindow *app_window_gtk(EditorWindow *win) {
    // Some helpers accept NULL parents, so keep this wrapper safe.
    return win && win->window ? GTK_WINDOW(win->window) : NULL;
}


/**
 * @brief App window set status.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param text The text fragment supplied by the caller.
 */
void app_window_set_status(EditorWindow *win, const char *text) {
    if (!win || !win->status_label) return;

    app_window_clear_error_status(win);
    // Empty string keeps the status label valid without showing stale text.
    gtk_label_set_text(GTK_LABEL(win->status_label), text ? text : "");
}

/**
 * @brief App window clear error status.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_clear_error_status(EditorWindow *win) {
    if (!win) return;
    g_clear_pointer(&win->status_error_title, g_free);
    g_clear_pointer(&win->status_error_detail, g_free);
    if (win->status_label) {
        gtk_widget_remove_css_class(win->status_label, "graptos-status-error");
        gtk_widget_set_tooltip_text(win->status_label, NULL);
    }
}

/**
 * @brief App window set error status.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param short_text The short text supplied by the caller.
 * @param detail The detail supplied by the caller.
 */
void app_window_set_error_status(EditorWindow *win,
                                 const char *short_text,
                                 const char *detail) {
    if (!win || !win->status_label) return;

    g_clear_pointer(&win->status_error_title, g_free);
    g_clear_pointer(&win->status_error_detail, g_free);
    win->status_error_title = g_strdup(short_text && short_text[0] ? short_text : "Error");
    win->status_error_detail = g_strdup(detail && detail[0] ? detail : win->status_error_title);

    gtk_label_set_text(GTK_LABEL(win->status_label), win->status_error_title);
    gtk_widget_add_css_class(win->status_label, "graptos-status-error");
    gtk_widget_set_tooltip_text(win->status_label, "Click to show error details");
}

/**
 * @brief Report an application error through status and optional dialog detail.
 * @details A file operation can fail before the user has any visible tab to
 *          inspect. The status bar keeps the failure reachable after the dialog
 *          closes, while the immediate dialog makes blocking failures explicit.
 * @param win The window that owns the status bar and parent dialog.
 * @param short_text The short error title shown in the status bar and dialog.
 * @param detail The detailed error message shown in tooltips and dialogs.
 * @param show_dialog TRUE to show the error dialog immediately.
 */
void app_window_report_error(EditorWindow *win,
                             const char *short_text,
                             const char *detail,
                             gboolean show_dialog) {
    const char *title = short_text && short_text[0] ? short_text : "Error";
    const char *message = detail && detail[0] ? detail : title;

    app_window_set_error_status(win, title, message);
    if (show_dialog) {
        dialog_error(app_window_gtk(win), title, message);
    }
}

/**
 * @brief App window show status error.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 */
void app_window_show_status_error(EditorWindow *win) {
    if (!win || !win->status_error_detail) return;
    dialog_error(app_window_gtk(win),
                 win->status_error_title ? win->status_error_title : "Error",
                 win->status_error_detail);
}


/**
 * @brief Canonical or dup.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *canonical_or_dup(const char *path) {
    if (!path || path[0] == '\0') return NULL;

    /*
     * Prefer canonical paths for comparisons. Fall back to a duplicate so
     * callers still get an owned string if canonicalization fails.
     */
    char *canonical = g_canonicalize_filename(path, NULL);
    return canonical ? canonical : g_strdup(path);
}


/**
 * @brief App window is file locked.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean app_window_is_file_locked(EditorWindow *win, const char *path) {
    if (!win || !win->locked_paths || !path) return FALSE;

    // Locked paths are stored canonicalized so project-tree and tab paths match.
    char *canonical = canonical_or_dup(path);
    gboolean locked = canonical
        && g_hash_table_contains(win->locked_paths, canonical);

    g_free(canonical);
    return locked;
}


/**
 * @brief App window set file locked.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 * @param locked The locked supplied by the caller.
 */
void app_window_set_file_locked(EditorWindow *win,
                                const char *path,
                                gboolean locked) {
    if (!win || !path) return;

    /*
     * Create the lock table lazily because most sessions will never lock files.
     * Keys are owned by the table.
     */
    if (!win->locked_paths) {
        win->locked_paths = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                  g_free, NULL);
    }

    char *canonical = canonical_or_dup(path);
    if (!canonical) return;

    if (locked) {
        g_hash_table_replace(win->locked_paths, g_strdup(canonical),
                             GINT_TO_POINTER(TRUE));
    } else {
        g_hash_table_remove(win->locked_paths, canonical);
    }

    /*
     * Open tabs keep their own locked state so the editor can update UI and
     * editing behavior without checking the hash table every time.
     */
    guint count = app_window_tab_count(win);
    for (guint i = 0u; i < count; i++) {
        EditorTab *tab = app_window_tab_at(win, i);

        if (!tab || !tab->file_path) continue;

        char *tab_path = canonical_or_dup(tab->file_path);
        gboolean same = tab_path && g_strcmp0(tab_path, canonical) == 0;
        g_free(tab_path);

        if (same) editor_tab_set_locked(tab, locked);
    }

    g_free(canonical);
}


/**
 * @brief App window note path renamed.
 * @details Application glue touches actions, tabs, panels, and persistent state. Keeping the contract explicit here makes UI callbacks easier to audit when a later change moves work between the window and child widgets.
 * @param win The win supplied by the caller.
 * @param old_path The old path supplied by the caller.
 * @param new_path The new path supplied by the caller.
 */
void app_window_note_path_renamed(EditorWindow *win,
                                  const char *old_path,
                                  const char *new_path) {
    if (!win || !old_path || !new_path) return;

    /*
     * Rename handling needs canonical paths because the project tree, open tabs,
     * and lock table may have seen the same file through different path forms.
     */
    char *old_canonical = canonical_or_dup(old_path);
    char *new_canonical = canonical_or_dup(new_path);

    if (!old_canonical || !new_canonical) {
        g_free(old_canonical);
        g_free(new_canonical);
        return;
    }

    /*
     * Preserve lock state across renames. Otherwise a locked file becomes
     * editable after being renamed from the project tree.
     */
    gboolean was_locked = app_window_is_file_locked(win, old_canonical);
    if (was_locked) {
        app_window_set_file_locked(win, old_canonical, FALSE);
        app_window_set_file_locked(win, new_canonical, TRUE);
    }

    /*
     * Update open tabs that point to the renamed file or to files inside a
     * renamed folder.
     */
    guint count = app_window_tab_count(win);
    for (guint i = 0u; i < count; i++) {
        EditorTab *tab = app_window_tab_at(win, i);

        if (!tab || !tab->file_path) continue;

        char *tab_canonical = canonical_or_dup(tab->file_path);
        if (!tab_canonical) continue;

        char *replacement = NULL;
        gsize old_len = strlen(old_canonical);

        if (g_strcmp0(tab_canonical, old_canonical) == 0) {
            replacement = g_strdup(new_canonical);
        } else if (g_str_has_prefix(tab_canonical, old_canonical)
                   && tab_canonical[old_len] == G_DIR_SEPARATOR) {
            /*
             * Folder rename case. Keep the relative path below the renamed
             * folder and rebuild it under the new folder path.
             */
            replacement = g_build_filename(new_canonical,
                                           tab_canonical + old_len + 1u,
                                           NULL);
        }

        if (replacement) {
            g_free(tab->file_path);
            tab->file_path = replacement;

            /*
             * A renamed path can change the detected language, so drop manual
             * syntax state and run detection again.
             */
            tab->manual_syntax_override = FALSE;
            editor_tab_auto_select_syntax(tab);

            // Refresh tab state after the path change.
            editor_tab_set_locked(tab,
                                  app_window_is_file_locked(win, tab->file_path));
            editor_tab_update_title(tab);
            editor_tab_update_status(tab);
        }

        g_free(tab_canonical);
    }

    /*
     * Keep the project root list aligned with project-tree renames. The pointer
     * array owns these strings.
     */
    if (win->project_roots) {
        for (guint i = 0u; i < win->project_roots->len; i++) {
            char *root = g_ptr_array_index(win->project_roots, i);

            if (g_strcmp0(root, old_canonical) == 0) {
                g_free(root);
                g_ptr_array_index(win->project_roots, i) = g_strdup(new_canonical);
            }
        }
    }

    // project_root mirrors the active root and must follow exact root renames.
    if (g_strcmp0(win->project_root, old_canonical) == 0) {
        g_free(win->project_root);
        win->project_root = g_strdup(new_canonical);
    }

    g_free(old_canonical);
    g_free(new_canonical);
}
