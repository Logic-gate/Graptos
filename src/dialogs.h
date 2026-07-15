/**
 * @file src/dialogs.h
 * @brief Dialog helper declarations and implementation.
 */

#ifndef CLEAF_DIALOGS_H
#define CLEAF_DIALOGS_H

#include <gtk/gtk.h>

/**
 * @brief Dialog error.
 */
void dialog_error(GtkWindow *parent, const char *primary, const char *detail);
/**
 * @brief Dialog info.
 */
void dialog_info(GtkWindow *parent, const char *primary, const char *detail);
/**
 * @brief Dialog prompt text.
 */
char *dialog_prompt_text(GtkWindow *parent, const char *title, const char *label, const char *initial);
/**
 * @brief Dialog confirm yes no.
 */
gboolean dialog_confirm_yes_no(GtkWindow *parent, const char *title, const char *message);

#endif /* CLEAF_DIALOGS_H */
