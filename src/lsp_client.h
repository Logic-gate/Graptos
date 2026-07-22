/**
 * @file src/lsp_client.h
 * @brief Optional Language Server Protocol client.
 * @details LSP is useful, but it is not allowed to own the editor. Requests are
 *          asynchronous, answers can be stale, and local YAML/index behavior still needs to
 *          work when a server cannot help.
 */

#ifndef GRAPTOS_LSP_CLIENT_H
#define GRAPTOS_LSP_CLIENT_H

#include "editor_tab.h"
#include "index.h"

/**
 * @brief LSP client type definition.
 */
typedef struct _LspClient LspClient;

/**
 * @brief LSP location callback.
 */
typedef void (*LspLocationCallback)(EditorTab *tab,
                                    GPtrArray *locations,
                                    gpointer user_data);

/**
 * @brief LSP client new.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
LspClient *lsp_client_new(EditorWindow *win);

/**
 * @brief LSP client free.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 */
void lsp_client_free(LspClient *client);

/**
 * @brief Notify LSP that a document was opened.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void lsp_client_document_opened(LspClient *client, EditorTab *tab);

/**
 * @brief Notify LSP that a document changed.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void lsp_client_document_changed(LspClient *client, EditorTab *tab);

/**
 * @brief Notify LSP that a document was saved.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void lsp_client_document_saved(LspClient *client, EditorTab *tab);

/**
 * @brief Notify LSP that a document was closed.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 */
void lsp_client_document_closed(LspClient *client, EditorTab *tab);

/**
 * @brief Request LSP completion and return cached candidates.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param prefix The prefix supplied by the caller.
 * @param member_context The member context supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *lsp_client_completion_candidates(LspClient *client,
                                            EditorTab *tab,
                                            const char *prefix,
                                            gboolean member_context,
                                            guint max_results);

/**
 * @brief Request LSP references at a document position.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param character The character supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean lsp_client_request_references(LspClient *client,
                                       EditorTab *tab,
                                       guint line,
                                       guint character,
                                       guint max_results,
                                       LspLocationCallback callback,
                                       gpointer user_data);

/**
 * @brief Request LSP definitions at a document position.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param character The character supplied by the caller.
 * @param callback Callback invoked when the asynchronous step completes.
 * @param user_data The callback context passed through GTK signal data.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean lsp_client_request_definition(LspClient *client,
                                       EditorTab *tab,
                                       guint line,
                                       guint character,
                                       LspLocationCallback callback,
                                       gpointer user_data);

/**
 * @brief Convert a local path to a file URI.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param path The filesystem path supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *lsp_path_to_uri(const char *path);

/**
 * @brief Convert a GTK iterator to an LSP UTF-16-ish position.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param iter The text iterator that anchors the lookup.
 * @param line_out Output storage filled when the operation can provide a value.
 * @param character_out Output storage filled when the operation can provide a value.
 */
void lsp_iter_to_position(GtkTextIter *iter, gint *line_out, gint *character_out);

#endif /* GRAPTOS_LSP_CLIENT_H */
