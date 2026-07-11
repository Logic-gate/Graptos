#ifndef CLEAF_DIALOGS_H
#define CLEAF_DIALOGS_H

#include <gtk/gtk.h>

void dialog_error(GtkWindow *parent, const char *primary, const char *detail);
void dialog_info(GtkWindow *parent, const char *primary, const char *detail);
char *dialog_prompt_text(GtkWindow *parent, const char *title, const char *label, const char *initial);
gboolean dialog_confirm_yes_no(GtkWindow *parent, const char *title, const char *message);

#endif /* CLEAF_DIALOGS_H */
