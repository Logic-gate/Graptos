/**
 * @file src/codex_client.h
 * @brief Codex process client and event bridge.
 * @details AI touches the parts of the app where mistakes are expensive: files,
 *          permissions, markdown, and long-running processes. We keep protocol, client,
 *          panel, and review logic separated so approval and cleanup paths are easy to
 *          audit.
 */

#ifndef GRAPTOS_CODEX_CLIENT_H
#define GRAPTOS_CODEX_CLIENT_H

#include <gio/gio.h>

/**
 * @brief Codex client type definition.
 * @details Opaque outside the client so process lifetime and pending approvals
 *          cannot be mutated by the UI by accident.
 */
typedef struct _CodexClient CodexClient;

/**
 * @brief Codex client type definition.
 * @details The panel only needs these coarse states for buttons and status
 *          text. Turn-level progress is reported through CodexClientEvent.
 */
typedef enum {
    CODEX_CLIENT_STOPPED, /**< Codex client stopped. */
    CODEX_CLIENT_CONNECTING, /**< Codex client connecting. */
    CODEX_CLIENT_READY, /**< Codex client ready. */
    CODEX_CLIENT_FAILED /**< Codex client failed. */
} CodexClientState;

/**
 * @brief Void.
 * @details Status callbacks report connection/process state, not streamed turn
 *          content.
 */
typedef void (*CodexClientStatusFunc)(CodexClient *client,
                                      CodexClientState state,
                                      const char *detail,
                                      gpointer user_data);

/**
 * @brief Codex client type definition.
 * @details These are UI-level events distilled from the app-server protocol.
 *          The panel should not need to know every raw JSON method name.
 */
typedef enum {
    CODEX_EVENT_TURN_STARTED, /**< Codex event turn started. */
    CODEX_EVENT_AGENT_DELTA, /**< Codex event agent delta. */
    CODEX_EVENT_TURN_COMPLETED, /**< Codex event turn completed. */
    CODEX_EVENT_TURN_INTERRUPTED, /**< Codex event turn interrupted. */
    CODEX_EVENT_TURN_FAILED, /**< Codex event turn failed. */
    CODEX_EVENT_ACTIVITY, /**< Codex event activity. */
    CODEX_EVENT_APPROVAL_REQUESTED, /**< Codex event approval requested. */
    CODEX_EVENT_APPROVAL_RESOLVED, /**< Codex event approval resolved. */
    CODEX_EVENT_DIFF_UPDATED /**< Codex event diff updated. */
} CodexClientEvent;

/**
 * @brief Void.
 * @details Event callbacks carry optional text owned by the caller for the
 *          duration of the call.
 */
typedef void (*CodexClientEventFunc)(CodexClient *client,
                                     CodexClientEvent event,
                                     const char *text,
                                     gpointer user_data);

/**
 * @brief Codex client new.
 * @details Creates local client state; call `codex_client_start` to launch the
 *          subprocess.
 */
CodexClient *codex_client_new(CodexClientStatusFunc status_func,
                              gpointer user_data);
/**
 * @brief Codex client start.
 * @details Starts Codex in `cwd` and begins the initialize/thread-start flow.
 * @param client The client instance that owns the protocol state.
 * @param cwd The cwd supplied by the caller.
 */
void codex_client_start(CodexClient *client, const char *cwd);
/**
 * @brief Codex client set event func.
 * @details Installs the panel-facing event callback after construction.
 * @param client The client instance that owns the protocol state.
 * @param event_func The event func supplied by the caller.
 */
void codex_client_set_event_func(CodexClient *client,
                                 CodexClientEventFunc event_func);
/**
 * @brief Codex client send prompt.
 * @details Returns FALSE when the client is not ready or a turn is already live.
 * @param client The client instance that owns the protocol state.
 * @param prompt The prompt supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean codex_client_send_prompt(CodexClient *client, const char *prompt);
/**
 * @brief Codex client interrupt.
 * @details Requests stop for the current turn, or remembers the stop if the
 *          turn id has not arrived yet.
 * @param client The client instance that owns the protocol state.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean codex_client_interrupt(CodexClient *client);
/**
 * @brief Codex client new thread.
 * @details Creates a fresh Codex thread while keeping the subprocess alive.
 * @param client The client instance that owns the protocol state.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean codex_client_new_thread(CodexClient *client);
/**
 * @brief Codex client resume thread.
 * @details Reconnects to an existing thread id while the client is idle.
 * @param client The client instance that owns the protocol state.
 * @param thread_id The thread id supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean codex_client_resume_thread(CodexClient *client,
                                    const char *thread_id);
/**
 * @brief Codex client resolve approval.
 * @details Sends the stored approval response for `decision`.
 * @param client The client instance that owns the protocol state.
 * @param decision The decision supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean codex_client_resolve_approval(CodexClient *client,
                                       const char *decision);
/**
 * @brief Codex client stop.
 * @details Cancels reads and terminates the subprocess.
 * @param client The client instance that owns the protocol state.
 */
void codex_client_stop(CodexClient *client);
/**
 * @brief Codex client free.
 * @details Stops the client and releases owned memory.
 * @param client The client instance that owns the protocol state.
 */
void codex_client_free(CodexClient *client);
/**
 * @brief Codex client get state.
 * @details Safe for NULL callers; NULL is treated as stopped.
 */
CodexClientState codex_client_get_state(const CodexClient *client);
/**
 * @brief Codex client get thread id.
 * @details Returns a borrowed thread id pointer.
 * @param client The client instance that owns the protocol state.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
const char *codex_client_get_thread_id(const CodexClient *client);

#endif /* GRAPTOS_CODEX_CLIENT_H */
