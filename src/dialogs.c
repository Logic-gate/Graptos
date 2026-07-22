/**
 * @file src/dialogs.c
 * @brief Dialog helper declarations and implementation.
 * @details Dialogs should feel like the same app no matter which feature opened them. We
 *          centralize the GTK presentation rules so errors and command output do not drift
 *          into slightly different designs.
 */

#include "dialogs.h"
#include "ui.h"

#include <gtksourceview/gtksource.h>
#include <string.h>

/**
 * @brief Dialog window new.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @param width The width supplied by the caller.
 * @param height The height supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *dialog_window_new(GtkWindow *parent,
                                    const char *title,
                                    int width,
                                    int height) {
    GtkWidget *window = gtk_window_new();
    gtk_widget_add_css_class(window, "graptos-window");
    gtk_widget_add_css_class(window, "graptos-dialog");
    gtk_window_set_title(GTK_WINDOW(window), title ? title : "Graptoς");
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    if (parent) {
        gtk_window_set_transient_for(GTK_WINDOW(window), parent);
        gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
    }
    return window;
}

/**
 * @brief Dialog content new.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param window The window supplied by the caller.
 * @param output_dialog TRUE when the content hosts command or error output.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *dialog_content_new(GtkWidget *window, gboolean output_dialog) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_add_css_class(box, "graptos-root");
    gtk_widget_add_css_class(box, "graptos-dialog-root");
    gtk_widget_add_css_class(box, "graptos-flat-dialog");
    if (output_dialog) {
        gtk_widget_add_css_class(box, "graptos-output-dialog-root");
    }
    graptos_set_all_margins(box, 14);
    gtk_window_set_child(GTK_WINDOW(window), box);
    return box;
}

/**
 * @brief Dialog label new.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param text The text fragment supplied by the caller.
 * @param title The title supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *dialog_label_new(const char *text, gboolean title) {
    GtkWidget *label = gtk_label_new(text ? text : "");
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    if (title) {
        gtk_widget_add_css_class(label, "graptos-menu-title");
        gtk_widget_add_css_class(label, "graptos-dialog-title");
    } else {
        gtk_widget_add_css_class(label, "graptos-dialog-body");
    }
    return label;
}

/**
 * @brief Button row new.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static GtkWidget *button_row_new(void) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(row, "graptos-dialog-actions");
    gtk_widget_set_halign(row, GTK_ALIGN_END);
    return row;
}

/**
 * @brief Create one standard dialog action button.
 * @details Buttons carry their semantic classes from the action descriptor.
 *          This keeps destructive and primary colors consistent across every
 *          dialog that asks the user to decide something.
 * @param action The action descriptor supplied by the caller.
 * @return The newly-created button widget.
 */
static GtkWidget *dialog_action_button_new(const GraptosDialogAction *action) {
    GtkWidget *button = graptos_flat_button_new(
        action && action->label ? action->label : "OK",
        action ? action->tooltip : NULL,
        G_CALLBACK(graptos_modal_window_respond),
        GINT_TO_POINTER(action ? action->response : GTK_RESPONSE_OK));
    gtk_widget_add_css_class(button, "graptos-dialog-action");
    if (action && action->destructive) {
        gtk_widget_add_css_class(button, "graptos-dialog-action-destructive");
    }
    if (action && action->default_action) {
        gtk_widget_add_css_class(button, "graptos-dialog-action-default");
    }
    return button;
}

/**
 * @brief Return TRUE when text contains a case-insensitive token.
 * @details Dialog output is shared by Git status, Git diff, and command
 *          messages. Small local detection keeps the caller API stable while
 *          letting generated Git text get the most useful highlighting.
 * @param text The text fragment supplied by the caller.
 * @param token The token searched for inside text.
 * @return TRUE when token appears in text.
 */
static gboolean dialog_text_contains_ci(const char *text, const char *token) {
    if (!text || !token || token[0] == '\0') return FALSE;
    g_autofree char *haystack = g_utf8_strdown(text, -1);
    g_autofree char *needle = g_utf8_strdown(token, -1);
    return haystack && needle && strstr(haystack, needle) != NULL;
}

/**
 * @brief Resolve the GtkSource language for an output dialog.
 * @details Git diff output has a real language definition in GtkSourceView.
 *          Other Git messages are highlighted with local tags instead because
 *          status and stderr blocks are not one source language.
 * @param title The dialog title supplied by the caller.
 * @param heading The dialog heading supplied by the caller.
 * @param body The dialog body supplied by the caller.
 * @return The GtkSource language to apply, or NULL for local highlighting only.
 */
static GtkSourceLanguage *dialog_output_language(const char *title,
                                                const char *heading,
                                                const char *body) {
    gboolean looks_like_diff =
        dialog_text_contains_ci(title, "diff") ||
        dialog_text_contains_ci(heading, "diff") ||
        dialog_text_contains_ci(body, "diff --git") ||
        dialog_text_contains_ci(body, "@@ ");
    if (!looks_like_diff) return NULL;

    GtkSourceLanguageManager *manager =
        gtk_source_language_manager_get_default();
    return manager ? gtk_source_language_manager_get_language(manager, "diff")
                   : NULL;
}

/**
 * @brief Apply lightweight Git message tags to output text.
 * @details Git status and Git error dialogs are structured command output, but
 *          not a single source language. Tags make headers, stderr, and status
 *          letters readable without adding separate dialog types.
 * @param buffer The buffer that owns the dialog output text.
 * @param body The dialog body supplied by the caller.
 * @param added_color The configured added-line/status color.
 * @param modified_color The configured modified-line/status color.
 * @param deleted_color The configured deleted-line/status color.
 * @param error_color The configured error color.
 * @param muted_color The configured muted/supporting text color.
 */
static void dialog_apply_git_output_tags(GtkTextBuffer *buffer,
                                         const char *body,
                                         const char *added_color,
                                         const char *modified_color,
                                         const char *deleted_color,
                                         const char *error_color,
                                         const char *muted_color) {
    if (!buffer || !body || body[0] == '\0') return;

    const char *added = added_color && added_color[0] == '#'
        ? added_color : "#57cc99";
    const char *modified = modified_color && modified_color[0] == '#'
        ? modified_color : "#f9c74f";
    const char *deleted = deleted_color && deleted_color[0] == '#'
        ? deleted_color : "#ff6b6b";
    const char *error = error_color && error_color[0] == '#'
        ? error_color : "#ff6b6b";
    const char *muted = muted_color && muted_color[0] == '#'
        ? muted_color : "#a6adc8";

    GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buffer);
    if (!gtk_text_tag_table_lookup(table, "git-heading")) {
        gtk_text_buffer_create_tag(buffer, "git-heading",
                                   "weight", PANGO_WEIGHT_BOLD,
                                   "foreground", muted,
                                   NULL);
        gtk_text_buffer_create_tag(buffer, "git-added",
                                   "foreground", added,
                                   NULL);
        gtk_text_buffer_create_tag(buffer, "git-modified",
                                   "foreground", modified,
                                   NULL);
        gtk_text_buffer_create_tag(buffer, "git-deleted",
                                   "foreground", deleted,
                                   NULL);
        gtk_text_buffer_create_tag(buffer, "git-error",
                                   "foreground", error,
                                   NULL);
        gtk_text_buffer_create_tag(buffer, "git-muted",
                                   "foreground", muted,
                                   NULL);
    }

    GtkTextIter line_start;
    gtk_text_buffer_get_start_iter(buffer, &line_start);
    while (!gtk_text_iter_is_end(&line_start)) {
        GtkTextIter line_end = line_start;
        if (!gtk_text_iter_ends_line(&line_end)) {
            gtk_text_iter_forward_to_line_end(&line_end);
        }
        g_autofree char *line = gtk_text_iter_get_text(&line_start, &line_end);
        const char *tag = NULL;
        if (line && (g_str_has_suffix(line, ":")
                     || g_str_has_prefix(line, "@@"))) {
            tag = "git-heading";
        } else if (line && (g_str_has_prefix(line, "stderr")
                            || g_str_has_prefix(line, "fatal:")
                            || g_str_has_prefix(line, "error:"))) {
            tag = "git-error";
        } else if (line && (g_str_has_prefix(line, "diff --git")
                            || g_str_has_prefix(line, "index ")
                            || g_str_has_prefix(line, "+++")
                            || g_str_has_prefix(line, "---")
                            || g_str_has_prefix(line, "Exit code:")
                            || g_str_has_prefix(line, "Repository:")
                            || g_str_has_prefix(line, "Branch:"))) {
            tag = "git-muted";
        } else if (line && (g_str_has_prefix(line, "A ")
                            || g_str_has_prefix(line, "?? ")
                            || g_str_has_prefix(line, "+"))) {
            tag = "git-added";
        } else if (line && (g_str_has_prefix(line, "M ")
                            || g_str_has_prefix(line, " M")
                            || g_str_has_prefix(line, "MM"))) {
            tag = "git-modified";
        } else if (line && (g_str_has_prefix(line, "D ")
                            || g_str_has_prefix(line, " D")
                            || g_str_has_prefix(line, "-"))) {
            tag = "git-deleted";
        }
        if (tag) gtk_text_buffer_apply_tag_by_name(buffer, tag,
                                                   &line_start, &line_end);
        if (!gtk_text_iter_forward_line(&line_start)) break;
    }
}

/**
 * @brief Run a standard message dialog with caller-provided actions.
 * @details Short dialogs should not invent their own margins, button rows, or
 *          close behavior. This path gives each caller custom responses while
 *          keeping the visual system flat and predictable.
 * @param parent The parent window used for transient modality.
 * @param window_title The native window title.
 * @param heading The prominent heading shown in the dialog content.
 * @param body The explanatory body text.
 * @param width The requested default width.
 * @param height The requested default height.
 * @param actions The action descriptors shown from left to right.
 * @param n_actions The number of action descriptors.
 * @param default_response The response returned when the dialog is closed.
 * @return The selected response id, or default_response when no action wins.
 */
int dialog_run_message(GtkWindow *parent,
                       const char *window_title,
                       const char *heading,
                       const char *body,
                       int width,
                       int height,
                       const GraptosDialogAction *actions,
                       gsize n_actions,
                       int default_response) {
    GtkWidget *window = dialog_window_new(parent,
                                          window_title ? window_title : "Graptoς",
                                          width > 0 ? width : 460,
                                          height > 0 ? height : 170);
    GtkWidget *box = dialog_content_new(window, FALSE);
    gtk_box_append(GTK_BOX(box), dialog_label_new(heading ? heading : window_title,
                                                  TRUE));
    if (body && body[0]) {
        gtk_box_append(GTK_BOX(box), dialog_label_new(body, FALSE));
    }

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_vexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(box), spacer);

    GtkWidget *row = button_row_new();
    if (actions && n_actions > 0u) {
        for (gsize i = 0; i < n_actions; i++) {
            gtk_box_append(GTK_BOX(row), dialog_action_button_new(&actions[i]));
        }
    } else {
        GraptosDialogAction ok = { "OK", NULL, GTK_RESPONSE_OK, FALSE, TRUE };
        gtk_box_append(GTK_BOX(row), dialog_action_button_new(&ok));
    }
    gtk_box_append(GTK_BOX(box), row);

    int response = graptos_modal_window_run(GTK_WINDOW(window),
                                            default_response);
    graptos_widget_destroy(window);
    return response;
}

/**
 * @brief Run the shared output dialog.
 * @details Git command output is the only dialog content that should behave
 *          like a scrollable text document. The shared implementation keeps
 *          the text area singular while allowing optional Git highlighting.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @param heading The heading supplied by the caller.
 * @param body The body supplied by the caller.
 * @param git_highlight TRUE when Git output tags should be applied.
 * @param added_color The configured added-line/status color.
 * @param modified_color The configured modified-line/status color.
 * @param deleted_color The configured deleted-line/status color.
 * @param error_color The configured error color.
 * @param muted_color The configured muted/supporting text color.
 * @param background_color The configured Git output background color.
 */
static void dialog_output_run(GtkWindow *parent,
                              const char *title,
                              const char *heading,
                              const char *body,
                              gboolean git_highlight,
                              const char *added_color,
                              const char *modified_color,
                              const char *deleted_color,
                              const char *error_color,
                              const char *muted_color,
                              const char *background_color) {
    GtkWidget *window = dialog_window_new(parent, title ? title : "Graptoς",
                                          760, 520);
    GtkWidget *box = dialog_content_new(window, TRUE);
    if (git_highlight) {
        gtk_widget_add_css_class(box, "graptos-git-output-dialog-root");
    }
    (void)background_color;

    GtkWidget *label = dialog_label_new(heading ? heading : title, TRUE);
    gtk_box_append(GTK_BOX(box), label);

    GtkSourceBuffer *buffer = gtk_source_buffer_new(NULL);
    gtk_source_buffer_set_style_scheme(buffer, NULL);
    GtkSourceLanguage *language = dialog_output_language(title, heading, body);
    if (language) gtk_source_buffer_set_language(buffer, language);
    gtk_source_buffer_set_highlight_syntax(buffer, TRUE);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(buffer), body ? body : "", -1);
    if (git_highlight) {
        dialog_apply_git_output_tags(GTK_TEXT_BUFFER(buffer),
                                     body,
                                     added_color,
                                     modified_color,
                                     deleted_color,
                                     error_color,
                                     muted_color);
    }
    GtkWidget *view = gtk_source_view_new_with_buffer(buffer);
    g_object_unref(buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_NONE);
    gtk_source_view_set_show_line_numbers(GTK_SOURCE_VIEW(view), FALSE);
    gtk_source_view_set_highlight_current_line(GTK_SOURCE_VIEW(view), FALSE);
    gtk_widget_add_css_class(view, "graptos-dialog-output");
    if (git_highlight) {
        gtk_widget_add_css_class(view, "graptos-git-dialog-output");
    }

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_add_css_class(scrolled, "graptos-dialog-output-scroll");
    if (git_highlight) {
        gtk_widget_add_css_class(scrolled, "graptos-git-dialog-output-scroll");
    }
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view);
    gtk_box_append(GTK_BOX(box), scrolled);

    GtkWidget *row = button_row_new();
    GraptosDialogAction close_action = { "Close", NULL, GTK_RESPONSE_CLOSE,
                                         FALSE, TRUE };
    GtkWidget *close = dialog_action_button_new(&close_action);
    gtk_box_append(GTK_BOX(row), close);
    gtk_box_append(GTK_BOX(box), row);

    (void)graptos_modal_window_run(GTK_WINDOW(window), GTK_RESPONSE_CLOSE);
    graptos_widget_destroy(window);
}

/**
 * @brief Dialog output.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @param heading The heading supplied by the caller.
 * @param body The body supplied by the caller.
 */
void dialog_output(GtkWindow *parent,
                   const char *title,
                   const char *heading,
                   const char *body) {
    dialog_output_run(parent, title, heading, body, FALSE,
                      NULL, NULL, NULL, NULL, NULL, NULL);
}

/**
 * @brief Dialog output with Git-aware highlighting.
 * @details Git output is command text, not a normal message. Highlighting
 *          status letters and diff lines makes the single output dialog easier
 *          to scan while preserving the flat dialog shell.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @param heading The heading supplied by the caller.
 * @param body The body supplied by the caller.
 * @param added_color The configured added-line/status color.
 * @param modified_color The configured modified-line/status color.
 * @param deleted_color The configured deleted-line/status color.
 * @param error_color The configured error color.
 * @param muted_color The configured muted/supporting text color.
 * @param background_color The configured Git output background color.
 */
void dialog_output_git(GtkWindow *parent,
                       const char *title,
                       const char *heading,
                       const char *body,
                       const char *added_color,
                       const char *modified_color,
                       const char *deleted_color,
                       const char *error_color,
                       const char *muted_color,
                       const char *background_color) {
    dialog_output_run(parent, title, heading, body, TRUE,
                      added_color, modified_color, deleted_color,
                      error_color, muted_color, background_color);
}

/**
 * @brief Dialog error.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param primary The primary supplied by the caller.
 * @param detail The detail supplied by the caller.
 */
void dialog_error(GtkWindow *parent, const char *primary, const char *detail) {
    GraptosDialogAction actions[] = {
        { "Close", NULL, GTK_RESPONSE_CLOSE, FALSE, TRUE },
    };
    (void)dialog_run_message(parent,
                             "Error",
                             primary ? primary : "Error",
                             detail,
                             500,
                             180,
                             actions,
                             G_N_ELEMENTS(actions),
                             GTK_RESPONSE_CLOSE);
}

/**
 * @brief Dialog info.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param primary The primary supplied by the caller.
 * @param detail The detail supplied by the caller.
 */
void dialog_info(GtkWindow *parent, const char *primary, const char *detail) {
    GraptosDialogAction actions[] = {
        { "Close", NULL, GTK_RESPONSE_CLOSE, FALSE, TRUE },
    };
    (void)dialog_run_message(parent,
                             "Information",
                             primary ? primary : "Information",
                             detail && detail[0] ? detail : primary,
                             500,
                             180,
                             actions,
                             G_N_ELEMENTS(actions),
                             GTK_RESPONSE_CLOSE);
}

/**
 * @brief Dialog prompt text.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @param label The label supplied by the caller.
 * @param initial The initial supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *dialog_prompt_text(GtkWindow *parent,
                         const char *title,
                         const char *label,
                         const char *initial) {
    GtkWidget *window = dialog_window_new(parent, title ? title : "Input",
                                          460, 150);
    GtkWidget *box = dialog_content_new(window, FALSE);
    gtk_box_append(GTK_BOX(box), dialog_label_new(label ? label : "Value:",
                                                  FALSE));

    GtkWidget *entry = gtk_entry_new();
    if (initial) gtk_entry_set_text(GTK_ENTRY(entry), initial);
    gtk_box_append(GTK_BOX(box), entry);

    GtkWidget *row = button_row_new();
    GraptosDialogAction cancel_action = { "Cancel", NULL, GTK_RESPONSE_CANCEL,
                                          FALSE, FALSE };
    GraptosDialogAction ok_action = { "OK", NULL, GTK_RESPONSE_ACCEPT,
                                      FALSE, TRUE };
    GtkWidget *cancel = dialog_action_button_new(&cancel_action);
    GtkWidget *ok = dialog_action_button_new(&ok_action);
    gtk_box_append(GTK_BOX(row), cancel);
    gtk_box_append(GTK_BOX(row), ok);
    gtk_box_append(GTK_BOX(box), row);

    gtk_widget_grab_focus(entry);
    char *result = NULL;
    if (graptos_modal_window_run(GTK_WINDOW(window), GTK_RESPONSE_CANCEL)
        == GTK_RESPONSE_ACCEPT) {
        result = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    }
    graptos_widget_destroy(window);
    return result;
}

/**
 * @brief Dialog confirm yes no.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @param message The message supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean dialog_confirm_yes_no(GtkWindow *parent,
                               const char *title,
                               const char *message) {
    GraptosDialogAction actions[] = {
        { "No", NULL, GTK_RESPONSE_NO, FALSE, TRUE },
        { "Yes", NULL, GTK_RESPONSE_YES, FALSE, FALSE },
    };
    return dialog_run_message(parent,
                              title ? title : "Confirm",
                              title ? title : "Confirm",
                              message,
                              440,
                              160,
                              actions,
                              G_N_ELEMENTS(actions),
                              GTK_RESPONSE_NO) == GTK_RESPONSE_YES;
}
