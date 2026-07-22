/**
 * @file src/main.c
 * @brief Graptoς application entry point.
 * @details GTK startup is more fragile than it looks. We do the environment cleanup,
 *          warning filters, and debug switch setup here because all of that has to happen
 *          before the first window is built. Keeping it up front makes startup boring,
 *          which is exactly what we want.
 */

#include "app.h"
#include "project.h"

#include <execinfo.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

/**
 * @brief Parsed Graptoς command line.
 */
typedef struct {
    int argc; /**< Filtered argument count passed to GApplication. */
    char **argv; /**< Filtered argument vector passed to GApplication. */
    gboolean debug_mode; /**< Whether the `debug` command was requested. */
} GraptosCommandLine;

static volatile sig_atomic_t graptos_crash_handler_active = 0; /**< Re-entry guard for debug crash reports. */

/**
 * @brief Warm up native backtrace support before a crash happens.
 * @details The crash handler is intentionally debug-only, but resolving the
 *          backtrace path once during normal startup avoids making the first
 *          call happen from inside a faulting signal handler.
 */
static void graptos_debug_backtrace_warmup(void) {
    void *frames[1];
    (void)backtrace(frames, (int)G_N_ELEMENTS(frames));
}

/**
 * @brief Print a native crash backtrace in debug mode.
 * @details GTK/GLib criticals often happen before the visible segfault. The
 *          signal handler is intentionally tiny and uses async-signal-safe
 *          writes around backtrace_symbols_fd() so `graptos debug` leaves the
 *          failing native stack in the terminal instead of only saying
 *          "Segmentation fault".
 * @param signal_number The signal that stopped the process.
 */
static void graptos_debug_crash_handler(int signal_number) {
    if (graptos_crash_handler_active) {
        signal(signal_number, SIG_DFL);
        raise(signal_number);
        return;
    }
    graptos_crash_handler_active = 1;

    const char header[] = "\nGraptoς debug crash handler: native backtrace follows\n";
    (void)write(STDERR_FILENO, header, sizeof(header) - 1u);

    void *frames[64];
    int count = backtrace(frames, (int)G_N_ELEMENTS(frames));
    backtrace_symbols_fd(frames, count, STDERR_FILENO);

    const char footer[] = "Graptoς debug crash handler: end backtrace\n";
    (void)write(STDERR_FILENO, footer, sizeof(footer) - 1u);

    signal(signal_number, SIG_DFL);
    raise(signal_number);
}

/**
 * @brief Install debug-only native crash handlers.
 * @details Normal Graptoς runs should keep default crash behavior. The debug
 *          command is explicitly for catching internal failures, so it enables
 *          a short native backtrace for the signals we most often need.
 * @param debug_mode Whether the user launched `graptos debug`.
 */
static void graptos_install_debug_crash_handlers(gboolean debug_mode) {
    if (!debug_mode) return;
    graptos_debug_backtrace_warmup();
    signal(SIGSEGV, graptos_debug_crash_handler);
    signal(SIGABRT, graptos_debug_crash_handler);
    signal(SIGBUS, graptos_debug_crash_handler);
    g_message("Graptoς debug crash handler installed");
}

/**
 * @brief Filter one known VTE deprecated-title warning emitted by the library.
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 * @param log_domain The log domain supplied by the caller.
 * @param log_level The log level supplied by the caller.
 * @param message The message supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void graptos_glib_object_log_handler(const char *log_domain,
                                            GLogLevelFlags log_level,
                                            const char *message,
                                            gpointer user_data) {
    (void)user_data;

    if ((log_level & G_LOG_LEVEL_MASK) == G_LOG_LEVEL_WARNING &&
        g_strcmp0(log_domain, "GLib-GObject") == 0 &&
        message &&
        g_strrstr(message, "VteTerminal::window-title-changed") &&
        g_strrstr(message, "deprecated")) {
        return;
    }

    g_log_default_handler(log_domain, log_level, message, NULL);
}

/**
 * @brief Install runtime log filters for known third-party diagnostics.
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 */
static void graptos_install_log_filters(void) {
    g_log_set_handler("GLib-GObject",
                      G_LOG_LEVEL_WARNING |
                          G_LOG_FLAG_FATAL |
                          G_LOG_FLAG_RECURSION,
                      graptos_glib_object_log_handler,
                      NULL);
}

/**
 * @brief Graptoς sanitize gtk im module.
 * @details Some environments force `GTK_IM_MODULE=xim`, and that has been a
 *          source of broken text input for us. We only clear that one known bad
 *          value, and we leave an escape hatch for debugging because input
 *          methods are very machine-specific.
 */
static void graptos_sanitize_gtk_im_module(void) {
    const char *keep = g_getenv("GRAPTOS_KEEP_GTK_IM_MODULE");
    const char *module = g_getenv("GTK_IM_MODULE");

    if (keep && keep[0] != '\0') return;
    if (g_strcmp0(module, "xim") == 0) g_unsetenv("GTK_IM_MODULE");
}

/**
 * @brief Graptoς enable debug logging.
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 * @param debug_mode The debug mode supplied by the caller.
 */
static void graptos_enable_debug_logging(gboolean debug_mode) {
#ifdef GRAPTOS_DEBUG
    (void)debug_mode;
    if (!g_getenv("G_MESSAGES_DEBUG")) g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
    g_message("Graptoς debug mode enabled");
#else
    const char *debug = g_getenv("GRAPTOS_DEBUG");
    if (debug_mode || (debug && debug[0] != '\0')) {
        if (!g_getenv("G_MESSAGES_DEBUG")) {
            g_setenv("G_MESSAGES_DEBUG", "all", TRUE);
        }
        if (!g_getenv("G_ENABLE_DIAGNOSTIC")) {
            g_setenv("G_ENABLE_DIAGNOSTIC", "1", TRUE);
        }
        if (debug_mode) {
            g_setenv("GRAPTOS_DEBUG", "1", TRUE);
        }
        g_message("Graptoς runtime debug logging enabled");
    }
#endif
}

/**
 * @brief Parse Graptoς command line.
 * @details `cleaf debug` became `graptos debug`, but GTK still needs to see a
 *          normal argv list. We strip the debug command before handing control
 *          to GApplication so GTK does not try to interpret it as a file path.
 * @param argc The argc supplied by the caller.
 * @param argv The argv supplied by the caller.
 * @return The computed value requested by the caller.
 */
static GraptosCommandLine graptos_command_line_parse(int argc, char **argv) {
    GraptosCommandLine command = {0};
    command.argv = g_new0(char *, (gsize)argc + 1u);
    if (!command.argv) return command;

    for (int i = 0; i < argc; i++) {
        if (i == 1 && g_strcmp0(argv[i], "debug") == 0) {
            command.debug_mode = TRUE;
            continue;
        }

        command.argv[command.argc] = g_strdup(argv[i] ? argv[i] : "");
        command.argc++;
    }

    if (command.argc == 0) {
        command.argv[0] = g_strdup("graptos");
        command.argc = 1;
    }
    command.argv[command.argc] = NULL;
    return command;
}

/**
 * @brief Free parsed Graptoς command line.
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 * @param command The command supplied by the caller.
 */
static void graptos_command_line_clear(GraptosCommandLine *command) {
    if (!command || !command->argv) return;
    for (int i = 0; i < command->argc; i++) {
        g_free(command->argv[i]);
    }
    g_free(command->argv);
    command->argv = NULL;
    command->argc = 0;
    command->debug_mode = FALSE;
}

/**
 * @brief Report runtime debug mode.
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 * @param command The command supplied by the caller.
 */
static void graptos_report_debug_mode(const GraptosCommandLine *command) {
    if (!command || !command->debug_mode) return;
    g_message("Graptoς debug command enabled: version=%s pid=%" G_PID_FORMAT,
              APP_VERSION,
              getpid());
    char *cwd = g_get_current_dir();
    if (cwd) {
        g_message("Graptoς debug working directory: %s", cwd);
        g_free(cwd);
    }
}

/**
 * @brief Open path.
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 */
static void open_path(EditorWindow *win, const char *path) {
    if (!win || !path || path[0] == '\0') return;

    if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
        project_tree_load_folder(win, path);
        if (win->project_revealer) {
            gtk_widget_set_visible(win->project_revealer, TRUE);
            gtk_revealer_set_reveal_child(GTK_REVEALER(win->project_revealer),
                                          TRUE);
        }
    } else if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        (void)app_window_open_file(win, path);
    }
}

/**
 * @brief Open files.
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 * @param win The win supplied by the caller.
 * @param files The files supplied by the caller.
 * @param n_files The n files supplied by the caller.
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
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 * @param application The application supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void on_activate(GtkApplication *application, gpointer user_data) {
    (void)user_data;
    (void)app_window_new(application);
}

/**
 * @brief On open.
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 * @param application The application supplied by the caller.
 * @param files The files supplied by the caller.
 * @param n_files The n files supplied by the caller.
 * @param hint The hint supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
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
 * @details Startup code decides which user intent came from the command line before the GTK window exists. The comment keeps debug-mode setup separate from normal application activation.
 * @param argc The argc supplied by the caller.
 * @param argv The argv supplied by the caller.
 * @return The computed value requested by the caller.
 */
int main(int argc, char **argv) {
    GraptosCommandLine command = graptos_command_line_parse(argc, argv);
    graptos_sanitize_gtk_im_module();
    graptos_install_log_filters();
    graptos_enable_debug_logging(command.debug_mode);
    graptos_install_debug_crash_handlers(command.debug_mode);
    graptos_report_debug_mode(&command);

#if GLIB_CHECK_VERSION(2, 74, 0)
    GApplicationFlags flags = G_APPLICATION_DEFAULT_FLAGS;
#else
    GApplicationFlags flags = G_APPLICATION_FLAGS_NONE;
#endif
    flags |= G_APPLICATION_NON_UNIQUE;
    flags |= G_APPLICATION_HANDLES_OPEN;

    GtkApplication *application = gtk_application_new(GRAPTOS_APP_ID, flags);
    g_object_set_data(G_OBJECT(application), "graptos-debug-mode",
                      GINT_TO_POINTER(command.debug_mode));
    g_signal_connect(application, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(application, "open", G_CALLBACK(on_open), NULL);
    int status = g_application_run(G_APPLICATION(application),
                                   command.argc,
                                   command.argv);
    g_object_unref(application);
    graptos_command_line_clear(&command);
    return status;
}
