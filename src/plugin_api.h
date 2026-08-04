/**
 * @file src/plugin_api.h
 * @brief Public ABI surface for trusted native Graptoς plugins.
 * @details Native plugins must use this small host object instead of reaching
 *          into EditorWindow, EditorTab, or GTK internals. Keeping the ABI
 *          opaque lets Graptoς change editor implementation details without
 *          breaking plugin source.
 */

#ifndef GRAPTOS_PLUGIN_API_H
#define GRAPTOS_PLUGIN_API_H

#include <glib.h>

/**
 * @brief Graptoς plugin API version.
 */
#define GRAPTOS_PLUGIN_API_VERSION 1u

/**
 * @brief Opaque host passed to native plugins.
 */
typedef struct _GraptosPluginHost GraptosPluginHost;

/**
 * @brief Opaque command execution context passed to native plugins.
 */
typedef struct _GraptosPluginCommandContext GraptosPluginCommandContext;

/**
 * @brief Native plugin command callback.
 * @details Graptoς calls this when a registered command is invoked from a menu,
 *          tool panel, shortcut, or a later command surface.
 * @param context Opaque execution context for the current editor command.
 * @param user_data Plugin data supplied during command registration.
 */
typedef void (*GraptosPluginCommandFunc)(GraptosPluginCommandContext *context,
                                         gpointer user_data);

/**
 * @brief Native plugin completion callback.
 * @details Providers inspect the command context and return insertable strings
 *          for the current cursor. Set replace_prefix_out to the exact text
 *          Graptoς should replace when a candidate is accepted.
 * @param context Opaque execution context for the current editor cursor.
 * @param replace_prefix_out Owned replacement prefix returned by the plugin.
 * @param user_data Plugin data supplied during provider registration.
 * @return GPtrArray of owned char* candidates, or NULL when there are none.
 */
typedef GPtrArray *(*GraptosPluginCompletionFunc)(GraptosPluginCommandContext *context,
                                                  char **replace_prefix_out,
                                                  gpointer user_data);

/**
 * @brief Native plugin hover callback.
 * @details Providers inspect the current hover word and may return a short
 *          plain-text body for Graptoς to show in the hover popover.
 * @param context Opaque execution context for the current editor hover.
 * @param word Word or token under the pointer.
 * @param user_data Plugin data supplied during provider registration.
 * @return Owned hover body, or NULL when the provider does not apply.
 */
typedef char *(*GraptosPluginHoverFunc)(GraptosPluginCommandContext *context,
                                        const char *word,
                                        gpointer user_data);

/**
 * @brief Native plugin hub render callback.
 * @details Hubs provide one focused tool surface without adding many command
 *          rows. The returned text is shown in the tool panel.
 * @param context Command context for the active editor/project.
 * @param user_data Plugin data supplied during hub registration.
 * @return Owned plain-text hub body.
 */
typedef char *(*GraptosPluginHubRenderFunc)(GraptosPluginCommandContext *context,
                                            gpointer user_data);

/**
 * @brief Native plugin hub action callback.
 * @details Graptoς owns the compact hub controls and passes action ids plus
 *          prompt text to the plugin.
 * @param context Command context for the active editor/project.
 * @param action_id Stable action id such as add, modify, done, delete, priority.
 * @param input Prompt text supplied by the user, or NULL.
 * @param user_data Plugin data supplied during hub registration.
 */
typedef void (*GraptosPluginHubActionFunc)(GraptosPluginCommandContext *context,
                                           const char *action_id,
                                           const char *input,
                                           gpointer user_data);

/**
 * @brief Native plugin data destroy callback.
 * @param user_data Plugin-owned data supplied during command registration.
 */
typedef void (*GraptosPluginDestroyFunc)(gpointer user_data);

/**
 * @brief Native plugin registration function.
 * @details Shared libraries export this symbol when they need code-backed
 *          editor automation. Declarative-only plugins do not need a native
 *          entrypoint.
 * @param host The host capability object supplied by Graptoς.
 * @return TRUE when registration succeeds.
 */
typedef gboolean (*GraptosPluginRegisterFunc)(GraptosPluginHost *host);

/**
 * @brief Return the plugin API version implemented by the host.
 * @details Plugins can use this during registration to reject incompatible
 *          Graptoς builds before registering commands.
 * @param host The host capability object supplied by Graptoς.
 * @return The integer API version.
 */
guint graptos_plugin_host_api_version(GraptosPluginHost *host);

/**
 * @brief Return the id of the plugin currently being registered.
 * @details The id is owned by Graptoς and remains valid only while the host is
 *          alive. Plugins should copy it if they need to keep it.
 * @param host The host capability object supplied by Graptoς.
 * @return The plugin id, or NULL when unavailable.
 */
const char *graptos_plugin_host_plugin_id(GraptosPluginHost *host);

/**
 * @brief Register a native command implementation.
 * @details This registers behavior only. Use
 *          graptos_plugin_host_register_editor_line_command() when the command
 *          should also appear in editor line menus and the Plugins tool panel.
 * @param host The host capability object supplied by Graptoς.
 * @param command_id Command id declared by the manifest.
 * @param callback Native function to run for the command.
 * @param user_data Plugin data passed to the callback.
 * @param destroy Optional destroy callback for user_data.
 * @return TRUE when the command was accepted.
 */
gboolean graptos_plugin_host_register_command(GraptosPluginHost *host,
                                              const char *command_id,
                                              GraptosPluginCommandFunc callback,
                                              gpointer user_data,
                                              GraptosPluginDestroyFunc destroy);

/**
 * @brief Register a native editor line command.
 * @details The command appears in editor right-click menus and the Plugins tool
 *          panel. The YAML manifest keeps plugin identity and permissions; the
 *          shared library owns this behavior and visible label.
 * @param host The host capability object supplied by Graptoς.
 * @param command_id Stable command id owned by the plugin.
 * @param label Human-facing command label.
 * @param callback Native function to run for the command.
 * @param user_data Plugin data passed to the callback.
 * @param destroy Optional destroy callback for user_data.
 * @return TRUE when the command was accepted.
 */
gboolean graptos_plugin_host_register_editor_line_command(GraptosPluginHost *host,
                                                          const char *command_id,
                                                          const char *label,
                                                          GraptosPluginCommandFunc callback,
                                                          gpointer user_data,
                                                          GraptosPluginDestroyFunc destroy);

/**
 * @brief Register a native editor line command with a shortcut.
 * @details The shortcut is a stable text id such as `Ctrl+Alt+P`. Graptoς owns
 *          key matching and invokes the registered command when it is pressed.
 * @param host The host capability object supplied by Graptoς.
 * @param command_id Stable command id owned by the plugin.
 * @param label Human-facing command label.
 * @param shortcut Shortcut id, or NULL for no shortcut.
 * @param callback Native function to run for the command.
 * @param user_data Plugin data passed to the callback.
 * @param destroy Optional destroy callback for user_data.
 * @return TRUE when the command was accepted.
 */
gboolean graptos_plugin_host_register_editor_line_command_with_shortcut(GraptosPluginHost *host,
                                                                        const char *command_id,
                                                                        const char *label,
                                                                        const char *shortcut,
                                                                        GraptosPluginCommandFunc callback,
                                                                        gpointer user_data,
                                                                        GraptosPluginDestroyFunc destroy);

/**
 * @brief Register a native completion provider.
 * @details Providers are asked during normal editor completion. The provider
 *          decides when it applies by returning candidates only for matching
 *          cursor context.
 * @param host The host capability object supplied by Graptoς.
 * @param provider_id Stable provider id owned by the plugin.
 * @param label Human-facing source label.
 * @param callback Native completion callback.
 * @param user_data Plugin data passed to the callback.
 * @param destroy Optional destroy callback for user_data.
 * @return TRUE when the provider was accepted.
 */
gboolean graptos_plugin_host_register_completion_provider(GraptosPluginHost *host,
                                                          const char *provider_id,
                                                          const char *label,
                                                          GraptosPluginCompletionFunc callback,
                                                          gpointer user_data,
                                                          GraptosPluginDestroyFunc destroy);

/**
 * @brief Register a native hover provider.
 * @details Providers are asked before built-in reference lookup. The first
 *          provider that returns text owns the hover body.
 * @param host The host capability object supplied by Graptoς.
 * @param provider_id Stable provider id owned by the plugin.
 * @param label Human-facing hover heading.
 * @param callback Native hover callback.
 * @param user_data Plugin data passed to the callback.
 * @param destroy Optional destroy callback for user_data.
 * @return TRUE when the provider was accepted.
 */
gboolean graptos_plugin_host_register_hover_provider(GraptosPluginHost *host,
                                                     const char *provider_id,
                                                     const char *label,
                                                     GraptosPluginHoverFunc callback,
                                                     gpointer user_data,
                                                     GraptosPluginDestroyFunc destroy);

/**
 * @brief Register a focused native plugin hub.
 * @details A hub appears as one plugin tool entry and opens a compact tool
 *          panel instead of contributing many command menu rows.
 * @param host The host capability object supplied by Graptoς.
 * @param hub_id Stable hub id owned by the plugin.
 * @param label Human-facing hub label.
 * @param render Render callback.
 * @param action Action callback.
 * @param user_data Plugin data passed to callbacks.
 * @param destroy Optional user data destroy hook.
 * @return TRUE when the hub was accepted.
 */
gboolean graptos_plugin_host_register_hub(GraptosPluginHost *host,
                                          const char *hub_id,
                                          const char *label,
                                          GraptosPluginHubRenderFunc render,
                                          GraptosPluginHubActionFunc action,
                                          gpointer user_data,
                                          GraptosPluginDestroyFunc destroy);

/**
 * @brief Return the plugin id for a command context.
 * @param context Command context supplied by Graptoς.
 * @return Plugin id, or NULL.
 */
const char *graptos_plugin_context_plugin_id(GraptosPluginCommandContext *context);

/**
 * @brief Return the command id for a command context.
 * @param context Command context supplied by Graptoς.
 * @return Command id, or NULL.
 */
const char *graptos_plugin_context_command_id(GraptosPluginCommandContext *context);

/**
 * @brief Return the one-based editor line targeted by a command.
 * @param context Command context supplied by Graptoς.
 * @return One-based line number, or 0 when no editor line is available.
 */
guint graptos_plugin_context_line(GraptosPluginCommandContext *context);

/**
 * @brief Return the active file path.
 * @details The returned string is owned by the caller and must be freed with
 *          g_free().
 * @param context Command context supplied by Graptoς.
 * @return Owned file path, or NULL for an unsaved editor.
 */
char *graptos_plugin_context_file_path(GraptosPluginCommandContext *context);

/**
 * @brief Return the active editor text.
 * @details The returned string is owned by the caller and must be freed with
 *          g_free().
 * @param context Command context supplied by Graptoς.
 * @return Owned editor text, or an empty string when unavailable.
 */
char *graptos_plugin_context_text(GraptosPluginCommandContext *context);

/**
 * @brief Return the active editor selection.
 * @details The returned string is owned by the caller and must be freed with
 *          g_free().
 * @param context Command context supplied by Graptoς.
 * @return Owned selected text, or an empty string when there is no selection.
 */
char *graptos_plugin_context_selection(GraptosPluginCommandContext *context);

/**
 * @brief Return text from one editor line.
 * @details The returned string is owned by the caller and must be freed with
 *          g_free().
 * @param context Command context supplied by Graptoς.
 * @param line One-based editor line.
 * @return Owned line text, or an empty string when unavailable.
 */
char *graptos_plugin_context_line_text(GraptosPluginCommandContext *context,
                                       guint line);

/**
 * @brief Return text before the cursor on the active line.
 * @details The returned string is owned by the caller and must be freed with
 *          g_free().
 * @param context Command context supplied by Graptoς.
 * @return Owned line prefix text, or an empty string when unavailable.
 */
char *graptos_plugin_context_line_prefix(GraptosPluginCommandContext *context);

/**
 * @brief Return the active syntax name.
 * @details Native plugins use this to stay language-aware without reading
 *          editor internals or duplicating Graptoς syntax detection.
 * @param context Command context supplied by Graptoς.
 * @return Syntax name owned by Graptoς, or NULL when no syntax is active.
 */
const char *graptos_plugin_context_syntax_name(GraptosPluginCommandContext *context);

/**
 * @brief Return whether the active editor has unsaved changes.
 * @details Plugins that invoke file-backed command line tools should use this
 *          before reading the on-disk file.
 * @param context Command context supplied by Graptoς.
 * @return TRUE when the active buffer differs from disk.
 */
gboolean graptos_plugin_context_is_modified(GraptosPluginCommandContext *context);

/**
 * @brief Insert text at the active cursor.
 * @param context Command context supplied by Graptoς.
 * @param text Text to insert.
 * @return TRUE when text was inserted.
 */
gboolean graptos_plugin_context_insert_text(GraptosPluginCommandContext *context,
                                            const char *text);

/**
 * @brief Replace the active selection or insert at cursor when nothing is selected.
 * @param context Command context supplied by Graptoς.
 * @param text Replacement text.
 * @return TRUE when the buffer was updated.
 */
gboolean graptos_plugin_context_replace_selection(GraptosPluginCommandContext *context,
                                                  const char *text);

/**
 * @brief Replace the entire active editor text.
 * @details This is intended for formatter-style plugins. Graptoς owns the GTK
 *          buffer edit so undo state and editor refreshes stay consistent.
 * @param context Command context supplied by Graptoς.
 * @param text New complete buffer text.
 * @return TRUE when the buffer was updated.
 */
gboolean graptos_plugin_context_replace_text(GraptosPluginCommandContext *context,
                                             const char *text);

/**
 * @brief Clear diagnostics currently shown in the active editor.
 * @details External tool plugins can clear stale results before publishing
 *          fresh diagnostics from a validation run.
 * @param context Command context supplied by Graptoς.
 */
void graptos_plugin_context_clear_diagnostics(GraptosPluginCommandContext *context);

/**
 * @brief Add a diagnostic to one editor line.
 * @details The line is one-based to match command-line tool output and the
 *          editor gutter. Graptoς clamps the range to the live buffer.
 * @param context Command context supplied by Graptoς.
 * @param line One-based target line.
 * @param message Diagnostic message shown on hover.
 * @return TRUE when the diagnostic was applied.
 */
gboolean graptos_plugin_context_add_line_diagnostic(GraptosPluginCommandContext *context,
                                                    guint line,
                                                    const char *message);

/**
 * @brief Show command output in a Graptoς dialog.
 * @param context Command context supplied by Graptoς.
 * @param title Dialog title.
 * @param heading Dialog heading.
 * @param body Dialog body.
 */
void graptos_plugin_context_show_output(GraptosPluginCommandContext *context,
                                        const char *title,
                                        const char *heading,
                                        const char *body);

/**
 * @brief Show plugin text in the active editor preview pane.
 * @details Plugins use this for derived reports that belong beside the editor
 *          instead of in a transient dialog. Graptoς owns the preview widgets
 *          and copies both strings before returning.
 * @param context Command context supplied by Graptoς.
 * @param title Preview title.
 * @param body Plain-text preview body.
 */
void graptos_plugin_context_show_preview(GraptosPluginCommandContext *context,
                                         const char *title,
                                         const char *body);

/**
 * @brief Set active editor preview visibility.
 * @details This lets report plugins bring the side preview into view without
 *          exposing the preview widget tree through the plugin ABI.
 * @param context Command context supplied by Graptoς.
 * @param visible TRUE to show preview, FALSE to hide it.
 */
void graptos_plugin_context_set_preview_visible(GraptosPluginCommandContext *context,
                                                gboolean visible);

/**
 * @brief Export plugin supplied LaTeX to a PDF.
 * @details Graptoς writes the source into a build directory beside the active
 *          file, runs a detected LaTeX engine without shell expansion, and
 *          opens the generated PDF.
 * @param context Command context supplied by Graptoς.
 * @param basename Safe base name for generated files.
 * @param latex_source Complete LaTeX source.
 * @return TRUE when a PDF was generated and opened.
 */
gboolean graptos_plugin_context_export_latex_pdf(GraptosPluginCommandContext *context,
                                                 const char *basename,
                                                 const char *latex_source);

/**
 * @brief Set the Graptoς status text.
 * @param context Command context supplied by Graptoς.
 * @param text Status text.
 */
void graptos_plugin_context_set_status(GraptosPluginCommandContext *context,
                                       const char *text);

/**
 * @brief Show plugin supplied completions.
 * @details The provider passes the exact prefix to replace and a list of owned
 *          or borrowed candidate strings. Graptoς copies candidate text before
 *          showing the popup.
 * @param context Command context supplied by Graptoς.
 * @param replace_prefix Text immediately before the cursor to replace.
 * @param source_label Visible source heading.
 * @param candidates GPtrArray of char* candidate text.
 */
void graptos_plugin_context_show_completions(GraptosPluginCommandContext *context,
                                             const char *replace_prefix,
                                             const char *source_label,
                                             GPtrArray *candidates);

/**
 * @brief Return the number of open editor tabs.
 * @param context Command context supplied by Graptoς.
 * @return Open tab count.
 */
guint graptos_plugin_context_tab_count(GraptosPluginCommandContext *context);

/**
 * @brief Return the first open project root.
 * @details The returned string is owned by the caller and must be freed with
 *          g_free().
 * @param context Command context supplied by Graptoς.
 * @return Owned project root path, or NULL when no project is open.
 */
char *graptos_plugin_context_project_root(GraptosPluginCommandContext *context);

/**
 * @brief Open a file in Graptoς.
 * @param context Command context supplied by Graptoς.
 * @param path File path to open.
 * @return TRUE when Graptoς opened the file.
 */
gboolean graptos_plugin_context_open_file(GraptosPluginCommandContext *context,
                                          const char *path);

#endif /* GRAPTOS_PLUGIN_API_H */
