#ifndef CLEAF_CODEX_CLIENT_H
#define CLEAF_CODEX_CLIENT_H

#include <gio/gio.h>

typedef struct _CodexClient CodexClient;

typedef enum {
    CODEX_CLIENT_STOPPED,
    CODEX_CLIENT_CONNECTING,
    CODEX_CLIENT_READY,
    CODEX_CLIENT_FAILED
} CodexClientState;

typedef void (*CodexClientStatusFunc)(CodexClient *client,
                                      CodexClientState state,
                                      const char *detail,
                                      gpointer user_data);

typedef enum {
    CODEX_EVENT_TURN_STARTED,
    CODEX_EVENT_AGENT_DELTA,
    CODEX_EVENT_TURN_COMPLETED,
    CODEX_EVENT_TURN_INTERRUPTED,
    CODEX_EVENT_TURN_FAILED,
    CODEX_EVENT_ACTIVITY,
    CODEX_EVENT_APPROVAL_REQUESTED,
    CODEX_EVENT_APPROVAL_RESOLVED,
    CODEX_EVENT_DIFF_UPDATED
} CodexClientEvent;

typedef void (*CodexClientEventFunc)(CodexClient *client,
                                     CodexClientEvent event,
                                     const char *text,
                                     gpointer user_data);

CodexClient *codex_client_new(CodexClientStatusFunc status_func,
                              gpointer user_data);
void codex_client_start(CodexClient *client, const char *cwd);
void codex_client_set_event_func(CodexClient *client,
                                 CodexClientEventFunc event_func);
gboolean codex_client_send_prompt(CodexClient *client, const char *prompt);
gboolean codex_client_interrupt(CodexClient *client);
gboolean codex_client_new_thread(CodexClient *client);
gboolean codex_client_resume_thread(CodexClient *client,
                                    const char *thread_id);
gboolean codex_client_resolve_approval(CodexClient *client,
                                       const char *decision);
void codex_client_stop(CodexClient *client);
void codex_client_free(CodexClient *client);
CodexClientState codex_client_get_state(const CodexClient *client);
const char *codex_client_get_thread_id(const CodexClient *client);

#endif /* CLEAF_CODEX_CLIENT_H */
