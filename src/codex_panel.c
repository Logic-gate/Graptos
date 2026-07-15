/**
 * @file src/codex_panel.c
 * @brief Codex sidebar panel and chat review UI.
 */

#include "codex_panel.h"

#include "app.h"
#include "codex_review.h"
#include "codex_markdown.h"
#include "dialogs.h"
#include "git.h"
#include "ui.h"

/**
 * @brief Codex panel type definition.
 */
typedef struct {
    struct _CodexPanel *panel; /**< Panel. */
    GtkWidget *page; /**< Page. */
    GtkWidget *view; /**< View. */
    GtkTextBuffer *buffer; /**< Buffer. */
    char *thread_id; /**< Thread id. */
    GString *markdown; /**< Markdown. */
    gboolean render_dirty; /**< Render dirty. */
} CodexChat;

/**
 * @brief Codex panel type definition.
 */
struct _CodexPanel {
    EditorWindow *win; /**< Win. */
    GtkWidget *revealer; /**< Revealer. */
    GtkWidget *status_label; /**< Status label. */
    GtkWidget *transcript_view; /**< Transcript view. */
    GtkTextBuffer *transcript; /**< Transcript. */
    GtkWidget *chat_notebook; /**< Chat notebook. */
    GPtrArray *chats; /**< Chats. */
    CodexChat *current_chat; /**< Current chat. */
    gboolean switching_chat; /**< Switching chat. */
    GtkWidget *composer_view; /**< Composer view. */
    GtkTextBuffer *composer; /**< Composer. */
    GtkWidget *send_button; /**< Send button. */
    GtkWidget *stop_button; /**< Stop button. */
    GtkWidget *new_button; /**< New button. */
    GtkWidget *file_context; /**< File context. */
    GtkWidget *selection_context; /**< Selection context. */
    GtkWidget *context_label; /**< Context label. */
    GtkWidget *approval_box; /**< Approval box. */
    GtkWidget *approval_label; /**< Approval label. */
    GtkWidget *review_box; /**< Review box. */
    GtkWidget *changed_files; /**< Changed files. */
    gboolean turn_active; /**< Turn active. */
    gboolean visible; /**< Visible. */
    char *turn_diff; /**< Turn diff. */
    guint render_timeout; /**< Render timeout. */
};

/**
 * @brief Codex context limit macro.
 */
#define CODEX_CONTEXT_LIMIT 65536

/**
 * @brief Panel render now.
 */
static void panel_render_now(CodexPanel *panel) {
    if (!panel || !panel->current_chat ||
        !panel->current_chat->render_dirty) return;
    CodexChat *chat = panel->current_chat;
    codex_markdown_render(chat->buffer, chat->markdown->str);
    chat->render_dirty = FALSE;
    if (gtk_widget_get_mapped(chat->view) &&
        gtk_widget_get_width(chat->view) > 0 &&
        gtk_widget_get_height(chat->view) > 0) {
        GtkTextMark *mark = gtk_text_buffer_get_insert(chat->buffer);
        gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(chat->view), mark);
    }
}

/**
 * @brief Panel render timeout cb.
 */
static gboolean panel_render_timeout_cb(gpointer user_data) {
    CodexPanel *panel = user_data;
    if (!panel) return G_SOURCE_REMOVE;
    panel->render_timeout = 0u;
    panel_render_now(panel);
    return G_SOURCE_REMOVE; /**< G source remove. */
}

/**
 * @brief Panel schedule render.
 */
static void panel_schedule_render(CodexPanel *panel) {
    if (!panel || panel->render_timeout != 0u || !panel->current_chat) return;
    gsize length = panel->current_chat->markdown->len;
    guint delay = length < 8192u ? 50u : length < 65536u ? 120u : 250u;
    panel->render_timeout = g_timeout_add(delay, panel_render_timeout_cb, panel);
}

/**
 * @brief Codex chat free.
 */
static void codex_chat_free(gpointer data) {
    CodexChat *chat = data;
    if (!chat) return;
    g_free(chat->thread_id);
    if (chat->markdown) g_string_free(chat->markdown, TRUE);
    g_free(chat);
}

/**
 * @brief Panel close chat.
 */
static void panel_close_chat(GtkWidget *widget, gpointer user_data);

/**
 * @brief Panel add chat.
 */
static CodexChat *panel_add_chat(CodexPanel *panel) {
    CodexChat *chat = g_new0(CodexChat, 1);
    chat->panel = panel;
    chat->markdown = g_string_new(NULL);
    chat->buffer = gtk_text_buffer_new(NULL);
    chat->view = gtk_text_view_new_with_buffer(chat->buffer);
    g_object_unref(chat->buffer);
    gtk_widget_add_css_class(chat->view, "cleaf-codex-preview");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat->view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(chat->view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat->view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(chat->view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(chat->view), 8);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), chat->view);
    chat->page = scroll;
    char *title = g_strdup_printf("AI %u", panel->chats->len + 1u);
    GtkWidget *tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    GtkWidget *label = gtk_label_new(title);
    GtkWidget *close = cleaf_flat_button_new("×", "Close AI tab",
                                              G_CALLBACK(panel_close_chat), chat);
    g_free(title);
    gtk_widget_add_css_class(close, "cleaf-tab-close");
    gtk_box_append(GTK_BOX(tab_box), label);
    gtk_box_append(GTK_BOX(tab_box), close);
    gtk_notebook_append_page(GTK_NOTEBOOK(panel->chat_notebook), scroll, tab_box);
    g_ptr_array_add(panel->chats, chat);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(panel->chat_notebook),
                                  (gint)panel->chats->len - 1);
    panel->current_chat = chat;
    panel->transcript_view = chat->view;
    panel->transcript = chat->buffer;
    panel_render_now(panel);
    return chat; /**< Chat. */
}

/**
 * @brief Panel activate chat.
 */
static void panel_activate_chat(CodexPanel *panel, guint index) {
    if (!panel || index >= panel->chats->len) return;
    CodexChat *chat = g_ptr_array_index(panel->chats, index);
    panel->current_chat = chat;
    panel->transcript_view = chat->view;
    panel->transcript = chat->buffer;
    panel_render_now(panel);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(panel->chat_notebook), (gint)index);
    if (chat->thread_id && panel->win->codex_client &&
        g_strcmp0(chat->thread_id,
                  codex_client_get_thread_id(panel->win->codex_client)) != 0) {
        (void)codex_client_resume_thread(panel->win->codex_client,
                                         chat->thread_id);
    }
}

/**
 * @brief Panel close chat.
 */
static void panel_close_chat(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    CodexChat *chat = user_data;
    CodexPanel *panel = chat ? chat->panel : NULL;
    if (!panel) return;
    if (panel->turn_active && chat == panel->current_chat) {
        gtk_label_set_text(GTK_LABEL(panel->status_label),
                           "Stop the active turn before closing this tab");
        return;
    }
    guint index = 0u;
    while (index < panel->chats->len &&
           g_ptr_array_index(panel->chats, index) != chat) index++;
    if (index >= panel->chats->len) return;
    if (panel->chats->len == 1u) {
        if (codex_client_new_thread(panel->win->codex_client)) {
            gtk_text_buffer_set_text(chat->buffer, "", 0);
            g_string_truncate(chat->markdown, 0u);
            g_clear_pointer(&chat->thread_id, g_free);
            gtk_label_set_text(GTK_LABEL(panel->status_label),
                               "Starting new chat…");
        }
        return;
    }
    panel->switching_chat = TRUE;
    gtk_notebook_remove_page(GTK_NOTEBOOK(panel->chat_notebook), (gint)index);
    g_ptr_array_remove_index(panel->chats, index);
    panel->switching_chat = FALSE;
    guint next = index < panel->chats->len ? index : panel->chats->len - 1u;
    panel_activate_chat(panel, next);
}

/**
 * @brief Panel chat switched.
 */
static void panel_chat_switched(GtkNotebook *notebook,
                                GtkWidget *page,
                                guint page_num,
                                gpointer user_data) {
    (void)notebook;
    (void)page;
    CodexPanel *panel = user_data;
    if (!panel || panel->switching_chat || page_num >= panel->chats->len) return;
    CodexChat *chat = g_ptr_array_index(panel->chats, page_num);
    if (panel->turn_active && chat != panel->current_chat) {
        gint old_page = panel->current_chat
            ? gtk_notebook_page_num(GTK_NOTEBOOK(panel->chat_notebook),
                                    panel->current_chat->page) : -1;
        if (old_page >= 0) {
            panel->switching_chat = TRUE;
            gtk_notebook_set_current_page(GTK_NOTEBOOK(panel->chat_notebook), old_page);
            panel->switching_chat = FALSE;
        }
        return;
    }
    panel->current_chat = chat;
    panel->transcript_view = chat->view;
    panel->transcript = chat->buffer;
    if (chat->thread_id && panel->win->codex_client &&
        g_strcmp0(chat->thread_id,
                  codex_client_get_thread_id(panel->win->codex_client)) != 0) {
        (void)codex_client_resume_thread(panel->win->codex_client,
                                         chat->thread_id);
    }
}

/**
 * @brief Panel append.
 */
static void panel_append(CodexPanel *panel, const char *text) {
    if (!panel || !panel->transcript || !text) return;
    if (!panel->current_chat || !panel->current_chat->markdown) return;
    g_string_append(panel->current_chat->markdown, text);
    panel->current_chat->render_dirty = TRUE;
    panel_schedule_render(panel);
}

/**
 * @brief Panel set turn active.
 */
static void panel_set_turn_active(CodexPanel *panel, gboolean active) {
    if (!panel) return;
    panel->turn_active = active;
    gtk_widget_set_sensitive(panel->send_button, !active);
    gtk_widget_set_sensitive(panel->new_button, !active);
    gtk_widget_set_sensitive(panel->stop_button, active);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(panel->composer_view), !active);
}

/**
 * @brief Panel editor context.
 */
static char *panel_editor_context(CodexPanel *panel) {
    EditorTab *tab = panel && panel->win
        ? app_window_current_tab(panel->win) : NULL;
    if (!tab || !tab->buffer) return g_strdup("");
    gboolean include_file = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(panel->file_context));
    gboolean include_selection = gtk_check_button_get_active(
        GTK_CHECK_BUTTON(panel->selection_context));
    GtkTextIter start; /**< Start. */
    GtkTextIter end; /**< End. */
    gboolean selected = gtk_text_buffer_get_selection_bounds(tab->buffer,
                                                              &start, &end);
    if (!include_file && (!include_selection || !selected)) return g_strdup("");
    if (!selected || !include_selection) gtk_text_buffer_get_bounds(tab->buffer,
                                                                     &start, &end);
    char *content = gtk_text_buffer_get_text(tab->buffer, &start, &end, FALSE);
    if (strlen(content) > CODEX_CONTEXT_LIMIT) {
        content[CODEX_CONTEXT_LIMIT] = '\0';
    }
    const char *path = tab->file_path ? tab->file_path : "Untitled buffer";
    char *context = g_strdup_printf(
        "\n\n<cleaf_editor_context>\nFile: %s\nSource: %s\n```\n%s\n```\n"
        "</cleaf_editor_context>",
        path, selected && include_selection ? "selected text" : "current buffer",
        content);
    g_free(content);
    return context; /**< Context. */
}

/**
 * @brief Codex panel refresh context.
 */
void codex_panel_refresh_context(CodexPanel *panel) {
    EditorTab *tab = panel && panel->win
        ? app_window_current_tab(panel->win) : NULL;
    GtkTextIter start; /**< Start. */
    GtkTextIter end; /**< End. */
    gboolean selected = tab && tab->buffer &&
        gtk_text_buffer_get_selection_bounds(tab->buffer, &start, &end);
    gtk_widget_set_sensitive(panel->selection_context, selected);
    const char *name = tab && tab->file_path ? tab->file_path : "Untitled";
    char *label = g_strdup_printf("Context: %s%s", name,
                                  selected ? " + selection" : "");
    gtk_label_set_text(GTK_LABEL(panel->context_label), label);
    g_free(label);
}

/**
 * @brief Panel send.
 */
static void panel_send(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    CodexPanel *panel = user_data;
    if (!panel || !panel->win || !panel->win->codex_client) return;
    GtkTextIter start; /**< Start. */
    GtkTextIter end; /**< End. */
    gtk_text_buffer_get_bounds(panel->composer, &start, &end);
    char *prompt = gtk_text_buffer_get_text(panel->composer, &start, &end, FALSE);
    g_strstrip(prompt);
    if (prompt[0] == '\0') {
        g_free(prompt);
        return;
    }
    codex_panel_refresh_context(panel);
    char *context = panel_editor_context(panel);
    char *request = g_strconcat(prompt, context, NULL);
    if (codex_client_send_prompt(panel->win->codex_client, request)) {
        panel_append(panel, "### You\n");
        panel_append(panel, prompt);
        panel_append(panel, "\n\n### Codex\n");
        gtk_text_buffer_set_text(panel->composer, "", 0);
        panel_set_turn_active(panel, TRUE);
        gtk_label_set_text(GTK_LABEL(panel->status_label), "Codex is working…");
    } else {
        gtk_label_set_text(GTK_LABEL(panel->status_label),
                           "Cannot send: Codex is not ready or a turn is active");
    }
    g_free(request);
    g_free(context);
    g_free(prompt);
}

/**
 * @brief Panel stop.
 */
static void panel_stop(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    CodexPanel *panel = user_data;
    if (panel && panel->win && panel->win->codex_client) {
        if (codex_client_interrupt(panel->win->codex_client)) {
            gtk_label_set_text(GTK_LABEL(panel->status_label), "Stopping…");
            gtk_widget_set_sensitive(panel->stop_button, FALSE);
        } else {
            gtk_label_set_text(GTK_LABEL(panel->status_label),
                               "No active Codex turn to stop");
        }
    }
}

/**
 * @brief Panel new thread.
 */
static void panel_new_thread(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    CodexPanel *panel = user_data;
    if (!panel || !panel->win || !panel->win->codex_client) return;
    if (codex_client_new_thread(panel->win->codex_client)) {
        panel_add_chat(panel);
        gtk_label_set_text(GTK_LABEL(panel->status_label), "Starting new chat…");
    }
}

/**
 * @brief Panel close.
 */
static void panel_close(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    codex_panel_toggle(user_data);
}

/**
 * @brief Panel approval.
 */
static void panel_approval(GtkWidget *widget, gpointer user_data) {
    CodexPanel *panel = user_data;
    const char *decision = g_object_get_data(G_OBJECT(widget), "decision");
    if (panel && panel->win && panel->win->codex_client && decision) {
        (void)codex_client_resolve_approval(panel->win->codex_client, decision);
    }
}

/**
 * @brief Panel clear children.
 */
static void panel_clear_children(GtkWidget *box) {
    GtkWidget *child = box ? gtk_widget_get_first_child(box) : NULL;
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(box), child);
        child = next;
    }
}

/**
 * @brief Panel open changed file.
 */
static void panel_open_changed_file(GtkWidget *widget, gpointer user_data) {
    CodexPanel *panel = user_data;
    const char *relative = g_object_get_data(G_OBJECT(widget), "codex-path");
    if (!panel || !relative) return;
    const char *root = panel->win->project_root;
    char *path = root ? g_build_filename(root, relative, NULL)
                      : g_canonicalize_filename(relative, NULL);
    (void)app_window_open_file(panel->win, path);
    g_free(path);
}

/**
 * @brief Panel update changed files.
 */
static void panel_update_changed_files(CodexPanel *panel) {
    panel_clear_children(panel->changed_files);
    if (!panel->turn_diff) return;
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    char **lines = g_strsplit(panel->turn_diff, "\n", -1);
    for (guint i = 0u; lines[i]; i++) {
        const char *path = g_str_has_prefix(lines[i], "+++ b/")
            ? lines[i] + 6
            : g_str_has_prefix(lines[i], "--- a/") ? lines[i] + 6 : NULL;
        if (!path || path[0] == '\0' || g_str_equal(path, "/dev/null") ||
            g_hash_table_contains(seen, path)) continue;
        g_hash_table_add(seen, g_strdup(path));
        GtkWidget *button = cleaf_flat_button_new(path, "Open changed file",
                                                   G_CALLBACK(panel_open_changed_file),
                                                   panel);
        g_object_set_data_full(G_OBJECT(button), "codex-path", g_strdup(path), g_free);
        gtk_widget_set_halign(button, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(panel->changed_files), button);
    }
    g_strfreev(lines);
    g_hash_table_destroy(seen);
}

/**
 * @brief Panel add changed path.
 */
static void panel_add_changed_path(CodexPanel *panel,
                                   GPtrArray *paths,
                                   GHashTable *seen,
                                   const char *relative) {
    if (!panel || !paths || !seen || !relative || relative[0] == '\0' ||
        g_str_equal(relative, "/dev/null")) return;

    const char *root = panel->win && panel->win->project_root
        ? panel->win->project_root : NULL;
    char *path = root ? g_build_filename(root, relative, NULL)
                      : g_canonicalize_filename(relative, NULL);
    char *canonical = path ? g_canonicalize_filename(path, NULL) : NULL;
    g_free(path);
    if (!canonical || g_hash_table_contains(seen, canonical)) {
        g_free(canonical);
        return;
    }
    g_hash_table_add(seen, g_strdup(canonical));
    g_ptr_array_add(paths, canonical);
}

/**
 * @brief Panel changed paths.
 */
static GPtrArray *panel_changed_paths(CodexPanel *panel) {
    GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
    if (!panel || !panel->turn_diff) return paths;

    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal,
                                             g_free, NULL);
    char **lines = g_strsplit(panel->turn_diff, "\n", -1);
    for (guint i = 0u; lines[i]; i++) {
        const char *path = g_str_has_prefix(lines[i], "+++ b/")
            ? lines[i] + 6
            : g_str_has_prefix(lines[i], "--- a/") ? lines[i] + 6 : NULL;
        panel_add_changed_path(panel, paths, seen, path);
    }
    g_strfreev(lines);
    g_hash_table_destroy(seen);
    return paths;
}

/**
 * @brief Path in array.
 */
static gboolean path_in_array(GPtrArray *paths, const char *path) {
    if (!paths || !path) return FALSE;
    for (guint i = 0u; i < paths->len; i++) {
        if (g_strcmp0(g_ptr_array_index(paths, i), path) == 0) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Panel reload kept tabs.
 */
static void panel_reload_kept_tabs(CodexPanel *panel,
                                   guint *reloaded_out,
                                   guint *skipped_modified_out,
                                   guint *skipped_missing_out) {
    if (reloaded_out) *reloaded_out = 0u;
    if (skipped_modified_out) *skipped_modified_out = 0u;
    if (skipped_missing_out) *skipped_missing_out = 0u;
    if (!panel || !panel->win || !panel->win->notebook) return;

    GPtrArray *paths = panel_changed_paths(panel);
    gint pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(panel->win->notebook));
    for (gint i = 0; i < pages; i++) {
        GtkWidget *child = gtk_notebook_get_nth_page(GTK_NOTEBOOK(panel->win->notebook), i);
        EditorTab *tab = child ? g_object_get_data(G_OBJECT(child), "cleaf-tab") : NULL;
        if (!tab || !tab->file_path) continue;

        char *canonical = g_canonicalize_filename(tab->file_path, NULL);
        if (!path_in_array(paths, canonical)) {
            g_free(canonical);
            continue;
        }
        if (tab->modified) {
            if (skipped_modified_out) (*skipped_modified_out)++;
            g_free(canonical);
            continue;
        }
        if (!g_file_test(canonical, G_FILE_TEST_IS_REGULAR)) {
            if (skipped_missing_out) (*skipped_missing_out)++;
            g_free(canonical);
            continue;
        }
        if (editor_tab_load_file(tab, canonical) && reloaded_out) {
            (*reloaded_out)++;
        }
        g_free(canonical);
    }
    g_ptr_array_free(paths, TRUE);
}

/**
 * @brief Panel review diff.
 */
static void panel_review_diff(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    CodexPanel *panel = user_data;
    if (panel && panel->turn_diff) {
        codex_review_open_diff(panel->win, panel->turn_diff);
    }
}

/**
 * @brief Panel keep diff.
 */
static void panel_keep_diff(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    CodexPanel *panel = user_data;
    if (!panel) return;
    guint reloaded = 0u;
    guint skipped_modified = 0u;
    guint skipped_missing = 0u;
    panel_reload_kept_tabs(panel, &reloaded, &skipped_modified,
                           &skipped_missing);
    g_clear_pointer(&panel->turn_diff, g_free);
    gtk_widget_set_visible(panel->review_box, FALSE);
    cleaf_git_refresh_and_rebuild(panel->win);
    if (skipped_modified > 0u || skipped_missing > 0u) {
        char *message = g_strdup_printf(
            "Codex changes kept; reloaded %u open tab%s%s%s",
            reloaded,
            reloaded == 1u ? "" : "s",
            skipped_modified > 0u ? "; skipped modified tab" : "",
            skipped_missing > 0u ? "; skipped missing file" : "");
        app_window_set_status(panel->win, message);
        g_free(message);
    } else if (reloaded > 0u) {
        char *message = g_strdup_printf("Codex changes kept; reloaded %u open tab%s",
                                        reloaded, reloaded == 1u ? "" : "s");
        app_window_set_status(panel->win, message);
        g_free(message);
    } else {
        app_window_set_status(panel->win, "Codex changes kept");
    }
}

/**
 * @brief Panel revert diff.
 */
static void panel_revert_diff(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    CodexPanel *panel = user_data;
    if (!panel || !panel->turn_diff) return;
    if (!dialog_confirm_yes_no(app_window_gtk(panel->win), "Revert Codex turn",
                               "Reverse only the changes recorded for this Codex turn?")) {
        return;
    }
    g_autoptr(GError) error = NULL;
    if (codex_review_revert(panel->win, panel->turn_diff, &error)) {
        g_clear_pointer(&panel->turn_diff, g_free);
        gtk_widget_set_visible(panel->review_box, FALSE);
        app_window_set_status(panel->win, "Codex turn reverted");
    } else {
        app_window_set_error_status(panel->win, "Could not revert Codex turn",
                                    error ? error->message : "The files changed after this turn.");
    }
}

/**
 * @brief Codex panel new.
 */
CodexPanel *codex_panel_new(EditorWindow *win) {
    CodexPanel *panel = g_new0(CodexPanel, 1);
    panel->win = win;
    panel->chats = g_ptr_array_new_with_free_func(codex_chat_free);
    panel->revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(panel->revealer),
                                     GTK_REVEALER_TRANSITION_TYPE_NONE);
    gtk_revealer_set_transition_duration(GTK_REVEALER(panel->revealer), 0u);
    gtk_revealer_set_reveal_child(GTK_REVEALER(panel->revealer), TRUE);
    gtk_widget_set_size_request(panel->revealer, 340, -1);
    panel->visible = TRUE;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(box, "cleaf-codex-panel");
    gtk_widget_set_size_request(box, 340, -1);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *title = gtk_label_new("Codex");
    gtk_widget_add_css_class(title, "cleaf-tool-title");
    gtk_widget_set_hexpand(title, TRUE);
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    panel->new_button = cleaf_flat_button_new("New", "Start a new Codex chat",
                                               G_CALLBACK(panel_new_thread), panel);
    GtkWidget *close = cleaf_flat_button_new("×", "Close Codex panel",
                                              G_CALLBACK(panel_close), panel);
    gtk_box_append(GTK_BOX(header), title);
    gtk_box_append(GTK_BOX(header), panel->new_button);
    gtk_box_append(GTK_BOX(header), close);
    gtk_box_append(GTK_BOX(box), header);

    panel->status_label = gtk_label_new("Connecting to Codex…");
    gtk_label_set_xalign(GTK_LABEL(panel->status_label), 0.0f);
    gtk_widget_add_css_class(panel->status_label, "cleaf-codex-status");
    gtk_box_append(GTK_BOX(box), panel->status_label);

    GtkWidget *context_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    panel->file_context = gtk_check_button_new_with_label("File");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(panel->file_context), TRUE);
    panel->selection_context = gtk_check_button_new_with_label("Selection");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(panel->selection_context), TRUE);
    panel->context_label = gtk_label_new("Context: Untitled");
    gtk_label_set_ellipsize(GTK_LABEL(panel->context_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_hexpand(panel->context_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(panel->context_label), 0.0f);
    gtk_box_append(GTK_BOX(context_row), panel->file_context);
    gtk_box_append(GTK_BOX(context_row), panel->selection_context);
    gtk_box_append(GTK_BOX(context_row), panel->context_label);
    gtk_box_append(GTK_BOX(box), context_row);

    panel->chat_notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(panel->chat_notebook), TRUE);
    gtk_widget_set_vexpand(panel->chat_notebook, TRUE);
    g_signal_connect(panel->chat_notebook, "switch-page",
                     G_CALLBACK(panel_chat_switched), panel);
    gtk_box_append(GTK_BOX(box), panel->chat_notebook);
    panel_add_chat(panel);

    panel->review_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(panel->review_box, "cleaf-codex-review");
    GtkWidget *review_title = gtk_label_new("Changes from this turn");
    gtk_label_set_xalign(GTK_LABEL(review_title), 0.0f);
    panel->changed_files = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *review_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *review = cleaf_flat_button_new("Review diff", "Open the turn diff",
                                              G_CALLBACK(panel_review_diff), panel);
    GtkWidget *keep = cleaf_flat_button_new("Keep", "Keep these changes",
                                            G_CALLBACK(panel_keep_diff), panel);
    GtkWidget *revert = cleaf_flat_button_new("Revert", "Reverse this turn only",
                                              G_CALLBACK(panel_revert_diff), panel);
    gtk_box_append(GTK_BOX(review_actions), review);
    gtk_box_append(GTK_BOX(review_actions), keep);
    gtk_box_append(GTK_BOX(review_actions), revert);
    gtk_box_append(GTK_BOX(panel->review_box), review_title);
    gtk_box_append(GTK_BOX(panel->review_box), panel->changed_files);
    gtk_box_append(GTK_BOX(panel->review_box), review_actions);
    gtk_widget_set_visible(panel->review_box, FALSE);
    gtk_box_append(GTK_BOX(box), panel->review_box);

    panel->approval_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(panel->approval_box, "cleaf-codex-approval");
    panel->approval_label = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(panel->approval_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(panel->approval_label), 0.0f);
    GtkWidget *approval_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *deny = cleaf_flat_button_new("Deny", "Decline this request",
                                            G_CALLBACK(panel_approval), panel);
    GtkWidget *allow = cleaf_flat_button_new("Allow once", "Allow this request",
                                             G_CALLBACK(panel_approval), panel);
    g_object_set_data(G_OBJECT(deny), "decision", "decline");
    g_object_set_data(G_OBJECT(allow), "decision", "accept");
    gtk_box_append(GTK_BOX(approval_actions), deny);
    gtk_box_append(GTK_BOX(approval_actions), allow);
    gtk_box_append(GTK_BOX(panel->approval_box), panel->approval_label);
    gtk_box_append(GTK_BOX(panel->approval_box), approval_actions);
    gtk_widget_set_visible(panel->approval_box, FALSE);
    gtk_box_append(GTK_BOX(box), panel->approval_box);

    panel->composer_view = gtk_text_view_new();
    gtk_widget_add_css_class(panel->composer_view, "cleaf-codex-prompt");
    panel->composer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(panel->composer_view));
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(panel->composer_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(panel->composer_view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(panel->composer_view), 8);
    GtkWidget *composer_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(composer_scroll, -1, 100);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(composer_scroll),
                                  panel->composer_view);
    gtk_box_append(GTK_BOX(box), composer_scroll);

    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    panel->stop_button = cleaf_flat_button_new("Stop", "Interrupt this turn",
                                                G_CALLBACK(panel_stop), panel);
    panel->send_button = cleaf_flat_button_new("Send", "Send prompt to Codex",
                                                G_CALLBACK(panel_send), panel);
    gtk_widget_set_hexpand(panel->stop_button, TRUE);
    gtk_widget_set_halign(panel->stop_button, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(actions), panel->stop_button);
    gtk_box_append(GTK_BOX(actions), panel->send_button);
    gtk_box_append(GTK_BOX(box), actions);
    panel_set_turn_active(panel, FALSE);
    gtk_widget_set_sensitive(panel->send_button, FALSE);
    codex_panel_refresh_context(panel);

    gtk_revealer_set_child(GTK_REVEALER(panel->revealer), box);
    return panel;
}

/**
 * @brief Codex panel free.
 */
void codex_panel_free(CodexPanel *panel) {
    if (panel && panel->render_timeout != 0u) {
        g_source_remove(panel->render_timeout);
        panel->render_timeout = 0u;
    }
    if (panel) g_free(panel->turn_diff);
    if (panel && panel->chats) g_ptr_array_free(panel->chats, TRUE);
    g_free(panel);
}

/**
 * @brief Codex panel widget.
 */
GtkWidget *codex_panel_widget(CodexPanel *panel) {
    return panel ? panel->revealer : NULL;
}

/**
 * @brief Codex panel toggle.
 */
void codex_panel_toggle(CodexPanel *panel) {
    if (!panel) return;
    panel->visible = !panel->visible;
    gtk_revealer_set_reveal_child(GTK_REVEALER(panel->revealer), panel->visible);
    gtk_widget_set_visible(panel->revealer, panel->visible);
    if (panel->visible) {
        gtk_widget_set_size_request(panel->revealer, 340, -1);
        codex_panel_refresh_context(panel);
    }
    gtk_widget_queue_resize(panel->revealer);
}

/**
 * @brief Codex panel set connection.
 */
void codex_panel_set_connection(CodexPanel *panel,
                                CodexClientState state,
                                const char *detail) {
    if (!panel) return;
    if (state == CODEX_CLIENT_READY) {
        if (panel->current_chat && !panel->current_chat->thread_id) {
            panel->current_chat->thread_id = g_strdup(
                codex_client_get_thread_id(panel->win->codex_client));
        }
        gtk_label_set_text(GTK_LABEL(panel->status_label), "Ready");
        gtk_widget_set_sensitive(panel->send_button, !panel->turn_active);
        gtk_widget_set_sensitive(panel->new_button, !panel->turn_active);
    } else if (state == CODEX_CLIENT_FAILED) {
        char *message = g_strdup_printf("Unavailable: %s",
                                        detail ? detail : "unknown error");
        gtk_label_set_text(GTK_LABEL(panel->status_label), message);
        gtk_widget_set_sensitive(panel->send_button, FALSE);
        g_free(message);
    } else {
        gtk_label_set_text(GTK_LABEL(panel->status_label), "Connecting…");
        gtk_widget_set_sensitive(panel->send_button, FALSE);
    }
}

/**
 * @brief Codex panel handle event.
 */
void codex_panel_handle_event(CodexClient *client,
                              CodexClientEvent event,
                              const char *text,
                              gpointer user_data) {
    (void)client;
    EditorWindow *win = user_data;
    CodexPanel *panel = win ? win->codex_panel : NULL;
    if (!panel) return;
    if (event == CODEX_EVENT_AGENT_DELTA) {
        panel_append(panel, text);
    } else if (event == CODEX_EVENT_TURN_STARTED) {
        panel_set_turn_active(panel, TRUE);
    } else if (event == CODEX_EVENT_TURN_COMPLETED) {
        panel_append(panel, "\n\n");
        if (panel->render_timeout != 0u) {
            g_source_remove(panel->render_timeout);
            panel->render_timeout = 0u;
        }
        panel_render_now(panel);
        panel_set_turn_active(panel, FALSE);
        gtk_label_set_text(GTK_LABEL(panel->status_label), "Ready");
    } else if (event == CODEX_EVENT_TURN_INTERRUPTED) {
        panel_append(panel, "\n[Stopped]\n\n");
        if (panel->render_timeout != 0u) {
            g_source_remove(panel->render_timeout);
            panel->render_timeout = 0u;
        }
        panel_render_now(panel);
        panel_set_turn_active(panel, FALSE);
        gtk_label_set_text(GTK_LABEL(panel->status_label), "Ready");
    } else if (event == CODEX_EVENT_TURN_FAILED) {
        panel_append(panel, "\n[Error] ");
        panel_append(panel, text ? text : "Codex turn failed");
        panel_append(panel, "\n\n");
        if (panel->render_timeout != 0u) {
            g_source_remove(panel->render_timeout);
            panel->render_timeout = 0u;
        }
        panel_render_now(panel);
        panel_set_turn_active(panel, FALSE);
        gtk_label_set_text(GTK_LABEL(panel->status_label), "Turn failed");
    } else if (event == CODEX_EVENT_ACTIVITY) {
        panel_append(panel, "\n[Activity] ");
        panel_append(panel, text ? text : "Codex activity");
        panel_append(panel, "\n");
    } else if (event == CODEX_EVENT_APPROVAL_REQUESTED) {
        gtk_label_set_text(GTK_LABEL(panel->approval_label),
                           text ? text : "Codex requests approval");
        gtk_widget_set_visible(panel->approval_box, TRUE);
        gtk_label_set_text(GTK_LABEL(panel->status_label), "Waiting for approval");
    } else if (event == CODEX_EVENT_APPROVAL_RESOLVED) {
        gtk_widget_set_visible(panel->approval_box, FALSE);
        if (panel->turn_active) {
            gtk_label_set_text(GTK_LABEL(panel->status_label), "Codex is working…");
        }
    } else if (event == CODEX_EVENT_DIFF_UPDATED) {
        g_free(panel->turn_diff);
        panel->turn_diff = g_strdup(text ? text : "");
        panel_update_changed_files(panel);
        gtk_widget_set_visible(panel->review_box,
                               panel->turn_diff[0] != '\0');
    }
}
