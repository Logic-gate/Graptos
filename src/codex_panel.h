/**
 * @file src/codex_panel.h
 * @brief Codex sidebar panel and chat review UI.
 */

#ifndef CLEAF_CODEX_PANEL_H
#define CLEAF_CODEX_PANEL_H

#include <gtk/gtk.h>

#include "codex_client.h"

/**
 * @brief Codex panel type definition.
 */
typedef struct _EditorWindow EditorWindow;
/**
 * @brief Codex panel type definition.
 */
typedef struct _CodexPanel CodexPanel;

/**
 * @brief Codex panel new.
 */
CodexPanel *codex_panel_new(EditorWindow *win);
/**
 * @brief Codex panel free.
 */
void codex_panel_free(CodexPanel *panel);
/**
 * @brief Codex panel widget.
 */
GtkWidget *codex_panel_widget(CodexPanel *panel);
/**
 * @brief Codex panel toggle.
 */
void codex_panel_toggle(CodexPanel *panel);
/**
 * @brief Codex panel refresh context.
 */
void codex_panel_refresh_context(CodexPanel *panel);
/**
 * @brief Codex panel set connection.
 */
void codex_panel_set_connection(CodexPanel *panel,
                                CodexClientState state,
                                const char *detail);
/**
 * @brief Codex panel handle event.
 */
void codex_panel_handle_event(CodexClient *client,
                              CodexClientEvent event,
                              const char *text,
                              gpointer user_data);

#endif /* CLEAF_CODEX_PANEL_H */
