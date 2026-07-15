/**
 * @file src/editor_tab_io.c
 * @brief Cleaf editor tab io module.
 */

#include "editor_tab_private.h"
#include "git.h"

#include <glib/gstdio.h>
#include <time.h>

/**
 * @brief Write all fd.
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
 */
gboolean write_text_atomic(const char *path, const char *text, GError **error) {
    if (!path || path[0] == '\0' || !text) return FALSE;

    char *dir = g_path_get_dirname(path);
    if (!dir) {
        g_free(dir);
        return FALSE;
    }

    struct stat st;
    mode_t mode = 0644u;
    if (stat(path, &st) == 0) mode = (mode_t)(st.st_mode & 0777u);

    gboolean ok = FALSE;
    char *tmp_path = NULL;
    int fd = -1;
    for (guint attempt = 0u; attempt < 100u; attempt++) {
        g_free(tmp_path);
        tmp_path = g_strdup_printf("%s/.cleaf-save-%ld-%u.tmp", dir, (long)getpid(), attempt);
        fd = open(tmp_path, O_CREAT | O_EXCL | O_WRONLY, mode);
        if (fd >= 0) break;
        if (errno != EEXIST) break;
    }

    if (fd < 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno), "Could not create temporary save file: %s", g_strerror(errno));
        g_free(tmp_path);
        g_free(dir);
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

    g_free(tmp_path);
    g_free(dir);
    return ok;
}


/**
 * @brief Autosave path for tab.
 */
char *autosave_path_for_tab(EditorTab *tab) {
    if (!tab) return NULL;
    const char *cache = g_get_user_cache_dir();
    if (!cache) return NULL;
    char *dir = g_build_filename(cache, "cleaf", "autosave", NULL);
    if (g_mkdir_with_parents(dir, 0700) != 0) {
        g_free(dir);
        return NULL;
    }
    char *name = NULL;
    if (tab->file_path) {
        guint hash = g_str_hash(tab->file_path);
        name = g_strdup_printf("%08x.autosave", hash);
    } else {
        name = g_strdup_printf("untitled-%p.autosave", (void *)tab);
    }
    char *path = g_build_filename(dir, name, NULL);
    g_free(name);
    g_free(dir);
    return path;
}



/**
 * @brief Cleanup legacy tilde backup.
 */
static void cleanup_legacy_tilde_backup(const char *path) {
    if (!path || path[0] == '\0') return;
    char *legacy = g_strdup_printf("%s~", path);
    if (legacy && g_file_test(legacy, G_FILE_TEST_IS_REGULAR)) {
        (void)g_unlink(legacy);
    }
    g_free(legacy);
}

/**
 * @brief Editor tab load file.
 */
gboolean editor_tab_load_file(EditorTab *tab, const char *path) {
    if (!tab || !path) return FALSE;
    char *contents = NULL;
    gsize length = 0;
    GError *error = NULL;
    if (!g_file_get_contents(path, &contents, &length, &error)) {
        dialog_error(app_window_gtk(tab->win), "Could not open file", error ? error->message : path);
        g_clear_error(&error);
        return FALSE;
    }
    if (length > (gsize)G_MAXINT) {
        g_free(contents);
        dialog_error(app_window_gtk(tab->win), "File too large", "This build refuses to load files larger than GTK's text-buffer insertion limit.");
        return FALSE;
    }
    if (!g_utf8_validate(contents, (gssize)length, NULL)) {
        g_free(contents);
        dialog_error(app_window_gtk(tab->win), "Unsupported encoding", "Cleaf currently opens UTF-8 text files only. Convert the file to UTF-8 first.");
        return FALSE;
    }

    tab->applying_change = TRUE;
    gtk_text_buffer_set_text(tab->buffer, contents, (gint)length);
    tab->applying_change = FALSE;
    g_free(contents);
    g_free(tab->file_path);
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
    editor_tab_update_title(tab);
    editor_tab_update_status(tab);
    app_window_update_ui(tab->win);
    cleaf_git_refresh_and_rebuild(tab->win);
    return TRUE;
}


/**
 * @brief Write backup if needed.
 */
gboolean write_backup_if_needed(EditorTab *tab, const char *path) {
    if (!tab || !tab->backup_enabled || !path || !g_file_test(path, G_FILE_TEST_EXISTS)) return TRUE;

    char *old = NULL;
    gsize len = 0;
    GError *error = NULL;
    if (!g_file_get_contents(path, &old, &len, &error)) {
        g_clear_error(&error);
        return TRUE;
    }

    char *dir = g_path_get_dirname(path);
    char *base = g_path_get_basename(path);
    char *backup_dir = g_build_filename(dir, ".cleaf-backups", NULL);
    if (g_mkdir_with_parents(backup_dir, 0700) != 0) {
        char *msg = g_strdup_printf("Could not create backup folder %s: %s", backup_dir, g_strerror(errno));
        dialog_error(app_window_gtk(tab->win), "Backup failed", msg);
        g_free(msg);
        g_free(backup_dir);
        g_free(base);
        g_free(dir);
        g_free(old);
        return FALSE;
    }

    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *tmv = localtime_r(&now, &tm_buf);
    char stamp[32];
    if (tmv) strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", tmv);
    else g_snprintf(stamp, sizeof(stamp), "backup");

    char *backup_name = g_strdup_printf("%s.%s~", base ? base : "file", stamp);
    char *backup_path = g_build_filename(backup_dir, backup_name, NULL);
    (void)len;
    gboolean ok = write_text_atomic(backup_path, old, &error);
    if (!ok) {
        char *msg = g_strdup_printf("Could not write backup %s: %s", backup_path, error ? error->message : "unknown error");
        dialog_error(app_window_gtk(tab->win), "Backup failed", msg);
        g_free(msg);
        g_clear_error(&error);
    }

    g_free(backup_path);
    g_free(backup_name);
    g_free(backup_dir);
    g_free(base);
    g_free(dir);
    g_free(old);
    return ok;
}


/**
 * @brief Save to path.
 */
gboolean save_to_path(EditorTab *tab, const char *path) {
    if (!tab || !path || path[0] == '\0') return FALSE;

    /*
     * path may be tab->file_path when Save is used.  Copy it before any
     * operation that can free or replace tab->file_path; otherwise the tab
     * title can be rebuilt from freed memory and appear as mojibake.
     */
    char *stable_path = g_strdup(path);
    if (!stable_path) return FALSE;

    if (!write_backup_if_needed(tab, stable_path)) {
        g_free(stable_path);
        return FALSE;
    }
    char *text = buffer_text(tab);
    GError *error = NULL;
    gboolean ok = write_text_atomic(stable_path, text, &error);
    g_free(text);
    if (!ok) {
        dialog_error(app_window_gtk(tab->win), "Could not save file", error ? error->message : stable_path);
        g_clear_error(&error);
        g_free(stable_path);
        return FALSE;
    }
    g_free(tab->file_path);
    tab->file_path = g_canonicalize_filename(stable_path, NULL);
    g_free(stable_path);
    cleanup_legacy_tilde_backup(tab->file_path);
    tab->modified = FALSE;
    tab->manual_syntax_override = FALSE;
    tab->low_latency_mode_active = FALSE;
    reset_undo_state(tab);
    editor_tab_auto_select_syntax(tab);
    editor_tab_update_title(tab);
    editor_tab_update_status(tab);
    app_window_update_ui(tab->win);
    cleaf_git_refresh_and_rebuild(tab->win);
    return TRUE;
}


/**
 * @brief Editor tab save.
 */
gboolean editor_tab_save(EditorTab *tab, gboolean force_dialog) {
    if (!tab) return FALSE;
    if (tab->locked) {
        dialog_info(app_window_gtk(tab->win), "File is locked",
                    "Unlock this file from the project tree before saving changes.");
        return FALSE;
    }
    if (!force_dialog && tab->file_path) {
        return save_to_path(tab, tab->file_path);
    }

    char *filename = cleaf_save_file_dialog(app_window_gtk(tab->win),
                                            "Save File");
    if (!filename) return FALSE;

    gboolean ok = save_to_path(tab, filename);
    g_free(filename);
    return ok;
}


/**
 * @brief Close dialog button.
 */
static GtkWidget *close_dialog_button(const char *label, int response) {
    return cleaf_flat_button_new(label, NULL,
                                 G_CALLBACK(cleaf_modal_window_respond),
                                 GINT_TO_POINTER(response));
}

/**
 * @brief Editor tab confirm close.
 */
gboolean editor_tab_confirm_close(EditorTab *tab) {
    if (!tab || !tab->modified) return TRUE;

    char *base = editor_tab_basename(tab);
    char *title = g_strdup_printf("Save changes to %s?", base);

    GtkWidget *dialog = gtk_window_new();
    gtk_widget_add_css_class(dialog, "cleaf-window");
    gtk_window_set_title(GTK_WINDOW(dialog), "Unsaved Changes");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 460, 170);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), app_window_gtk(tab->win));

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(box, "cleaf-root");
    cleaf_set_all_margins(box, 14);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    GtkWidget *title_label = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(title_label), 0.0f);
    gtk_widget_add_css_class(title_label, "cleaf-menu-title");
    gtk_box_append(GTK_BOX(box), title_label);

    GtkWidget *detail = gtk_label_new(
        "Unsaved changes will be lost if you discard them.");
    gtk_label_set_xalign(GTK_LABEL(detail), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(detail), TRUE);
    gtk_box_append(GTK_BOX(box), detail);

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(row, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(row),
                   close_dialog_button("Cancel", GTK_RESPONSE_CANCEL));
    gtk_box_append(GTK_BOX(row),
                   close_dialog_button("Discard", GTK_RESPONSE_REJECT));
    gtk_box_append(GTK_BOX(row),
                   close_dialog_button("Save", GTK_RESPONSE_ACCEPT));
    gtk_box_append(GTK_BOX(box), row);

    int response = cleaf_modal_window_run(GTK_WINDOW(dialog),
                                          GTK_RESPONSE_CANCEL);
    cleaf_widget_destroy(dialog);
    g_free(title);
    g_free(base);

    if (response == GTK_RESPONSE_ACCEPT) return editor_tab_save(tab, FALSE);
    if (response == GTK_RESPONSE_REJECT) return TRUE;
    return FALSE;
}


/**
 * @brief Editor tab auto save.
 */
gboolean editor_tab_auto_save(EditorTab *tab) {
    if (!tab || !tab->modified) return TRUE;
    if (tab->file_path) return save_to_path(tab, tab->file_path);

    char *path = autosave_path_for_tab(tab);
    if (!path) return FALSE;
    char *text = buffer_text(tab);
    GError *error = NULL;
    gboolean ok = write_text_atomic(path, text, &error);
    if (!ok) {
        dialog_error(app_window_gtk(tab->win), "Auto-save failed", error ? error->message : path);
        g_clear_error(&error);
    } else if (tab->win) {
        char *status = g_strdup_printf("Auto-saved unsaved buffer to %s", path);
        app_window_set_status(tab->win, status);
        g_free(status);
    }
    g_free(text);
    g_free(path);
    return ok;
}

