/**
 * @file src/editor_tab_io.c
 * @brief Graptoς editor tab io module.
 * @details Saving is not just writing bytes. Backups, autosave, disk errors, and title
 *          state all depend on whether the filesystem operation really succeeded, so file
 *          I/O stays separate from normal editing.
 */

#include "editor_tab_private.h"
#include "git.h"

#include <glib/gstdio.h>
#include <time.h>

/**
 * @brief Write all fd.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param fd The fd supplied by the caller.
 * @param data The callback context passed by the caller.
 * @param len The len supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean write_all_fd(int fd, const char *data, gsize len) {
    const char *cursor = data;
    gsize remaining = len;
    while (remaining > 0u) {
        gsize chunk = remaining > (gsize)(1024u * 1024u) ? (gsize)(1024u * 1024u) : remaining;
        ssize_t written = write(fd, cursor, (size_t)chunk);
        if (written < 0) {
            if (errno == EINTR) continue;
            return FALSE;
        }
        if (written == 0) return FALSE;
        cursor += (gsize)written;
        remaining -= (gsize)written;
    }
    return TRUE;
}


/**
 * @brief Write text atomic.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param path The filesystem path supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param error Return location for an optional error; left untouched on success unless GTK sets it.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean write_text_atomic(const char *path, const char *text, GError **error) {
    if (!path || path[0] == '\0' || !text) return FALSE;

    g_autofree char *dir = g_path_get_dirname(path);
    if (!dir) return FALSE;

    struct stat st;
    mode_t mode = 0644u;
    if (stat(path, &st) == 0) mode = (mode_t)(st.st_mode & 0777u);

    gboolean ok = FALSE;
    g_autofree char *tmp_path = NULL;
    int fd = -1;
    for (guint attempt = 0u; attempt < 100u; attempt++) {
        g_free(tmp_path);
        tmp_path = g_strdup_printf("%s/.graptos-save-%ld-%u.tmp", dir, (long)getpid(), attempt);
        fd = open(tmp_path, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (fd >= 0) break;
        if (errno != EEXIST) break;
    }

    if (fd < 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Could not create temporary save file: %s", g_strerror(errno));
        return FALSE;
    }

    gsize len = strlen(text);
    if (!write_all_fd(fd, text, len)) {
        int saved_errno = errno;
        close(fd);
        unlink(tmp_path);
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(saved_errno), "Could not write temporary save file: %s", g_strerror(saved_errno));
    } else if (fsync(fd) != 0) {
        int saved_errno = errno;
        close(fd);
        unlink(tmp_path);
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(saved_errno), "Could not flush temporary save file: %s", g_strerror(saved_errno));
    } else if (close(fd) != 0) {
        int saved_errno = errno;
        unlink(tmp_path);
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(saved_errno), "Could not close temporary save file: %s", g_strerror(saved_errno));
    } else if (rename(tmp_path, path) != 0) {
        int saved_errno = errno;
        unlink(tmp_path);
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(saved_errno), "Could not replace destination file: %s", g_strerror(saved_errno));
    } else {
        ok = TRUE;
    }

    return ok;
}


/**
 * @brief Autosave path for tab.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *autosave_path_for_tab(EditorTab *tab) {
    if (!tab) return NULL;
    const char *cache = g_get_user_cache_dir();
    if (!cache) return NULL;
    g_autofree char *dir = g_build_filename(cache, "graptos", "autosave", NULL);
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        return NULL;
    }
    g_autofree char *name = NULL;
    if (tab->file_path) {
        guint hash = g_str_hash(tab->file_path);
        name = g_strdup_printf("%08x.autosave", hash);
    } else {
        name = g_strdup_printf("untitled-%p.autosave", (void *)tab);
    }
    return g_build_filename(dir, name, NULL);
}



/**
 * @brief Cleanup legacy tilde backup.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param path The filesystem path supplied by the caller.
 */
static void cleanup_legacy_tilde_backup(const char *path) {
    if (!path || path[0] == '\0') return;
    g_autofree char *legacy = g_strdup_printf("%s~", path);
    if (legacy && g_file_test(legacy, G_FILE_TEST_IS_REGULAR)) {
        (void)g_unlink(legacy);
    }
}

/**
 * @brief Return whether the buffer already matches a file on disk.
 * @details Autosave should be quiet when the modified flag is stale but the
 *          actual text already reached disk. Comparing before writing avoids
 *          refreshing timestamps, Git state, LSP saves, and status messages for
 *          a document that has no real pending changes.
 * @param tab The editor tab whose buffer is compared.
 * @param path The filesystem path to read.
 * @return TRUE when disk contents and buffer text are identical.
 */
static gboolean editor_tab_file_matches_buffer(EditorTab *tab,
                                               const char *path) {
    if (!tab || !path || path[0] == '\0') return FALSE;
    g_autofree char *disk = NULL;
    gsize disk_len = 0u;
    if (!g_file_get_contents(path, &disk, &disk_len, NULL)) return FALSE;
    g_autofree char *text = buffer_text(tab);
    if (!text) return FALSE;
    gsize text_len = strlen(text);
    return disk_len == text_len && memcmp(disk, text, text_len) == 0;
}

/**
 * @brief Editor tab load file.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_load_file(EditorTab *tab, const char *path) {
    if (!tab || !path) return FALSE;
    g_autofree char *contents = NULL;
    gsize length = 0;
    g_autoptr(GError) error = NULL;
    if (!g_file_get_contents(path, &contents, &length, &error)) {
        app_window_report_error(tab->win, "Could not open file",
                                error ? error->message : path, TRUE);
        return FALSE;
    }
    if (length > (gsize)G_MAXINT) {
        app_window_report_error(
            tab->win,
            "File too large",
            "This build refuses to load files larger than GTK's text-buffer insertion limit.",
            TRUE);
        return FALSE;
    }
    if (!g_utf8_validate(contents, (gssize)length, NULL)) {
        app_window_report_error(
            tab->win,
            "Unsupported encoding",
            "Graptoς currently opens UTF-8 text files only. Convert the file to UTF-8 first.",
            TRUE);
        return FALSE;
    }

    tab->applying_change = TRUE;
    gtk_text_buffer_set_text(tab->buffer, contents, (gint)length);
    tab->applying_change = FALSE;
    g_free(tab->file_path);
    g_clear_pointer(&tab->display_title, g_free);
    g_clear_pointer(&tab->display_title_markup, g_free);
    tab->file_path = g_canonicalize_filename(path, NULL);
    cleanup_legacy_tilde_backup(tab->file_path);
    tab->modified = FALSE;
    tab->manual_syntax_override = FALSE;
    tab->low_latency_mode_active = FALSE;
    editor_tab_set_tab_policy(tab, tab->tab_width, tab->insert_spaces);
    reset_undo_state(tab);
    update_gutter_width(tab);
    update_minimap_text(tab);
    editor_tab_auto_select_syntax(tab);
    if (tab->win && tab->win->lsp_client) lsp_client_document_opened(tab->win->lsp_client, tab);
    editor_tab_schedule_color_literals(tab);
    editor_tab_update_title(tab);
    editor_tab_update_status(tab);
    app_window_update_ui(tab->win);
    graptos_git_refresh_and_rebuild(tab->win);
    return TRUE;
}


/**
 * @brief Write backup if needed.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean write_backup_if_needed(EditorTab *tab, const char *path) {
    if (!tab || !tab->backup_enabled || !path || !g_file_test(path, G_FILE_TEST_EXISTS)) return TRUE;

    g_autofree char *old = NULL;
    gsize len = 0;
    g_autoptr(GError) error = NULL;
    if (!g_file_get_contents(path, &old, &len, &error)) {
        return TRUE;
    }

    g_autofree char *dir = g_path_get_dirname(path);
    g_autofree char *base = g_path_get_basename(path);
    g_autofree char *backup_dir = g_build_filename(dir, ".graptos-backups", NULL);
    if (g_mkdir_with_parents(backup_dir, 0700) != 0) {
        g_autofree char *msg = g_strdup_printf("Could not create backup folder %s: %s", backup_dir, g_strerror(errno));
        app_window_report_error(tab->win, "Backup failed", msg, TRUE);
        return FALSE;
    }

    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *tmv = localtime_r(&now, &tm_buf);
    char stamp[32];
    if (tmv) strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tmv);
    else g_snprintf(stamp, sizeof(stamp), "backup");

    g_autofree char *backup_name = g_strdup_printf("%s.%s~", base ? base : "file", stamp);
    g_autofree char *backup_path = g_build_filename(backup_dir, backup_name, NULL);
    (void)len;
    gboolean ok = write_text_atomic(backup_path, old, &error);
    if (!ok) {
        g_autofree char *msg = g_strdup_printf("Could not write backup %s: %s", backup_path, error ? error->message : "unknown error");
        app_window_report_error(tab->win, "Backup failed", msg, TRUE);
    }

    return ok;
}


/**
 * @brief Save to path.
 * @details Save updates more than the file on disk. Once the atomic write
 *          succeeds, the tab path, syntax choice, undo baseline, LSP document,
 *          Git status, and window controls all need to agree on the new truth.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param path The filesystem path supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean save_to_path(EditorTab *tab, const char *path) {
    if (!tab || !path || path[0] == '\0') return FALSE;

    /*
     * path may be tab->file_path when Save is used.  Copy it before any
     * operation that can free or replace tab->file_path; otherwise the tab
     * title can be rebuilt from freed memory and appear as mojibake.
     */
    g_autofree char *stable_path = g_strdup(path);
    if (!stable_path) return FALSE;

    if (!write_backup_if_needed(tab, stable_path)) {
        return FALSE;
    }
    g_autofree char *text = buffer_text(tab);
    g_autoptr(GError) error = NULL;
    gboolean ok = write_text_atomic(stable_path, text, &error);
    if (!ok) {
        app_window_report_error(tab->win, "Could not save file",
                                error ? error->message : stable_path, TRUE);
        return FALSE;
    }
    g_free(tab->file_path);
    g_clear_pointer(&tab->display_title, g_free);
    g_clear_pointer(&tab->display_title_markup, g_free);
    tab->file_path = g_canonicalize_filename(stable_path, NULL);
    cleanup_legacy_tilde_backup(tab->file_path);
    tab->modified = FALSE;
    tab->manual_syntax_override = FALSE;
    tab->low_latency_mode_active = FALSE;
    reset_undo_state(tab);
    editor_tab_auto_select_syntax(tab);
    if (tab->win && tab->win->lsp_client) {
        lsp_client_document_opened(tab->win->lsp_client, tab);
        lsp_client_document_saved(tab->win->lsp_client, tab);
    }
    editor_tab_update_title(tab);
    editor_tab_update_status(tab);
    app_window_update_ui(tab->win);
    graptos_git_refresh_and_rebuild(tab->win);
    return TRUE;
}


/**
 * @brief Editor tab save.
 * @details Editor code runs in response to fast input, delayed timeouts, and background language work. The notes here mark the boundary between immediate GTK state and deferred refresh paths so latency fixes do not turn into stale-widget bugs.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param force_dialog The force dialog supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_save(EditorTab *tab, gboolean force_dialog) {
    if (!tab) return FALSE;
    if (tab->locked) {
        dialog_info(app_window_gtk(tab->win), "File is locked",
                    "Unlock this file from the project tree before saving changes.");
        return FALSE;
    }
    if (!force_dialog && tab->file_path) {
        if (!tab->modified) return TRUE;
        return save_to_path(tab, tab->file_path);
    }

    g_autofree char *filename = graptos_save_file_dialog(app_window_gtk(tab->win),
                                                       "Save File");
    if (!filename) return FALSE;

    return save_to_path(tab, filename);
}


/**
 * @brief Editor tab confirm close.
 * @details Closing is the last chance to protect unsaved text. The dialog is
 *          deliberately synchronous because the caller must know immediately
 *          whether it can remove the notebook page and free the tab.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_confirm_close(EditorTab *tab) {
    if (!tab || !tab->modified) return TRUE;

    g_autofree char *base = editor_tab_basename(tab);
    g_autofree char *title = g_strdup_printf("Save changes to %s?", base);
    GraptosDialogAction actions[] = {
        { "Cancel", "Return to editing", GTK_RESPONSE_CANCEL, FALSE, TRUE },
        { "Discard", "Close without saving", GTK_RESPONSE_REJECT, TRUE, FALSE },
        { "Save", "Save and close", GTK_RESPONSE_ACCEPT, FALSE, FALSE },
    };
    int response = dialog_run_message(
        app_window_gtk(tab->win),
        "Unsaved Changes",
        title,
        "Unsaved changes will be lost if you discard them.",
        460,
        170,
        actions,
        G_N_ELEMENTS(actions),
        GTK_RESPONSE_CANCEL);

    if (response == GTK_RESPONSE_ACCEPT) return editor_tab_save(tab, FALSE);
    if (response == GTK_RESPONSE_REJECT) return TRUE;
    return FALSE;
}


/**
 * @brief Editor tab auto save.
 * @details Named files use the normal save path so LSP, Git, and title state
 *          are refreshed. Untitled buffers go to an autosave file instead, so
 *          we do not silently decide a permanent filename for the user.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean editor_tab_auto_save(EditorTab *tab) {
    if (!tab || !tab->modified) return TRUE;
    if (tab->file_path) {
        if (editor_tab_file_matches_buffer(tab, tab->file_path)) {
            tab->modified = FALSE;
            editor_tab_update_title(tab);
            editor_tab_update_status(tab);
            app_window_update_ui(tab->win);
            return TRUE;
        }
        return save_to_path(tab, tab->file_path);
    }

    g_autofree char *path = autosave_path_for_tab(tab);
    if (!path) return FALSE;
    g_autofree char *text = buffer_text(tab);
    g_autoptr(GError) error = NULL;
    gboolean ok = write_text_atomic(path, text, &error);
    if (!ok) {
        app_window_report_error(tab->win, "Auto-save failed",
                                error ? error->message : path, FALSE);
    } else if (tab->win) {
        g_autofree char *status = g_strdup_printf("Auto-saved unsaved buffer to %s", path);
        app_window_set_status(tab->win, status);
    }
    return ok;
}
