#ifndef CLEAF_CODEX_PANEL_H
#define CLEAF_CODEX_PANEL_H

#include <gtk/gtk.h>

#include "codex_client.h"

typedef struct _EditorWindow EditorWindow;
typedef struct _CodexPanel CodexPanel;

CodexPanel *codex_panel_new(EditorWindow *win);
void codex_panel_free(CodexPanel *panel);
GtkWidget *codex_panel_widget(CodexPanel *panel);
void codex_panel_toggle(CodexPanel *panel);
void codex_panel_refresh_context(CodexPanel *panel);
void codex_panel_set_connection(CodexPanel *panel,
                                CodexClientState state,
                                const char *detail);
void codex_panel_handle_event(CodexClient *client,
                              CodexClientEvent event,
                              const char *text,
                              gpointer user_data);

#endif /* CLEAF_CODEX_PANEL_H */
