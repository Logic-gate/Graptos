/**
 * @file src/lsp_client.c
 * @brief Optional Language Server Protocol client.
 * @details LSP is useful, but it is not allowed to own the editor. Requests are
 *          asynchronous, answers can be stale, and local YAML/index behavior still needs to
 *          work when a server cannot help.
 */

#include "lsp_client.h"

#include "app.h"
#include "editor_tab_private.h"
#include "project.h"

#include <json-glib/json-glib.h>
#include <stdarg.h>
#include <string.h>

/**
 * @brief Maximum snippet length copied from an LSP target line.
 */
#define GRAPTOS_LSP_SNIPPET_MAX_LINE 512u

/**
 * @brief Maximum delayed retries after an empty LSP completion response.
 */
#define GRAPTOS_LSP_COMPLETION_MAX_RETRIES 4u

/**
 * @brief Delay before retrying an empty LSP completion response.
 */
#define GRAPTOS_LSP_COMPLETION_RETRY_DELAY_MS 90u

/**
 * @brief Active LSP server process state.
 * @details One server belongs to one syntax and one workspace root. Language
 *          servers cache package state, so reusing a Zig server across two
 *          build roots gives fast-looking startup but stale or empty answers.
 */
typedef struct {
    EditorWindow *win; /**< Owning window. */
    SyntaxDef *syntax; /**< Syntax owning this server configuration. */
    char *root_path; /**< Workspace root used for initialize and server reuse. */
    GSubprocess *process; /**< Server process. */
    GDataInputStream *output; /**< Server stdout reader. */
    GDataInputStream *error_output; /**< Server stderr reader in debug mode. */
    GOutputStream *input; /**< Server stdin writer. */
    GCancellable *cancellable; /**< Shared cancellable. */
    GHashTable *open_uris; /**< Set of char* open document URIs. */
    GHashTable *completion_cache; /**< Completion key to GPtrArray<char*> cache. */
    GHashTable *completion_pending; /**< Request id to LspCompletionPending*. */
    GHashTable *references_pending; /**< Request id to LspLocationPending*. */
    GHashTable *definition_pending; /**< Request id to LspLocationPending*. */
    guint64 next_id; /**< Next JSON-RPC request id. */
    guint64 initialize_id; /**< Initialize request id. */
    gboolean initialized; /**< Initialize request has been sent. */
    gboolean initialized_notification_sent; /**< Initialized notification has been sent. */
    gboolean alive; /**< Server process is alive enough to use. */
    gboolean debug; /**< Whether to emit LSP debug logs. */
} LspServer;

/**
 * @brief Pending LSP completion request.
 * @details Completion responses can arrive after the user has typed more text.
 *          We keep the URI, prefix, version, and cache key from request time so
 *          stale answers can be rejected instead of shown in the wrong context.
 */
typedef struct {
    EditorTab *tab; /**< Tab that requested completion. */
    char *uri; /**< Document URI. */
    char *cache_key; /**< Completion cache key. */
    char *prefix; /**< Prefix at request time. */
    guint version; /**< Document version at request time. */
    guint retry_count; /**< Retry count for this completion context. */
    gboolean manual; /**< Whether completion was manually requested. */
} LspCompletionPending;

/**
 * @brief Delayed LSP completion retry state.
 */
typedef struct {
    EditorTab *tab; /**< Tab that owns the retry. */
    char *cache_key; /**< Completion cache key expected at retry time. */
    guint version; /**< Document version expected at retry time. */
    gboolean manual; /**< Whether to retry as a manual completion. */
} LspCompletionRetry;

/**
 * @brief Pending LSP location request.
 */
typedef struct {
    EditorTab *tab; /**< Tab that requested the locations. */
    LspLocationCallback callback; /**< Completion callback. */
    gpointer user_data; /**< Caller data. */
    guint max_results; /**< Maximum locations to return. */
    char *kind; /**< Reference kind label. */
} LspLocationPending;

/**
 * @brief Pending LSP body read state.
 */
typedef struct {
    LspServer *server; /**< Server owning the read. */
    gsize length; /**< JSON body byte length. */
    gsize received; /**< Bytes already read into data. */
    char *data; /**< Exact Content-Length JSON body buffer. */
} LspBodyRead;

struct _LspClient {
    EditorWindow *win; /**< Owning window. */
    GPtrArray *servers; /**< LspServer*. */
    GHashTable *failed_server_keys; /**< Syntax/root/command keys that failed to spawn. */
};

/**
 * @brief Return whether the server completed initialize/initialized.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean lsp_server_ready(LspServer *server) {
    return server && server->initialized_notification_sent;
}

/**
 * @brief Open every matching tab after an LSP server becomes ready.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 */
static void lsp_server_open_matching_tabs(LspServer *server);

/**
 * @brief Return full text for a tab.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *lsp_tab_text(EditorTab *tab);

/**
 * @brief Emit a debug-mode LSP log message.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param server The server supplied by the caller.
 * @param format The format supplied by the caller.
 */
static void lsp_debug(LspServer *server, const char *format, ...);

#include "lsp/lsp_client_diagnostics.inc.c"
#include "lsp/lsp_client_locations.inc.c"
#include "lsp/lsp_client_completion.inc.c"
#include "lsp/lsp_client_protocol.inc.c"
#include "lsp/lsp_client_io.inc.c"

/**
 * @brief Create an LSP client for an editor window.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param win The win supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
LspClient *lsp_client_new(EditorWindow *win) {
    LspClient *client = g_new0(LspClient, 1);
    if (!client) return NULL;
    client->win = win;
    client->servers = g_ptr_array_new_with_free_func(lsp_server_free);
    client->failed_server_keys =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    return client;
}

/**
 * @brief Free an LSP client and all owned language servers.
 * @details LSP traffic is asynchronous and can arrive after the buffer has changed. The comment records which side owns the data at this boundary and why stale replies must be handled carefully.
 * @param client The client instance that owns the protocol state.
 */
void lsp_client_free(LspClient *client) {
    if (!client) return;
    if (client->servers) g_ptr_array_free(client->servers, TRUE);
    if (client->failed_server_keys) g_hash_table_destroy(client->failed_server_keys);
    g_free(client);
}

#include "lsp/lsp_client_documents.inc.c"
#include "lsp/lsp_client_completion_api.inc.c"
#include "lsp/lsp_client_locations_api.inc.c"
