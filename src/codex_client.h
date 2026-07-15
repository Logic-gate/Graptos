/**
 * @file src/codex_client.h
 * @brief Codex process client and event bridge.
 */

#ifndef CLEAF_CODEX_CLIENT_H
#define CLEAF_CODEX_CLIENT_H

#include <gio/gio.h>

/**
 * @brief Codex client type definition.
 */
typedef struct _CodexClient CodexClient;

/**
 * @brief Codex client type definition.
 */
typedef enum {
    CODEX_CLIENT_STOPPED, /**< Codex client stopped. */
    CODEX_CLIENT_CONNECTING, /**< Codex client connecting. */
    CODEX_CLIENT_READY, /**< Codex client ready. */
    CODEX_CLIENT_FAILED /**< Codex client failed. */
} CodexClientState;

/**
 * @brief Void.
 */
typedef void (*CodexClientStatusFunc)(CodexClient *client,
                                      CodexClientState state,
                                      const char *detail,
                                      gpointer user_data);

/**
 * @brief Codex client type definition.
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
 */
typedef void (*CodexClientEventFunc)(CodexClient *client,
                                     CodexClientEvent event,
                                     const char *text,
                                     gpointer user_data);

/**
 * @brief Codex client new.
 */
CodexClient *codex_client_new(CodexClientStatusFunc status_func,
                              gpointer user_data);
/**
 * @brief Codex client start.
 */
void codex_client_start(CodexClient *client, const char *cwd);
/**
 * @brief Codex client set event func.
 */
void codex_client_set_event_func(CodexClient *client,
                                 CodexClientEventFunc event_func);
/**
 * @brief Codex client send prompt.
 */
gboolean codex_client_send_prompt(CodexClient *client, const char *prompt);
/**
 * @brief Codex client interrupt.
 */
gboolean codex_client_interrupt(CodexClient *client);
/**
 * @brief Codex client new thread.
 */
gboolean codex_client_new_thread(CodexClient *client);
/**
 * @brief Codex client resume thread.
 */
gboolean codex_client_resume_thread(CodexClient *client,
                                    const char *thread_id);
/**
 * @brief Codex client resolve approval.
 */
gboolean codex_client_resolve_approval(CodexClient *client,
                                       const char *decision);
/**
 * @brief Codex client stop.
 */
void codex_client_stop(CodexClient *client);
/**
 * @brief Codex client free.
 */
void codex_client_free(CodexClient *client);
/**
 * @brief Codex client get state.
 */
CodexClientState codex_client_get_state(const CodexClient *client);
/**
 * @brief Codex client get thread id.
 */
const char *codex_client_get_thread_id(const CodexClient *client);

#endif /* CLEAF_CODEX_CLIENT_H */
