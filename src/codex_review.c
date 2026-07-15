/**
 * @file src/codex_review.c
 * @brief Codex diff review tab helpers.
 */

#include "codex_review.h"

#include <gio/gio.h>

#include "app.h"
#include "editor_tab.h"
#include "git.h"

/**
 * @brief Review diff syntax.
 */
static SyntaxDef *review_diff_syntax(EditorWindow *win) {
    if (!win || !win->syntaxes) return NULL;
    for (guint i = 0u; i < win->syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(win->syntaxes, i);
        if (syntax && syntax->name &&
            g_ascii_strcasecmp(syntax->name, "Diff") == 0) return syntax;
    }
    return NULL;
}

/**
 * @brief Codex review open diff.
 */
void codex_review_open_diff(EditorWindow *win, const char *diff) {
    if (!win || !diff) return;
    EditorTab *tab = editor_tab_new(win);
    if (!tab) return;
    tab->applying_change = TRUE;
    gtk_text_buffer_set_text(tab->buffer, diff, -1);
    tab->applying_change = FALSE;
    tab->modified = FALSE;
    tab->locked = TRUE;
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tab->text_view), FALSE);
    SyntaxDef *syntax = review_diff_syntax(win);
    if (syntax) editor_tab_set_syntax(tab, syntax, TRUE);
    app_window_add_tab(win, tab, TRUE);
    if (tab->tab_title) {
        gtk_label_set_text(GTK_LABEL(tab->tab_title), "Codex Turn Diff");
    }
}

/**
 * @brief Run reverse apply.
 */
static gboolean run_reverse_apply(const char *cwd,
                                  const char *diff,
                                  gboolean check,
                                  GError **error) {
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDIN_PIPE |
        G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        G_SUBPROCESS_FLAGS_STDERR_PIPE);
    g_subprocess_launcher_set_cwd(launcher, cwd);
    GSubprocess *process = check
        ? g_subprocess_launcher_spawn(launcher, error, "git", "apply",
                                      "--reverse", "--check", NULL)
        : g_subprocess_launcher_spawn(launcher, error, "git", "apply",
                                      "--reverse", NULL);
    g_object_unref(launcher);
    if (!process) return FALSE;
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    gboolean communicated = g_subprocess_communicate_utf8(process, diff, NULL,
                                                           &stdout_text,
                                                           &stderr_text, error);
    gboolean success = communicated && g_subprocess_get_successful(process);
    if (!success && communicated && error && !*error) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s",
                    stderr_text && stderr_text[0] ? stderr_text
                                                  : "reverse patch failed");
    }
    g_free(stdout_text);
    g_free(stderr_text);
    g_object_unref(process);
    return success;
}

/**
 * @brief Codex review revert.
 */
gboolean codex_review_revert(EditorWindow *win,
                             const char *diff,
                             GError **error) {
    if (!win || !diff || diff[0] == '\0') return FALSE;
    char *fallback = win->project_root ? NULL : g_get_current_dir();
    const char *cwd = win->project_root ? win->project_root : fallback;
    gboolean valid = run_reverse_apply(cwd, diff, TRUE, error);
    gboolean reverted = valid && run_reverse_apply(cwd, diff, FALSE, error);
    g_free(fallback);
    if (reverted) cleaf_git_refresh_and_rebuild(win);
    return reverted;
}
