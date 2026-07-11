#include "codex_client.h"

#include <string.h>

#include "codex_protocol.h"

struct _CodexClient {
    GSubprocess *process;
    GDataInputStream *output;
    GOutputStream *input;
    GCancellable *cancellable;
    CodexClientStatusFunc status_func;
    CodexClientEventFunc event_func;
    gpointer user_data;
    CodexClientState state;
    guint ref_count;
    gboolean disposing;
    guint64 next_request_id;
    guint64 turn_request_id;
    guint64 interrupt_request_id;
    char *cwd;
    char *thread_id;
    char *turn_id;
    gboolean turn_pending;
    gboolean interrupt_pending;
    guint64 approval_request_id;
    char *approval_method;
};

enum {
    INITIALIZE_REQUEST_ID = 1,
    THREAD_START_REQUEST_ID = 2
};

static void codex_client_read_next(CodexClient *client);

static CodexClient *codex_client_ref(CodexClient *client) {
    if (client) client->ref_count++;
    return client;
}

static void codex_client_unref(CodexClient *client) {
    if (!client || --client->ref_count != 0u) return;
    g_free(client->cwd);
    g_free(client->thread_id);
    g_free(client->turn_id);
    g_free(client->approval_method);
    g_free(client);
}

static void codex_client_emit(CodexClient *client,
                              CodexClientEvent event,
                              const char *text) {
    if (client && client->event_func) {
        client->event_func(client, event, text, client->user_data);
    }
}

static void codex_client_set_state(CodexClient *client,
                                   CodexClientState state,
                                   const char *detail) {
    if (!client) return;
    client->state = state;
    if (client->status_func) {
        client->status_func(client, state, detail, client->user_data);
    }
}

static gboolean codex_client_write(CodexClient *client, char *message) {
    if (!client || !client->input || !message) {
        g_free(message);
        return FALSE;
    }
    GError *error = NULL;
    gboolean written = g_output_stream_write_all(client->input,
                                                   message,
                                                   strlen(message),
                                                   NULL,
                                                   client->cancellable,
                                                   &error);
    g_free(message);
    if (!written) {
        codex_client_set_state(client, CODEX_CLIENT_FAILED,
                               error ? error->message : "write failed");
        g_clear_error(&error);
    }
    return written;
}

static void codex_client_start_thread(CodexClient *client) {
    JsonNode *params = codex_protocol_object_params();
    JsonObject *object = json_node_get_object(params);
    json_object_set_string_member(object, "cwd", client->cwd);
    char *request = codex_protocol_request(THREAD_START_REQUEST_ID,
                                           "thread/start", params);
    json_node_free(params);
    (void)codex_client_write(client, request);
}

static void codex_client_decline_request(CodexClient *client, guint64 id) {
    JsonNode *result = codex_protocol_object_params();
    json_object_set_string_member(json_node_get_object(result),
                                  "decision", "decline");
    char *response = codex_protocol_response(id, result);
    json_node_free(result);
    (void)codex_client_write(client, response);
}

static char *codex_client_item_summary(JsonObject *item) {
    const char *type = json_object_get_string_member_with_default(
        item, "type", "activity");
    const char *status = json_object_get_string_member_with_default(
        item, "status", NULL);
    if (g_str_equal(type, "commandExecution")) {
        const char *command = json_object_get_string_member_with_default(
            item, "command", "command");
        return g_strdup_printf("Command%s%s: %s",
                               status ? " " : "", status ? status : "", command);
    }
    if (g_str_equal(type, "fileChange")) {
        JsonArray *changes = json_object_get_array_member(item, "changes");
        guint count = changes ? json_array_get_length(changes) : 0u;
        return g_strdup_printf("File changes%s%s: %u file%s",
                               status ? " " : "", status ? status : "",
                               count, count == 1u ? "" : "s");
    }
    return g_strdup_printf("%s%s%s", type, status ? ": " : "",
                           status ? status : "started");
}

static void codex_client_handle_notification(CodexClient *client,
                                             JsonObject *object) {
    const char *method = json_object_get_string_member_with_default(
        object, "method", NULL);
    JsonObject *params = json_object_get_object_member(object, "params");
    if (!method || !params) return;

    if (g_str_equal(method, "turn/diff/updated")) {
        codex_client_emit(client, CODEX_EVENT_DIFF_UPDATED,
                          json_object_get_string_member_with_default(
                              params, "diff", ""));
        return;
    }

    if (g_str_equal(method, "item/started") ||
        g_str_equal(method, "item/completed")) {
        JsonObject *item = json_object_get_object_member(params, "item");
        if (item) {
            const char *type = json_object_get_string_member_with_default(
                item, "type", "");
            if (g_str_equal(type, "commandExecution") ||
                g_str_equal(type, "fileChange")) {
                char *summary = codex_client_item_summary(item);
                codex_client_emit(client, CODEX_EVENT_ACTIVITY, summary);
                g_free(summary);
            }
        }
        return;
    }
    if (g_str_equal(method, "serverRequest/resolved")) {
        client->approval_request_id = 0u;
        g_clear_pointer(&client->approval_method, g_free);
        codex_client_emit(client, CODEX_EVENT_APPROVAL_RESOLVED, NULL);
        return;
    }

    if (g_str_equal(method, "item/agentMessage/delta")) {
        codex_client_emit(client, CODEX_EVENT_AGENT_DELTA,
                          json_object_get_string_member_with_default(
                              params, "delta", ""));
        return;
    }
    if (g_str_equal(method, "turn/started")) {
        JsonObject *turn = json_object_get_object_member(params, "turn");
        const char *id = turn
            ? json_object_get_string_member_with_default(turn, "id", NULL) : NULL;
        g_free(client->turn_id);
        client->turn_id = g_strdup(id);
        client->turn_pending = FALSE;
        codex_client_emit(client, CODEX_EVENT_TURN_STARTED, NULL);
        if (client->interrupt_pending) {
            client->interrupt_pending = FALSE;
            (void)codex_client_interrupt(client);
        }
        return;
    }
    if (!g_str_equal(method, "turn/completed")) return;
    JsonObject *turn = json_object_get_object_member(params, "turn");
    const char *status = turn
        ? json_object_get_string_member_with_default(turn, "status", "failed")
        : "failed";
    if (g_str_equal(status, "completed")) {
        codex_client_emit(client, CODEX_EVENT_TURN_COMPLETED, NULL);
    } else if (g_str_equal(status, "interrupted")) {
        codex_client_emit(client, CODEX_EVENT_TURN_INTERRUPTED, NULL);
    } else {
        codex_client_emit(client, CODEX_EVENT_TURN_FAILED,
                          "The Codex turn failed");
    }
    g_clear_pointer(&client->turn_id, g_free);
    client->turn_pending = FALSE;
    client->interrupt_pending = FALSE;
    client->turn_request_id = 0u;
    client->interrupt_request_id = 0u;
}

static void codex_client_handle_message(CodexClient *client, JsonNode *root) {
    JsonObject *object = json_node_get_object(root);
    if (!json_object_has_member(object, "id")) {
        codex_client_handle_notification(client, object);
        return;
    }
    gint64 id = json_object_get_int_member(object, "id");
    if (json_object_has_member(object, "method")) {
        const char *method = json_object_get_string_member_with_default(
            object, "method", "");
        JsonObject *params = json_object_get_object_member(object, "params");
        gboolean approval =
            g_str_equal(method, "item/commandExecution/requestApproval") ||
            g_str_equal(method, "item/fileChange/requestApproval");
        if (approval && client->approval_request_id == 0u) {
            client->approval_request_id = (guint64)id;
            client->approval_method = g_strdup(method);
            const char *reason = params
                ? json_object_get_string_member_with_default(params, "reason", NULL)
                : NULL;
            const char *command = params
                ? json_object_get_string_member_with_default(params, "command", NULL)
                : NULL;
            char *detail = g_strdup_printf("%s%s%s",
                g_str_has_prefix(method, "item/fileChange")
                    ? "Allow proposed file changes" : "Allow command",
                command ? ": " : reason ? ": " : "?",
                command ? command : reason ? reason : "");
            codex_client_emit(client, CODEX_EVENT_APPROVAL_REQUESTED, detail);
            g_free(detail);
        } else {
            codex_client_decline_request(client, (guint64)id);
        }
        return;
    }
    if (json_object_has_member(object, "error")) {
        if ((guint64)id == client->interrupt_request_id) {
            client->interrupt_request_id = 0u;
            client->interrupt_pending = FALSE;
            JsonObject *failure = json_object_get_object_member(object, "error");
            const char *message = failure
                ? json_object_get_string_member_with_default(
                      failure, "message", "Stop request was rejected")
                : "Stop request was rejected";
            char *detail = g_strdup_printf("Stop request rejected: %s", message);
            codex_client_emit(client, CODEX_EVENT_ACTIVITY, detail);
            g_free(detail);
            return;
        }
        if (id == INITIALIZE_REQUEST_ID || id == THREAD_START_REQUEST_ID) {
            codex_client_set_state(client, CODEX_CLIENT_FAILED,
                                   "Codex app-server rejected a request");
        } else {
            g_clear_pointer(&client->turn_id, g_free);
            client->turn_pending = FALSE;
            client->interrupt_pending = FALSE;
            codex_client_emit(client, CODEX_EVENT_TURN_FAILED,
                              "Codex rejected the turn request");
        }
        return;
    }
    if (id == INITIALIZE_REQUEST_ID) {
        JsonNode *params = codex_protocol_object_params();
        char *notification = codex_protocol_notification("initialized", params);
        json_node_free(params);
        if (codex_client_write(client, notification)) codex_client_start_thread(client);
        return;
    }
    if (id >= 3 && json_object_has_member(object, "result")) {
        JsonObject *result = json_object_get_object_member(object, "result");
        JsonObject *turn = result && json_object_has_member(result, "turn")
            ? json_object_get_object_member(result, "turn") : NULL;
        const char *turn_id = turn
            ? json_object_get_string_member_with_default(turn, "id", NULL) : NULL;
        if (turn_id && !client->turn_id) {
            client->turn_id = g_strdup(turn_id);
            codex_client_emit(client, CODEX_EVENT_TURN_STARTED, NULL);
        }
        if ((guint64)id == client->turn_request_id) {
            client->turn_request_id = 0u;
            client->turn_pending = FALSE;
        } else if ((guint64)id == client->interrupt_request_id) {
            client->interrupt_request_id = 0u;
        }
        return;
    }
    if (id != THREAD_START_REQUEST_ID ||
        !json_object_has_member(object, "result")) return;

    JsonObject *result = json_object_get_object_member(object, "result");
    JsonObject *thread = result
        ? json_object_get_object_member(result, "thread") : NULL;
    const char *thread_id = thread
        ? json_object_get_string_member_with_default(thread, "id", NULL) : NULL;
    if (!thread_id) {
        codex_client_set_state(client, CODEX_CLIENT_FAILED,
                               "Codex returned no thread identifier");
        return;
    }
    g_free(client->thread_id);
    client->thread_id = g_strdup(thread_id);
    codex_client_set_state(client, CODEX_CLIENT_READY, NULL);
}

static void codex_client_line_ready(GObject *source,
                                    GAsyncResult *result,
                                    gpointer user_data) {
    CodexClient *client = user_data;
    GError *error = NULL;
    gsize length = 0u;
    char *line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(source),
                                                       result, &length, &error);
    if (!line) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            codex_client_set_state(client, CODEX_CLIENT_FAILED,
                                   error ? error->message : "Codex exited");
        }
        g_clear_error(&error);
        codex_client_unref(client);
        return;
    }

    JsonNode *root = NULL;
    if (codex_protocol_parse(line, &root, &error)) {
        codex_client_handle_message(client, root);
        json_node_free(root);
    } else {
        g_clear_error(&error);
    }
    g_free(line);
    if (!client->disposing) codex_client_read_next(client);
    codex_client_unref(client);
}

static void codex_client_read_next(CodexClient *client) {
    if (!client || !client->output || !client->cancellable) return;
    g_data_input_stream_read_line_async(client->output,
                                        G_PRIORITY_DEFAULT,
                                        client->cancellable,
                                        codex_client_line_ready,
                                        codex_client_ref(client));
}

CodexClient *codex_client_new(CodexClientStatusFunc status_func,
                              gpointer user_data) {
    CodexClient *client = g_new0(CodexClient, 1);
    client->status_func = status_func;
    client->user_data = user_data;
    client->state = CODEX_CLIENT_STOPPED;
    client->ref_count = 1u;
    client->next_request_id = 3u;
    return client;
}

void codex_client_set_event_func(CodexClient *client,
                                 CodexClientEventFunc event_func) {
    if (client) client->event_func = event_func;
}

gboolean codex_client_send_prompt(CodexClient *client, const char *prompt) {
    if (!client || client->state != CODEX_CLIENT_READY || client->turn_id ||
        client->turn_pending ||
        !client->thread_id || !prompt || prompt[0] == '\0') return FALSE;
    JsonNode *params = codex_protocol_object_params();
    JsonObject *object = json_node_get_object(params);
    json_object_set_string_member(object, "threadId", client->thread_id);
    JsonArray *input = json_array_new();
    JsonObject *text = json_object_new();
    json_object_set_string_member(text, "type", "text");
    json_object_set_string_member(text, "text", prompt);
    json_array_add_object_element(input, text);
    json_object_set_array_member(object, "input", input);
    client->turn_request_id = client->next_request_id++;
    char *request = codex_protocol_request(client->turn_request_id,
                                           "turn/start", params);
    json_node_free(params);
    gboolean sent = codex_client_write(client, request);
    client->turn_pending = sent;
    return sent;
}

gboolean codex_client_interrupt(CodexClient *client) {
    if (!client || !client->thread_id) return FALSE;
    if (!client->turn_id && client->turn_pending) {
        client->interrupt_pending = TRUE;
        return TRUE;
    }
    if (!client->turn_id) return FALSE;
    JsonNode *params = codex_protocol_object_params();
    JsonObject *object = json_node_get_object(params);
    json_object_set_string_member(object, "threadId", client->thread_id);
    json_object_set_string_member(object, "turnId", client->turn_id);
    client->interrupt_request_id = client->next_request_id++;
    char *request = codex_protocol_request(client->interrupt_request_id,
                                           "turn/interrupt", params);
    json_node_free(params);
    return codex_client_write(client, request);
}

gboolean codex_client_new_thread(CodexClient *client) {
    if (!client || client->state != CODEX_CLIENT_READY || client->turn_id ||
        client->turn_pending) {
        return FALSE;
    }
    g_clear_pointer(&client->thread_id, g_free);
    codex_client_set_state(client, CODEX_CLIENT_CONNECTING, NULL);
    codex_client_start_thread(client);
    return TRUE;
}

gboolean codex_client_resume_thread(CodexClient *client,
                                    const char *thread_id) {
    if (!client || client->state != CODEX_CLIENT_READY || client->turn_id ||
        client->turn_pending ||
        !thread_id || thread_id[0] == '\0') return FALSE;
    JsonNode *params = codex_protocol_object_params();
    json_object_set_string_member(json_node_get_object(params),
                                  "threadId", thread_id);
    char *request = codex_protocol_request(THREAD_START_REQUEST_ID,
                                           "thread/resume", params);
    json_node_free(params);
    codex_client_set_state(client, CODEX_CLIENT_CONNECTING, NULL);
    return codex_client_write(client, request);
}

gboolean codex_client_resolve_approval(CodexClient *client,
                                       const char *decision) {
    if (!client || client->approval_request_id == 0u || !decision) return FALSE;
    if (!g_str_equal(decision, "accept") &&
        !g_str_equal(decision, "acceptForSession") &&
        !g_str_equal(decision, "decline") &&
        !g_str_equal(decision, "cancel")) return FALSE;
    JsonNode *result = codex_protocol_object_params();
    json_object_set_string_member(json_node_get_object(result),
                                  "decision", decision);
    char *response = codex_protocol_response(client->approval_request_id, result);
    json_node_free(result);
    client->approval_request_id = 0u;
    g_clear_pointer(&client->approval_method, g_free);
    gboolean sent = codex_client_write(client, response);
    if (sent) codex_client_emit(client, CODEX_EVENT_APPROVAL_RESOLVED, NULL);
    return sent;
}

void codex_client_start(CodexClient *client, const char *cwd) {
    if (!client || client->process) return;
    codex_client_set_state(client, CODEX_CLIENT_CONNECTING, NULL);
    client->cwd = g_strdup(cwd ? cwd : ".");
    client->cancellable = g_cancellable_new();

    GError *error = NULL;
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDIN_PIPE |
        G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    client->process = g_subprocess_launcher_spawn(launcher, &error,
                                                   "codex", "app-server",
                                                   "--stdio", NULL);
    g_object_unref(launcher);
    if (!client->process) {
        codex_client_set_state(client, CODEX_CLIENT_FAILED,
                               error ? error->message : "could not start Codex");
        g_clear_error(&error);
        return;
    }
    client->input = g_object_ref(g_subprocess_get_stdin_pipe(client->process));
    client->output = g_data_input_stream_new(
        g_subprocess_get_stdout_pipe(client->process));
    codex_client_read_next(client);

    JsonNode *params = codex_protocol_object_params();
    JsonObject *object = json_node_get_object(params);
    JsonObject *info = json_object_new();
    json_object_set_string_member(info, "name", "cleaf");
    json_object_set_string_member(info, "title", "Cleaf");
    json_object_set_string_member(info, "version", APP_VERSION);
    json_object_set_object_member(object, "clientInfo", info);
    char *request = codex_protocol_request(INITIALIZE_REQUEST_ID,
                                           "initialize", params);
    json_node_free(params);
    (void)codex_client_write(client, request);
}

void codex_client_stop(CodexClient *client) {
    if (!client) return;
    if (client->cancellable) g_cancellable_cancel(client->cancellable);
    if (client->process) g_subprocess_force_exit(client->process);
    g_clear_object(&client->output);
    g_clear_object(&client->input);
    g_clear_object(&client->process);
    g_clear_object(&client->cancellable);
    client->state = CODEX_CLIENT_STOPPED;
}

void codex_client_free(CodexClient *client) {
    if (!client) return;
    client->disposing = TRUE;
    client->status_func = NULL;
    client->event_func = NULL;
    client->user_data = NULL;
    codex_client_stop(client);
    codex_client_unref(client);
}

CodexClientState codex_client_get_state(const CodexClient *client) {
    return client ? client->state : CODEX_CLIENT_STOPPED;
}

const char *codex_client_get_thread_id(const CodexClient *client) {
    return client ? client->thread_id : NULL;
}
