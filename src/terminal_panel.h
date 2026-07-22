/**
 * @file src/terminal_panel.h
 * @brief Integrated VTE terminal panel.
 * @details The terminal is wrapped instead of exposing VTE everywhere. Project-directory
 *          tabs, child cleanup, sizing, and close behavior are Graptoς policy, so the rest
 *          of the app talks to this panel boundary.
 */

#ifndef GRAPTOS_TERMINAL_PANEL_H
#define GRAPTOS_TERMINAL_PANEL_H

#include <gtk/gtk.h>

/**
 * @brief Editor window forward declaration.
 */
typedef struct _EditorWindow EditorWindow;

/**
 * @brief Terminal panel opaque type.
 */
typedef struct _TerminalPanel TerminalPanel;

/**
 * @brief Create an integrated terminal panel for a window.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
TerminalPanel *terminal_panel_new(EditorWindow *win);

/**
 * @brief Free an integrated terminal panel and release its child process.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 */
void terminal_panel_free(TerminalPanel *panel);

/**
 * @brief Return the root widget for an integrated terminal panel.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GtkWidget *terminal_panel_widget(TerminalPanel *panel);

/**
 * @brief Toggle terminal visibility and spawn a shell when first shown.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 */
void terminal_panel_toggle(TerminalPanel *panel);

/**
 * @brief Return whether the terminal panel is currently open.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean terminal_panel_is_open(TerminalPanel *panel);

/**
 * @brief Synchronize terminal tabs with the active editor file.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 */
void terminal_panel_sync_to_active_file(TerminalPanel *panel);

/**
 * @brief Apply current editor colors to the terminal widget.
 * @details Terminal handling crosses GTK widgets, VTE child processes, and project directories. The comment keeps the lifetime rule visible so closing tabs and shell exits do not race each other.
 * @param panel The panel instance affected by the operation.
 */
void terminal_panel_apply_colors(TerminalPanel *panel);

#endif /* GRAPTOS_TERMINAL_PANEL_H */
