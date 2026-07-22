/**
 * @file src/terminal_panel.c
 * @brief Integrated VTE terminal panel implementation.
 * @details The terminal is wrapped instead of exposing VTE everywhere. Project-directory
 *          tabs, child cleanup, sizing, and close behavior are Graptoς policy, so the rest
 *          of the app talks to this panel boundary.
 */

#include "terminal_panel.h"

#include "app.h"
#include "ui.h"

#include <signal.h>
#include <vte/vte.h>

/**
 * @brief Default terminal content height in pixels.
 */
#define GRAPTOS_TERMINAL_DEFAULT_HEIGHT 140

/**
 * @brief Minimum terminal content height in pixels.
 */
#define GRAPTOS_TERMINAL_MIN_HEIGHT 80

/**
 * @brief Maximum terminal content height in pixels.
 */
#define GRAPTOS_TERMINAL_MAX_HEIGHT 360

/**
 * @brief Terminal height adjustment step in pixels.
 */
#define GRAPTOS_TERMINAL_HEIGHT_STEP 30

/**
 * @brief Owner responsible for reaping a terminal child process.
 * @details VTE can own the child watch once `vte_terminal_watch_child()` is
 *          called. If a spawn finishes while Graptoς is already disposing the
 *          terminal widget, a small orphan watch owns the reap instead. Keeping
 *          this as an enum prevents accidental double-watch paths.
 */
typedef enum {
    TERMINAL_CHILD_WATCH_NONE, /**< No child watch has been registered. */
    TERMINAL_CHILD_WATCH_VTE, /**< VTE owns the child watch. */
    TERMINAL_CHILD_WATCH_ORPHAN /**< Graptoς owns a minimal orphan watch. */
} TerminalChildWatchOwner;

/**
 * @brief One terminal tab and its child process state.
 * @details A terminal tab is half GTK widget and half child process. We track
 *          spawning, running, closing, and disposed separately because VTE can
 *          still emit callbacks while the UI is already trying to close.
 */
typedef struct _TerminalSession {
    struct _TerminalPanel *panel; /**< Owning terminal panel. */
    GtkWidget *terminal; /**< VTE terminal widget. */
    GtkWidget *tab_widget; /**< Notebook tab label widget. */
    GtkWidget *tab_entry; /**< Editable terminal tab title. */
    char *dir; /**< Canonical working directory for this terminal. */
    char *title; /**< User-visible terminal title. */
    GCancellable *spawn_cancellable; /**< Active shell spawn cancellable. */
    GPid child_pid; /**< Running shell process id. */
    guint close_source; /**< Deferred close completion source. */
    TerminalChildWatchOwner child_watch_owner; /**< Code path responsible for reaping child_pid. */
    gboolean child_running; /**< Whether a shell is active. */
    gboolean spawning; /**< Whether a shell spawn request is active. */
    gboolean closing; /**< Whether the user requested this terminal tab close. */
    gboolean disposed; /**< Whether this session is being released. */
} TerminalSession;

/**
 * @brief Integrated terminal panel state.
 * @details The panel owns the notebook of terminal sessions, but it also needs
 *          a guard for terminal-driven editor switches. Without that guard,
 *          dynamic directory mode can bounce between editor and terminal tabs.
 */
struct _TerminalPanel {
    EditorWindow *win; /**< Owning editor window. */
    GtkWidget *revealer; /**< Revealer wrapping the terminal panel. */
    GtkWidget *box; /**< Terminal panel container. */
    GtkWidget *notebook; /**< Notebook containing terminal sessions. */
    GtkWidget *status_label; /**< Small terminal status label. */
    GtkWidget *height_label; /**< Current terminal height label. */
    GPtrArray *sessions; /**< TerminalSession pointers. */
    int terminal_height; /**< Fixed terminal content height in pixels. */
    gboolean disposed; /**< Whether the owning window is releasing the panel. */
    gboolean switching_editor_from_terminal; /**< Guard for terminal-driven editor switches. */
};

/**
 * @brief Handle terminal notebook page switches.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param notebook The notebook supplied by the caller.
 * @param page The page supplied by the caller.
 * @param page_num The page num supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void terminal_panel_switch_page(GtkNotebook *notebook,
                                       GtkWidget *page,
                                       guint page_num,
                                       gpointer user_data);

/**
 * @brief Close and remove a terminal session tab.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param session The session supplied by the caller.
 */
static void terminal_panel_close_session(TerminalPanel *panel,
                                         TerminalSession *session);

/**
 * @brief Finish removing a terminal tab after its child process exits.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param session The session supplied by the caller.
 */
static void terminal_panel_finish_close_session(TerminalPanel *panel,
                                                TerminalSession *session);

/**
 * @brief Resolve the shell used by the integrated terminal.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *terminal_panel_shell(void) {
    const char *shell = g_getenv("SHELL");
    return (shell && shell[0] != '\0') ? shell : "/bin/sh";
}

/**
 * @brief Reap a terminal child that outlived its VTE widget.
 * @details Closing Graptoς can race an async terminal spawn completion. Once the
 *          terminal widget is being destroyed, VTE must not become the child
 *          watcher; this small watcher only reaps the shell after we ask it to
 *          leave.
 * @param pid The child process id supplied by GLib.
 * @param wait_status The wait status supplied by GLib.
 * @param user_data The callback context, unused.
 */
static void terminal_session_orphan_child_exited(GPid pid,
                                                 gint wait_status,
                                                 gpointer user_data) {
    (void)wait_status;
    (void)user_data;
    g_spawn_close_pid(pid);
}

/**
 * @brief Register the fallback child watcher for a terminal process.
 * @details This path is used only when VTE cannot own the child anymore, most
 *          often because shutdown or a failed close has detached the terminal
 *          widget before the spawn callback returned.
 * @param session The terminal session whose child needs a fallback watcher.
 * @param pid The child process id to reap.
 */
static void terminal_session_watch_orphan_child(TerminalSession *session,
                                                GPid pid) {
    if (!session || pid <= 0 ||
        session->child_watch_owner != TERMINAL_CHILD_WATCH_NONE) {
        return;
    }
    g_child_watch_add(pid, terminal_session_orphan_child_exited, NULL);
    session->child_watch_owner = TERMINAL_CHILD_WATCH_ORPHAN;
}

/**
 * @brief Update the terminal status label.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param text The text fragment supplied by the caller.
 */
static void terminal_panel_set_status(TerminalPanel *panel, const char *text) {
    if (!panel || !panel->status_label) return;
    gtk_label_set_text(GTK_LABEL(panel->status_label), text ? text : "");
}

/**
 * @brief Return the display basename for a terminal directory.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param dir The dir supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *terminal_panel_default_title_for_dir(const char *dir) {
    if (!dir || dir[0] == '\0') return g_strdup("Terminal");
    char *base = g_path_get_basename(dir);
    if (!base || g_strcmp0(base, G_DIR_SEPARATOR_S) == 0 ||
        g_strcmp0(base, ".") == 0) {
        g_free(base);
        return g_strdup(dir);
    }
    return base;
}

/**
 * @brief Return the directory for an editor tab file.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *terminal_panel_dir_for_tab(EditorTab *tab) {
    if (!tab || !tab->file_path || tab->file_path[0] == '\0') return NULL;
    char *dir = g_path_get_dirname(tab->file_path);
    if (!dir) return NULL;
    char *canonical = g_canonicalize_filename(dir, NULL);
    g_free(dir);
    if (canonical && g_file_test(canonical, G_FILE_TEST_IS_DIR)) {
        return canonical;
    }
    g_free(canonical);
    return NULL;
}

/**
 * @brief Resolve the working directory for a new terminal shell.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *terminal_panel_working_dir(TerminalPanel *panel) {
    if (!panel || !panel->win) return g_get_current_dir();

    g_autofree char *tab_dir =
        terminal_panel_dir_for_tab(app_window_current_tab(panel->win));
    if (tab_dir) return g_strdup(tab_dir);

    if (panel->win->project_root &&
        g_file_test(panel->win->project_root, G_FILE_TEST_IS_DIR)) {
        return g_canonicalize_filename(panel->win->project_root, NULL);
    }

    return g_get_current_dir();
}

/**
 * @brief Clamp terminal content height to supported bounds.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param height The height supplied by the caller.
 * @return The computed value requested by the caller.
 */
static int terminal_panel_clamp_height(int height) {
    if (height < GRAPTOS_TERMINAL_MIN_HEIGHT) return GRAPTOS_TERMINAL_MIN_HEIGHT;
    if (height > GRAPTOS_TERMINAL_MAX_HEIGHT) return GRAPTOS_TERMINAL_MAX_HEIGHT;
    return height;
}

/**
 * @brief Return the current terminal session.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static TerminalSession *terminal_panel_current_session(TerminalPanel *panel) {
    if (!panel || !panel->notebook || !panel->sessions) return NULL;
    gint page = gtk_notebook_get_current_page(GTK_NOTEBOOK(panel->notebook));
    if (page < 0 || (guint)page >= panel->sessions->len) return NULL;
    return g_ptr_array_index(panel->sessions, (guint)page);
}

/**
 * @brief Apply the fixed terminal height to a terminal widget.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param terminal The terminal supplied by the caller.
 */
static void terminal_panel_apply_height_to_widget(TerminalPanel *panel,
                                                  GtkWidget *terminal) {
    if (!panel || !terminal) return;
    gtk_widget_set_size_request(terminal, -1, panel->terminal_height);
}

/**
 * @brief Apply the fixed terminal height to the panel widgets.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 */
static void terminal_panel_apply_height(TerminalPanel *panel) {
    if (!panel) return;

    panel->terminal_height =
        terminal_panel_clamp_height(panel->terminal_height);

    if (panel->sessions) {
        for (guint i = 0; i < panel->sessions->len; i++) {
            TerminalSession *session = g_ptr_array_index(panel->sessions, i);
            if (session) {
                terminal_panel_apply_height_to_widget(panel, session->terminal);
            }
        }
    }

    if (panel->height_label) {
        char *label = g_strdup_printf("%dpx", panel->terminal_height);
        gtk_label_set_text(GTK_LABEL(panel->height_label), label);
        g_free(label);
    }
}

/**
 * @brief Apply current editor colors and font to one terminal session.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param session The session supplied by the caller.
 */
static void terminal_session_apply_colors(TerminalSession *session) {
    if (!session || !session->terminal || !session->panel ||
        !session->panel->win) {
        return;
    }

    EditorWindow *win = session->panel->win;
    GdkRGBA fg;
    GdkRGBA bg;
    GdkRGBA cursor;
    const char *fg_text = win->editor_fg_color ? win->editor_fg_color : "#d4d4d4";
    const char *bg_text = win->editor_bg_color ? win->editor_bg_color : "#181a1f";
    const char *cursor_text = win->editor_cursor_color
        ? win->editor_cursor_color : fg_text;

    if (gdk_rgba_parse(&fg, fg_text)) {
        vte_terminal_set_color_foreground(VTE_TERMINAL(session->terminal), &fg);
    }
    if (gdk_rgba_parse(&bg, bg_text)) {
        vte_terminal_set_color_background(VTE_TERMINAL(session->terminal), &bg);
    }
    if (gdk_rgba_parse(&cursor, cursor_text)) {
        vte_terminal_set_color_cursor(VTE_TERMINAL(session->terminal), &cursor);
    }

    if (win->terminal_font && win->terminal_font[0] != '\0') {
        PangoFontDescription *font =
            pango_font_description_from_string(win->terminal_font);
        if (font) {
            vte_terminal_set_font(VTE_TERMINAL(session->terminal), font);
            pango_font_description_free(font);
        }
    } else {
        vte_terminal_set_font(VTE_TERMINAL(session->terminal), NULL);
    }
}

/**
 * @brief Free terminal session memory.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param session The session supplied by the caller.
 */
static void terminal_session_destroy(TerminalSession *session) {
    if (!session) return;
    graptos_source_cancel(&session->close_source);
    g_clear_object(&session->spawn_cancellable);
    g_free(session->dir);
    g_free(session->title);
    g_free(session);
}

/**
 * @brief Mark a terminal session disposed and detach its callbacks.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param session The session supplied by the caller.
 */
static void terminal_session_begin_dispose(TerminalSession *session) {
    if (!session || session->disposed) return;

    session->disposed = TRUE;
    if (session->child_running && session->child_pid > 0) {
        GPid pid = session->child_pid;
        if (!session->terminal &&
            session->child_watch_owner == TERMINAL_CHILD_WATCH_NONE) {
            terminal_session_watch_orphan_child(session, pid);
        }
        (void)kill(pid, SIGHUP);
    }
    if (session->terminal) {
        g_signal_handlers_disconnect_by_data(session->terminal, session);
    }
    if (session->spawn_cancellable) {
        g_cancellable_cancel(session->spawn_cancellable);
    }
    session->child_pid = 0;
    session->child_running = FALSE;
}

/**
 * @brief Complete a deferred terminal tab close from the main loop.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean terminal_session_finish_close_idle(gpointer user_data) {
    TerminalSession *session = user_data;
    if (!session) return G_SOURCE_REMOVE;

    session->close_source = 0u;
    terminal_panel_finish_close_session(session->panel, session);
    return G_SOURCE_REMOVE;
}

/**
 * @brief Schedule terminal tab removal after VTE finishes child-exited emission.
 * @details Removing the page directly from the child-exited signal can leave
 *          VTE/GLib still walking callbacks for a widget we just destroyed. One
 *          idle hop lets the signal unwind before the notebook is mutated.
 * @param session The session supplied by the caller.
 */
static void terminal_session_queue_finish_close(TerminalSession *session) {
    if (!session || session->close_source != 0u) return;
    session->close_source = g_idle_add(terminal_session_finish_close_idle,
                                      session);
}

/**
 * @brief Dispose a terminal session and free it when no async spawn is pending.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param session The session supplied by the caller.
 */
static void terminal_session_dispose(TerminalSession *session) {
    if (!session) return;

    terminal_session_begin_dispose(session);
    if (!session->spawning) {
        terminal_session_destroy(session);
    }
}

/**
 * @brief Handle terminal child process exit.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param terminal The terminal supplied by the caller.
 * @param status The status supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void terminal_session_child_exited(VteTerminal *terminal,
                                          int status,
                                          gpointer user_data) {
    (void)terminal;
    (void)status;
    TerminalSession *session = user_data;
    if (!session) return;

    session->child_pid = 0;
    session->child_running = FALSE;
    session->child_watch_owner = TERMINAL_CHILD_WATCH_NONE;
    session->spawning = FALSE;
    if (session->closing) {
        terminal_session_queue_finish_close(session);
        return;
    }
    if (!session->disposed) {
        terminal_panel_set_status(session->panel, "Terminal exited");
    }
}

/**
 * @brief Handle async terminal shell spawn completion.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param terminal The terminal supplied by the caller.
 * @param pid The pid supplied by the caller.
 * @param error Return location for an optional error; left untouched on success unless GTK sets it.
 * @param user_data The callback context passed through GTK signal data.
 */
static void terminal_session_spawned(VteTerminal *terminal,
                                     GPid pid,
                                     GError *error,
                                     gpointer user_data) {
    TerminalSession *session = user_data;
    if (!session) return;

    g_clear_object(&session->spawn_cancellable);
    session->spawning = FALSE;
    if (session->disposed || !session->panel || session->panel->disposed) {
        if (pid > 0) {
            terminal_session_watch_orphan_child(session, pid);
            (void)kill(pid, SIGHUP);
        }
        terminal_session_destroy(session);
        return;
    }

    if (session->closing) {
        session->child_pid = pid;
        session->child_running = TRUE;
        vte_terminal_watch_child(terminal, pid);
        session->child_watch_owner = TERMINAL_CHILD_WATCH_VTE;
        (void)kill(pid, SIGHUP);
        return;
    }

    if (error) {
        session->child_running = FALSE;
        terminal_panel_set_status(session->panel, "Terminal failed to start");
        app_window_set_error_status(session->panel->win, "Terminal failed",
                                    error->message);
        return;
    }

    session->child_pid = pid;
    session->child_running = TRUE;
    vte_terminal_watch_child(terminal, pid);
    session->child_watch_owner = TERMINAL_CHILD_WATCH_VTE;
    terminal_panel_set_status(session->panel,
                              session->dir ? session->dir : "Terminal");
}

/**
 * @brief Spawn the terminal shell if this session has no active child.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param session The session supplied by the caller.
 */
static void terminal_session_spawn(TerminalSession *session) {
    if (!session || !session->terminal || session->child_running ||
        session->spawning) {
        return;
    }

    const char *shell = terminal_panel_shell();
    char *argv[] = { (char *)shell, NULL };

    g_clear_object(&session->spawn_cancellable);
    session->spawn_cancellable = g_cancellable_new();
    session->spawning = TRUE;
    terminal_panel_set_status(session->panel, session->dir);

    vte_terminal_spawn_async(VTE_TERMINAL(session->terminal),
                             VTE_PTY_DEFAULT,
                             session->dir,
                             argv,
                             NULL,
                             G_SPAWN_DEFAULT,
                             NULL,
                             NULL,
                             NULL,
                             -1,
                             session->spawn_cancellable,
                             terminal_session_spawned,
                             session);
}

/**
 * @brief Remember an edited terminal tab title.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param editable The editable supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void terminal_session_title_changed(GtkEditable *editable,
                                           gpointer user_data) {
    TerminalSession *session = user_data;
    if (!session || !editable) return;
    g_free(session->title);
    session->title = g_strdup(gtk_editable_get_text(editable));
}

/**
 * @brief Close a terminal session from its tab close button.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void terminal_session_close_clicked(GtkWidget *widget,
                                           gpointer user_data) {
    (void)widget;
    TerminalSession *session = user_data;
    if (!session) return;
    terminal_panel_close_session(session->panel, session);
}

/**
 * @brief Create a notebook tab widget for a terminal session.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param session The session supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *terminal_session_tab_widget(TerminalSession *session) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(box, "graptos-terminal-tab");

    GtkWidget *entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(entry),
                          session && session->title ? session->title : "Terminal");
    gtk_widget_set_tooltip_text(entry,
                                session && session->dir ? session->dir : NULL);
    gtk_widget_set_size_request(entry, 120, -1);
    gtk_widget_add_css_class(entry, "graptos-terminal-tab-title");
    g_signal_connect(entry, "changed",
                     G_CALLBACK(terminal_session_title_changed), session);
    gtk_box_append(GTK_BOX(box), entry);

    GtkWidget *close = graptos_flat_button_new("×",
                                               "Close terminal tab",
                                               G_CALLBACK(terminal_session_close_clicked),
                                               session);
    gtk_widget_add_css_class(close, "graptos-terminal-tab-close");
    gtk_box_append(GTK_BOX(box), close);

    if (session) {
        session->tab_widget = box;
        session->tab_entry = entry;
    }
    return box;
}

/**
 * @brief Create a terminal session for a directory.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param dir The dir supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static TerminalSession *terminal_session_new(TerminalPanel *panel,
                                             const char *dir) {
    if (!panel || !dir || dir[0] == '\0') return NULL;

    TerminalSession *session = g_new0(TerminalSession, 1);
    if (!session) return NULL;
    session->panel = panel;
    session->dir = g_canonicalize_filename(dir, NULL);
    session->title = terminal_panel_default_title_for_dir(session->dir);

    session->terminal = vte_terminal_new();
    gtk_widget_add_css_class(session->terminal, "graptos-terminal");
    gtk_widget_set_vexpand(session->terminal, FALSE);
    gtk_widget_set_hexpand(session->terminal, TRUE);
    vte_terminal_set_size(VTE_TERMINAL(session->terminal), 80, 8);
    terminal_panel_apply_height_to_widget(panel, session->terminal);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(session->terminal), 10000);
    vte_terminal_set_scroll_on_output(VTE_TERMINAL(session->terminal), FALSE);
    vte_terminal_set_scroll_on_keystroke(VTE_TERMINAL(session->terminal), TRUE);
    g_signal_connect(session->terminal, "child-exited",
                     G_CALLBACK(terminal_session_child_exited), session);
    terminal_session_apply_colors(session);

    GtkWidget *tab = terminal_session_tab_widget(session);
    gtk_notebook_append_page(GTK_NOTEBOOK(panel->notebook),
                             session->terminal,
                             tab);
    g_ptr_array_add(panel->sessions, session);
    return session;
}

/**
 * @brief Find an existing terminal session for a directory.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param dir The dir supplied by the caller.
 * @param index_out Output storage filled when the operation can provide a value.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static TerminalSession *terminal_panel_find_session(TerminalPanel *panel,
                                                    const char *dir,
                                                    guint *index_out) {
    if (index_out) *index_out = 0u;
    if (!panel || !panel->sessions || !dir) return NULL;

    g_autofree char *canonical = g_canonicalize_filename(dir, NULL);
    for (guint i = 0; i < panel->sessions->len; i++) {
        TerminalSession *session = g_ptr_array_index(panel->sessions, i);
        if (session && g_strcmp0(session->dir, canonical) == 0) {
            if (index_out) *index_out = i;
            return session;
        }
    }
    return NULL;
}

/**
 * @brief Ensure a terminal session exists for a directory.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param dir The dir supplied by the caller.
 * @param index_out Output storage filled when the operation can provide a value.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static TerminalSession *terminal_panel_ensure_session(TerminalPanel *panel,
                                                      const char *dir,
                                                      guint *index_out) {
    TerminalSession *session = terminal_panel_find_session(panel, dir, index_out);
    if (session) return session;

    if (index_out && panel && panel->sessions) {
        *index_out = panel->sessions->len;
    }
    return terminal_session_new(panel, dir);
}

/**
 * @brief Return the notebook index for a terminal session.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param session The session supplied by the caller.
 * @return The computed value requested by the caller.
 */
static gint terminal_panel_session_index(TerminalPanel *panel,
                                         TerminalSession *session) {
    if (!panel || !panel->sessions || !session) return -1;

    for (guint i = 0; i < panel->sessions->len; i++) {
        if (g_ptr_array_index(panel->sessions, i) == session) {
            return (gint)i;
        }
    }
    return -1;
}

/**
 * @brief Close and remove a terminal session tab.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param session The session supplied by the caller.
 */
static void terminal_panel_finish_close_session(TerminalPanel *panel,
                                                TerminalSession *session) {
    if (!panel || !panel->notebook || !panel->sessions || !session) return;

    gint index = terminal_panel_session_index(panel, session);
    if (index < 0) return;

    /*
     * Disconnect VTE signals before removing the notebook page.  Removing the
     * page releases the terminal widget, so cleanup after that point must not
     * touch the widget instance.
     */
    terminal_session_begin_dispose(session);

    g_signal_handlers_disconnect_by_data(panel->notebook, panel);
    gtk_notebook_remove_page(GTK_NOTEBOOK(panel->notebook), index);
    g_signal_connect(panel->notebook, "switch-page",
                     G_CALLBACK(terminal_panel_switch_page), panel);

    g_ptr_array_remove_index(panel->sessions, (guint)index);
    if (!session->spawning) {
        terminal_session_destroy(session);
    }

    if (panel->sessions->len == 0u) {
        terminal_panel_set_status(panel, "Terminal");
        if (panel->revealer) {
            gtk_revealer_set_reveal_child(GTK_REVEALER(panel->revealer), FALSE);
        }
        return;
    }

    TerminalSession *current = terminal_panel_current_session(panel);
    if (current) {
        terminal_panel_set_status(panel, current->dir);
        terminal_session_spawn(current);
    }
}

/**
 * @brief Close and remove a terminal session tab.
 * @details Closing a terminal is a small state machine: cancel a spawn if it is
 *          pending, ask a running shell to leave, and only remove the notebook
 *          page when no callback can still touch the terminal widget.
 * @param panel The panel instance affected by the operation.
 * @param session The session supplied by the caller.
 */
static void terminal_panel_close_session(TerminalPanel *panel,
                                         TerminalSession *session) {
    if (!panel || !session || session->closing || session->disposed) return;

    session->closing = TRUE;
    if (session->tab_entry) {
        gtk_editable_set_text(GTK_EDITABLE(session->tab_entry), "Closing…");
    }
    if (session->tab_widget) gtk_widget_set_sensitive(session->tab_widget, FALSE);
    if (session->terminal) gtk_widget_set_sensitive(session->terminal, FALSE);
    terminal_panel_set_status(panel, "Closing terminal");

    if (session->spawn_cancellable) {
        g_cancellable_cancel(session->spawn_cancellable);
    }
    if (session->child_running && session->child_pid > 0) {
        (void)kill(session->child_pid, SIGHUP);
        return;
    }
    if (session->spawning) return;

    terminal_panel_finish_close_session(panel, session);
}

/**
 * @brief Switch the editor to an open file that belongs to a terminal directory.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @param dir The dir supplied by the caller.
 */
static void terminal_panel_switch_editor_to_dir(TerminalPanel *panel,
                                                const char *dir) {
    if (!panel || !panel->win || !panel->win->notebook || !dir) return;
    if (!panel->win->terminal_dynamic_directory) return;

    guint count = app_window_tab_count(panel->win);
    for (guint i = 0u; i < count; i++) {
        EditorTab *tab = app_window_tab_at(panel->win, i);
        g_autofree char *tab_dir = terminal_panel_dir_for_tab(tab);
        if (tab_dir && g_strcmp0(tab_dir, dir) == 0) {
            panel->switching_editor_from_terminal = TRUE;
            if (app_window_restore_saved_tile_group(panel->win, tab)) {
                panel->win->active_tab = tab;
            }
            gint page = -1;
            if (panel->win->notebook) {
                gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(panel->win->notebook));
                for (gint n = 0; n < pages; n++) {
                    GtkWidget *child =
                        gtk_notebook_get_nth_page(GTK_NOTEBOOK(panel->win->notebook), n);
                    EditorTab *visible = child
                        ? g_object_get_data(G_OBJECT(child), "graptos-tab")
                        : NULL;
                    gboolean owns_tab = FALSE;
                    if (visible && visible->tile_group) {
                        for (guint g = 0u; g < visible->tile_group->len; g++) {
                            if (g_ptr_array_index(visible->tile_group, g) == tab) {
                                owns_tab = TRUE;
                                break;
                            }
                        }
                    }
                    if (visible == tab || owns_tab) {
                        page = n;
                        break;
                    }
                }
            }
            if (page >= 0) {
                gtk_notebook_set_current_page(GTK_NOTEBOOK(panel->win->notebook), page);
            }
            panel->switching_editor_from_terminal = FALSE;
            return;
        }
    }
}

/**
 * @brief Handle terminal notebook page switches.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param notebook The notebook supplied by the caller.
 * @param page The page supplied by the caller.
 * @param page_num The page num supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void terminal_panel_switch_page(GtkNotebook *notebook,
                                       GtkWidget *page,
                                       guint page_num,
                                       gpointer user_data) {
    (void)notebook;
    (void)page;
    TerminalPanel *panel = user_data;
    if (!panel || !panel->sessions || page_num >= panel->sessions->len) return;

    TerminalSession *session = g_ptr_array_index(panel->sessions, page_num);
    if (!session) return;
    terminal_panel_set_status(panel, session->dir);
    terminal_session_spawn(session);
    terminal_panel_switch_editor_to_dir(panel, session->dir);
}

/**
 * @brief Adjust terminal panel height from a header button.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void terminal_panel_adjust_height(GtkWidget *widget,
                                         gpointer user_data) {
    TerminalPanel *panel = user_data;
    if (!panel) return;

    const char *direction = widget
        ? g_object_get_data(G_OBJECT(widget), "graptos-terminal-height-dir")
        : NULL;
    int delta = g_strcmp0(direction, "larger") == 0
        ? GRAPTOS_TERMINAL_HEIGHT_STEP
        : -GRAPTOS_TERMINAL_HEIGHT_STEP;
    panel->terminal_height += delta;
    terminal_panel_apply_height(panel);
}

/**
 * @brief Hide the terminal panel from its close button.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param widget The widget that emitted the callback or receives the update.
 * @param user_data The callback context passed through GTK signal data.
 */
static void terminal_panel_close_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    TerminalPanel *panel = user_data;
    if (!panel || !panel->revealer) return;
    gtk_revealer_set_reveal_child(GTK_REVEALER(panel->revealer), FALSE);
}

/**
 * @brief Hide the terminal revealer after its close transition completes.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param revealer The revealer supplied by the caller.
 * @param pspec The pspec supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void terminal_panel_revealer_child_revealed(GtkRevealer *revealer,
                                                   GParamSpec *pspec,
                                                   gpointer user_data) {
    (void)pspec;
    (void)user_data;
    if (!revealer) return;
    if (!gtk_revealer_get_reveal_child(revealer) &&
        !gtk_revealer_get_child_revealed(revealer)) {
        gtk_widget_set_visible(GTK_WIDGET(revealer), FALSE);
    }
}

/**
 * @brief Apply current editor colors to all terminal widgets.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 */
void terminal_panel_apply_colors(TerminalPanel *panel) {
    if (!panel || !panel->sessions) return;
    for (guint i = 0; i < panel->sessions->len; i++) {
        terminal_session_apply_colors(g_ptr_array_index(panel->sessions, i));
    }
}

/**
 * @brief Create an integrated terminal panel for a window.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
TerminalPanel *terminal_panel_new(EditorWindow *win) {
    TerminalPanel *panel = g_new0(TerminalPanel, 1);
    if (!panel) return NULL;
    panel->win = win;
    panel->terminal_height = GRAPTOS_TERMINAL_DEFAULT_HEIGHT;
    panel->sessions = g_ptr_array_new();

    panel->revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(panel->revealer),
                                     GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);
    gtk_revealer_set_transition_duration(GTK_REVEALER(panel->revealer), 120u);
    gtk_revealer_set_reveal_child(GTK_REVEALER(panel->revealer), FALSE);
    gtk_widget_set_visible(panel->revealer, FALSE);
    gtk_widget_set_vexpand(panel->revealer, FALSE);
    g_signal_connect(panel->revealer, "notify::child-revealed",
                     G_CALLBACK(terminal_panel_revealer_child_revealed),
                     panel);

    panel->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(panel->box, FALSE);
    gtk_widget_add_css_class(panel->box, "graptos-terminal-panel");

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_add_css_class(header, "graptos-terminal-header");
    panel->status_label = gtk_label_new("Terminal");
    gtk_label_set_xalign(GTK_LABEL(panel->status_label), 0.0f);
    gtk_widget_set_hexpand(panel->status_label, TRUE);
    gtk_widget_add_css_class(panel->status_label, "graptos-terminal-title");
    gtk_box_append(GTK_BOX(header), panel->status_label);

    GtkWidget *smaller = graptos_flat_button_new("−",
                                               "Decrease terminal height",
                                               G_CALLBACK(terminal_panel_adjust_height),
                                               panel);
    g_object_set_data(G_OBJECT(smaller), "graptos-terminal-height-dir",
                      "smaller");
    gtk_box_append(GTK_BOX(header), smaller);

    panel->height_label = gtk_label_new("");
    gtk_widget_add_css_class(panel->height_label, "graptos-terminal-height");
    gtk_box_append(GTK_BOX(header), panel->height_label);

    GtkWidget *larger = graptos_flat_button_new("+",
                                              "Increase terminal height",
                                              G_CALLBACK(terminal_panel_adjust_height),
                                              panel);
    g_object_set_data(G_OBJECT(larger), "graptos-terminal-height-dir",
                      "larger");
    gtk_box_append(GTK_BOX(header), larger);

    GtkWidget *close = graptos_flat_button_new("×", "Close terminal panel",
                                             G_CALLBACK(terminal_panel_close_clicked),
                                             panel);
    gtk_box_append(GTK_BOX(header), close);
    gtk_box_append(GTK_BOX(panel->box), header);

    panel->notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(panel->notebook), TRUE);
    gtk_widget_set_vexpand(panel->notebook, FALSE);
    gtk_widget_set_hexpand(panel->notebook, TRUE);
    g_signal_connect(panel->notebook, "switch-page",
                     G_CALLBACK(terminal_panel_switch_page), panel);
    terminal_panel_apply_height(panel);

    gtk_box_append(GTK_BOX(panel->box), panel->notebook);
    gtk_revealer_set_child(GTK_REVEALER(panel->revealer), panel->box);
    return panel;
}

/**
 * @brief Free an integrated terminal panel and release child processes.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 */
void terminal_panel_free(TerminalPanel *panel) {
    if (!panel) return;

    panel->disposed = TRUE;
    if (panel->notebook) {
        g_signal_handlers_disconnect_by_data(panel->notebook, panel);
    }
    if (panel->sessions) {
        while (panel->sessions->len > 0u) {
            TerminalSession *session = g_ptr_array_index(panel->sessions, 0u);
            g_ptr_array_remove_index(panel->sessions, 0u);
            terminal_session_dispose(session);
        }
        g_ptr_array_free(panel->sessions, TRUE);
    }
    g_free(panel);
}

/**
 * @brief Return the root widget for an integrated terminal panel.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *terminal_panel_widget(TerminalPanel *panel) {
    return panel ? panel->revealer : NULL;
}

/**
 * @brief Return whether the terminal panel is currently open.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean terminal_panel_is_open(TerminalPanel *panel) {
    return panel && panel->revealer
        ? gtk_revealer_get_reveal_child(GTK_REVEALER(panel->revealer))
        : FALSE;
}

/**
 * @brief Synchronize the terminal tab with the active editor tab.
 * @details Dynamic terminals follow the directory of the active file, but only
 *          while the panel is visible. That keeps hidden terminals from creating
 *          sessions just because the user is moving through editor tabs.
 * @param panel The panel instance affected by the operation.
 */
void terminal_panel_sync_to_active_file(TerminalPanel *panel) {
    if (!panel || !panel->win || !panel->win->terminal_dynamic_directory) return;
    if (!terminal_panel_is_open(panel)) return;
    if (panel->switching_editor_from_terminal) return;

    g_autofree char *dir =
        terminal_panel_dir_for_tab(app_window_current_tab(panel->win));
    if (!dir) return;

    guint index = 0u;
    TerminalSession *session = terminal_panel_ensure_session(panel, dir, &index);
    if (!session) return;

    gtk_notebook_set_current_page(GTK_NOTEBOOK(panel->notebook), (gint)index);
    terminal_session_spawn(session);
    terminal_panel_set_status(panel, session->dir);
}

/**
 * @brief Toggle terminal visibility and spawn a shell when first shown.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 */
void terminal_panel_toggle(TerminalPanel *panel) {
    if (!panel || !panel->revealer) return;

    gboolean visible =
        gtk_revealer_get_reveal_child(GTK_REVEALER(panel->revealer));
    if (visible) {
        gtk_revealer_set_reveal_child(GTK_REVEALER(panel->revealer), FALSE);
        return;
    }

    gtk_widget_set_visible(panel->revealer, TRUE);
    gtk_revealer_set_reveal_child(GTK_REVEALER(panel->revealer), TRUE);
    terminal_panel_apply_colors(panel);

    if (panel->win && panel->win->terminal_dynamic_directory) {
        terminal_panel_sync_to_active_file(panel);
    } else {
        g_autofree char *cwd = terminal_panel_working_dir(panel);
        guint index = 0u;
        TerminalSession *session =
            terminal_panel_ensure_session(panel, cwd, &index);
        if (session) {
            gtk_notebook_set_current_page(GTK_NOTEBOOK(panel->notebook),
                                          (gint)index);
            terminal_session_spawn(session);
        }
    }

    TerminalSession *current = terminal_panel_current_session(panel);
    if (current && current->terminal) gtk_widget_grab_focus(current->terminal);
}
