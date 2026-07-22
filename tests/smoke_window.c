/**
 * @file smoke_window.c
 * @brief GTK smoke test for the main Graptoς window and basic editor workflow.
 *
 * This test is intended to run under a virtual display in CI.  It constructs a
 * real GtkApplication and EditorWindow, then exercises the file-open, find,
 * duplicate-tab prevention, edit, save, disk persistence, and clean-close
 * paths.
 */

#include <gio/gio.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <string.h>

#include "../src/app.h"

/**
 * @brief Temporary root directory used for smoke-test files and XDG state.
 *
 * The directory is created before gtk_init() so GTK and Graptoς configuration
 * files stay isolated from the developer or CI runner environment.
 */
static char *test_tmp_dir;

/**
 * @brief Processes all currently pending main-context events without blocking.
 *
 * GTK work can be queued during window construction, text-buffer updates, and
 * file actions.  Draining the context keeps assertions synchronized with the
 * effects that are already ready to run.
 */
static void drain_main_context(void) {
    while (g_main_context_iteration(NULL, FALSE)) {
    }
}

/**
 * @brief Returns the full text currently stored in a GtkTextBuffer.
 *
 * @param buffer The text buffer to read.
 * @return Newly allocated buffer contents; free with g_free().
 */
static char *buffer_contents(GtkTextBuffer *buffer) {
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

/**
 * @brief Returns the selected text from a GtkTextBuffer.
 *
 * @param buffer The text buffer to inspect.
 * @return Newly allocated selected text, or NULL when no selection exists.
 */
static char *selected_text(GtkTextBuffer *buffer) {
    GtkTextIter start;
    GtkTextIter end;
    if (!gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
        return NULL;
    }
    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

/**
 * @brief Creates and exports an isolated XDG directory for the test process.
 *
 * @param name Environment variable name, such as XDG_CONFIG_HOME.
 * @param base Temporary root directory that will contain the named directory.
 */
static void set_test_xdg_dir(const char *name, const char *base) {
    char *path = g_build_filename(base, name, NULL);
    g_assert_cmpint(g_mkdir_with_parents(path, 0700), ==, 0);
    g_setenv(name, path, TRUE);
    g_free(path);
}

/**
 * @brief Recursively removes a test-created file or directory tree.
 *
 * @param path File or directory path to remove. NULL is ignored.
 */
static void remove_tree(const char *path) {
    if (!path) return;

    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
        GDir *dir = g_dir_open(path, 0, NULL);
        if (dir) {
            const char *name = NULL;
            while ((name = g_dir_read_name(dir)) != NULL) {
                char *child = g_build_filename(path, name, NULL);
                remove_tree(child);
                g_free(child);
            }
            g_dir_close(dir);
        }
        (void)g_rmdir(path);
    } else {
        (void)g_remove(path);
    }
}

/**
 * @brief Exercises the main Graptoς editor window workflow.
 *
 * The smoke path verifies that a window is created, a file opens into the
 * active tab, find selects expected text, reopening the same file reuses the
 * existing tab, edited buffer content can be saved, the saved bytes reach disk,
 * and the window-owned state can be closed and freed cleanly.
 */
static void test_window_file_find_save_close(void) {
    g_autoptr(GError) error = NULL;
    g_assert_nonnull(test_tmp_dir);

    g_autofree char *file_path = g_build_filename(test_tmp_dir, "sample.txt", NULL);
    const char *initial = "alpha beta gamma\nsecond line\n";
    g_assert_true(g_file_set_contents(file_path, initial, -1, &error));
    g_assert_no_error(error);

    GtkApplication *app = gtk_application_new("io.github.graptos.Editor.Smoke",
                                              G_APPLICATION_NON_UNIQUE);
    g_assert_true(g_application_register(G_APPLICATION(app), NULL, &error));
    g_assert_no_error(error);

    EditorWindow *win = app_window_new(app);
    g_assert_nonnull(win);
    g_assert_true(GTK_IS_WINDOW(win->window));
    g_assert_true(GTK_IS_NOTEBOOK(win->notebook));
    drain_main_context();

    app_window_set_error_status(win, "Smoke error", "Full smoke error detail");
    g_assert_cmpstr(win->status_error_title, ==, "Smoke error");
    g_assert_cmpstr(win->status_error_detail, ==, "Full smoke error detail");
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(win->status_label)), ==, "Smoke error");
    app_window_set_status(win, "Smoke normal status");
    g_assert_null(win->status_error_title);
    g_assert_null(win->status_error_detail);
    g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(win->status_label)), ==, "Smoke normal status");

    gint initial_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    g_assert_cmpint(initial_pages, ==, 1);

    g_assert_true(app_window_open_file(win, file_path));
    drain_main_context();

    gint open_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook));
    g_assert_cmpint(open_pages, ==, initial_pages + 1);

    EditorTab *tab = app_window_current_tab(win);
    g_assert_nonnull(tab);
    g_assert_cmpstr(tab->file_path, ==, file_path);

    g_autofree char *loaded = buffer_contents(tab->buffer);
    g_assert_cmpstr(loaded, ==, initial);

    editor_tab_find(tab, "beta", FALSE);
    drain_main_context();

    g_autofree char *selection = selected_text(tab->buffer);
    g_assert_cmpstr(selection, ==, "beta");

    g_assert_true(app_window_open_file(win, file_path));
    drain_main_context();
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook)), ==, open_pages);
    g_assert_true(app_window_current_tab(win) == tab);

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(tab->buffer, &end);
    gtk_text_buffer_insert(tab->buffer, &end, "smoke edit\n", -1);
    drain_main_context();
    g_assert_true(tab->modified);

    g_assert_true(editor_tab_save(tab, FALSE));
    drain_main_context();
    g_assert_false(tab->modified);

    g_autofree char *saved = NULL;
    gsize saved_len = 0;
    g_assert_true(g_file_get_contents(file_path, &saved, &saved_len, &error));
    g_assert_no_error(error);
    g_assert_nonnull(strstr(saved, "smoke edit\n"));

    g_assert_true(app_window_close_all_tabs(win));
    g_assert_cmpint(gtk_notebook_get_n_pages(GTK_NOTEBOOK(win->notebook)), ==, 0);
    gtk_window_destroy(GTK_WINDOW(win->window));
    app_window_free(win);
    drain_main_context();

    g_object_unref(app);
}

/**
 * @brief Initializes isolated test state, registers the smoke test, and runs it.
 *
 * @param argc Command-line argument count supplied by the test runner.
 * @param argv Command-line argument vector supplied by the test runner.
 * @return The GLib test runner exit status.
 */
int main(int argc, char **argv) {
    g_setenv("GRAPTOS_TEST_MODE", "1", TRUE);

    g_autoptr(GError) error = NULL;
    test_tmp_dir = g_dir_make_tmp("graptos-smoke-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(test_tmp_dir);
    set_test_xdg_dir("XDG_CONFIG_HOME", test_tmp_dir);
    set_test_xdg_dir("XDG_CACHE_HOME", test_tmp_dir);
    set_test_xdg_dir("XDG_DATA_HOME", test_tmp_dir);

    g_test_init(&argc, &argv, NULL);
    gtk_init();
    g_test_add_func("/smoke/window-file-find-save-close",
                    test_window_file_find_save_close);
    int result = g_test_run();
    remove_tree(test_tmp_dir);
    g_free(test_tmp_dir);
    return result;
}
