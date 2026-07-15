/**
 * @file src/main.c
 * @brief Cleaf application entry point.
 */

#include "app.h"
#include "project.h"

#include <string.h>

/**
 * @brief Cleaf sanitize gtk im module.
 */
static void cleaf_sanitize_gtk_im_module(void) {
    const char *keep = g_getenv("CLEAF_KEEP_GTK_IM_MODULE");
    const char *module = g_getenv("GTK_IM_MODULE");

    if (keep && keep[0] != '\0') return;
    if (g_strcmp0(module, "xim") == 0) g_unsetenv("GTK_IM_MODULE");
}

/**
 * @brief Cleaf enable debug logging.
 */
static void cleaf_enable_debug_logging(void) {
#ifdef CLEAF_DEBUG
    if (!g_getenv("G_MESSAGES_DEBUG")) g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
    g_message("Cleaf debug mode enabled");
#else
    const char *debug = g_getenv("CLEAF_DEBUG");
    if (debug && debug[0] != '\0' && !g_getenv("G_MESSAGES_DEBUG")) {
        g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
        g_message("Cleaf runtime debug logging enabled");
    }
#endif
}

/**
 * @brief Open path.
 */
static void open_path(EditorWindow *win, const char *path) {
    if (!win || !path || path[0] == '\0') return;

    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
        project_tree_load_folder(win, path);
        if (win->project_revealer) {
            gtk_revealer_set_reveal_child(GTK_REVEALER(win->project_revealer),
                                          TRUE);
        }
    } else if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        (void)app_window_open_file(win, path);
    }
}

/**
 * @brief Open files.
 */
static void open_files(EditorWindow *win, GFile **files, gint n_files) {
    if (!win || !files || n_files <= 0) return;

    for (gint i = 0; i < n_files; i++) {
        char *path = g_file_get_path(files[i]);
        if (path) open_path(win, path);
        g_free(path);
    }
}

/**
 * @brief On activate.
 */
static void on_activate(GtkApplication *application, gpointer user_data) {
    (void)user_data;
    (void)app_window_new(application);
}

/**
 * @brief On open.
 */
static void on_open(GtkApplication *application,
                    GFile **files,
                    gint n_files,
                    const char *hint,
                    gpointer user_data) {
    (void)hint;
    (void)user_data;
    EditorWindow *win = app_window_new(application);
    open_files(win, files, n_files);
}

/**
 * @brief Main.
 */
int main(int argc, char **argv) {
    cleaf_sanitize_gtk_im_module();
    cleaf_enable_debug_logging();

#if GLIB_CHECK_VERSION(2, 74, 0)
    GApplicationFlags flags = G_APPLICATION_DEFAULT_FLAGS;
#else
    GApplicationFlags flags = G_APPLICATION_FLAGS_NONE;
#endif
    flags |= G_APPLICATION_NON_UNIQUE;
    flags |= G_APPLICATION_HANDLES_OPEN;

    GtkApplication *application = gtk_application_new(CLEAF_APP_ID, flags);
    g_signal_connect(application, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(application, "open", G_CALLBACK(on_open), NULL);
    int status = g_application_run(G_APPLICATION(application), argc, argv);
    g_object_unref(application);
    return status;
}
