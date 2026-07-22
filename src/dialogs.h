/**
 * @file src/dialogs.h
 * @brief Dialog helper declarations and implementation.
 * @details Dialogs should feel like the same app no matter which feature opened them. We
 *          centralize the GTK presentation rules so errors and command output do not drift
 *          into slightly different designs.
 */

#ifndef GRAPTOS_DIALOGS_H
#define GRAPTOS_DIALOGS_H

#include <gtk/gtk.h>

/**
 * @brief One action shown in a Graptoς dialog action row.
 * @details Dialog buttons should line up the same way even when the caller
 *          needs custom responses. The action description keeps labels,
 *          response ids, and visual intent together instead of spreading that
 *          wiring across each dialog body.
 */
typedef struct {
    const char *label; /**< Visible button text. */
    const char *tooltip; /**< Optional tooltip text. */
    int response; /**< Response id returned when the button is pressed. */
    gboolean destructive; /**< TRUE when the action can discard user work. */
    gboolean default_action; /**< TRUE when the action is the safest primary path. */
} GraptosDialogAction;

/**
 * @brief Run a standard message dialog with caller-provided actions.
 * @details This is the shared path for short decision dialogs. The caller
 *          provides text and responses; the helper owns window creation,
 *          spacing, button classes, modality, and cleanup.
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
                       int default_response);

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
                   const char *body);
/**
 * @brief Dialog output with Git-aware highlighting.
 * @details Git command dialogs are the one dialog family that intentionally
 *          keeps a text area. The caller passes configured theme colors so
 *          status letters, diff lines, and command failures stay readable
 *          without inventing another dialog widget.
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
                       const char *background_color);
/**
 * @brief Dialog error.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param primary The primary supplied by the caller.
 * @param detail The detail supplied by the caller.
 */
void dialog_error(GtkWindow *parent, const char *primary, const char *detail);
/**
 * @brief Dialog info.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param primary The primary supplied by the caller.
 * @param detail The detail supplied by the caller.
 */
void dialog_info(GtkWindow *parent, const char *primary, const char *detail);
/**
 * @brief Dialog prompt text.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @param label The label supplied by the caller.
 * @param initial The initial supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *dialog_prompt_text(GtkWindow *parent, const char *title, const char *label, const char *initial);
/**
 * @brief Dialog confirm yes no.
 * @details Dialogs sit on the edge between command failures and the active window. The comment keeps modality, parent ownership, and response handling visible at the call site.
 * @param parent The parent supplied by the caller.
 * @param title The title supplied by the caller.
 * @param message The message supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean dialog_confirm_yes_no(GtkWindow *parent, const char *title, const char *message);

#endif /* GRAPTOS_DIALOGS_H */
