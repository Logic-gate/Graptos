/**
 * @file src/codex_panel.h
 * @brief Codex sidebar panel and chat review UI.
 * @details AI touches the parts of the app where mistakes are expensive: files,
 *          permissions, markdown, and long-running processes. We keep protocol, client,
 *          panel, and review logic separated so approval and cleanup paths are easy to
 *          audit.
 */

#ifndef GRAPTOS_CODEX_PANEL_H
#define GRAPTOS_CODEX_PANEL_H

#include <gtk/gtk.h>

#include "codex_client.h"

/**
 * @brief Codex panel type definition.
 * @details Forward declaration keeps app/window headers from depending on panel
 *          internals.
 */
typedef struct _EditorWindow EditorWindow;
/**
 * @brief Codex panel type definition.
 * @details Opaque panel state; callers interact through the small API below.
 */
typedef struct _CodexPanel CodexPanel;

/**
 * @brief Codex panel new.
 * @details Builds a hidden sidebar for the owning editor window.
 */
CodexPanel *codex_panel_new(EditorWindow *win);
/**
 * @brief Codex panel free.
 * @details Releases chats, pending render state, and sidebar-owned data.
 * @param panel The panel instance affected by the operation.
 */
void codex_panel_free(CodexPanel *panel);
/**
 * @brief Codex panel widget.
 * @details Returns the revealer widget that should be packed into the layout.
 * @param panel The panel instance affected by the operation.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *codex_panel_widget(CodexPanel *panel);
/**
 * @brief Codex panel toggle.
 * @details Shows or hides the sidebar without destroying current chats.
 * @param panel The panel instance affected by the operation.
 */
void codex_panel_toggle(CodexPanel *panel);
/**
 * @brief Codex panel refresh context.
 * @details Updates context labels and selection availability from the active tab.
 * @param panel The panel instance affected by the operation.
 */
void codex_panel_refresh_context(CodexPanel *panel);
/**
 * @brief Codex panel set connection.
 * @details Applies CodexClient connection state to panel controls.
 * @param panel The panel instance affected by the operation.
 * @param state The state supplied by the caller.
 * @param detail The detail supplied by the caller.
 */
void codex_panel_set_connection(CodexPanel *panel,
                                CodexClientState state,
                                const char *detail);
/**
 * @brief Codex panel handle event.
 * @details Consumes normalized CodexClient events for the owning window.
 * @param client The client instance that owns the protocol state.
 * @param event The event supplied by the caller.
 * @param text The text fragment supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
void codex_panel_handle_event(CodexClient *client,
                              CodexClientEvent event,
                              const char *text,
                              gpointer user_data);

#endif /* GRAPTOS_CODEX_PANEL_H */
