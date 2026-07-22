/**
 * @file src/codex_client.c
 * @brief Codex process client and event bridge.
 * @details AI touches the parts of the app where mistakes are expensive: files,
 *          permissions, markdown, and long-running processes. We keep protocol, client,
 *          panel, and review logic separated so approval and cleanup paths are easy to
 *          audit.
 */

#include "codex_client.h"

#include <string.h>

#include "codex_protocol.h"
#include "version.h"

/**
 * @brief Codex client type definition.
 * @details The client owns the Codex subprocess and translates its JSON events
 *          into UI-safe callbacks. The extra request ids and pending flags are
 *          there because prompts, interrupts, and approvals can overlap in the
 *          protocol even though the panel presents one turn at a time.
 */
struct _CodexClient {
    GSubprocess *process; /**< Process. */
    GDataInputStream *output; /**< Output. */
    GOutputStream *input; /**< Input. */
    GCancellable *cancellable; /**< Cancellable. */
    CodexClientStatusFunc status_func; /**< Status func. */
    CodexClientEventFunc event_func; /**< Event func. */
    gpointer user_data; /**< User data. */
    CodexClientState state; /**< State. */
    guint ref_count; /**< Ref count. */
    gboolean disposing; /**< Disposing. */
    guint64 next_request_id; /**< Next request id. */
    guint64 turn_request_id; /**< Turn request id. */
    guint64 interrupt_request_id; /**< Interrupt request id. */
    char *cwd; /**< Cwd. */
    char *thread_id; /**< Thread id. */
    char *turn_id; /**< Turn id. */
    gboolean turn_pending; /**< Turn pending. */
    gboolean interrupt_pending; /**< Interrupt pending. */
    guint64 approval_request_id; /**< Approval request id. */
    JsonNode *approval_request_id_node; /**< Approval request id node. */
    char *approval_method; /**< Approval method. */
};

enum {
    INITIALIZE_REQUEST_ID = 1,
    THREAD_START_REQUEST_ID = 2
};

/**
 * @brief Codex client read next.
 * @details Reads are scheduled one line at a time. The function is forward
 *          declared because startup and callbacks both need to arm the next
 *          async read without exposing the helper outside this file.
 * @param client The client instance that owns the protocol state.
 */
static void codex_client_read_next(CodexClient *client);

/**
 * @brief Codex client ref.
 * @details Async GLib callbacks may run after the caller has tried to stop or
 *          free the client. A small manual refcount keeps the client alive until
 *          the pending read callback has finished using it.
 */
static CodexClient *codex_client_ref(CodexClient *client) {
    if (client) client->ref_count++;
    return client; /**< Client. */
}

/**
 * @brief Codex client unref.
 * @details The client is not a GObject, so we free the process-adjacent state
 *          manually when the last async user releases it. Approval JSON nodes
 *          are owned here because they must survive until the button click.
 * @param client The client instance that owns the protocol state.
 */
static void codex_client_unref(CodexClient *client) {
    if (!client || --client->ref_count != 0u) return;
    g_free(client->cwd);
    g_free(client->thread_id);
    g_free(client->turn_id);
    if (client->approval_request_id_node) json_node_free(client->approval_request_id_node);
    g_free(client->approval_method);
    g_free(client);
}

/**
 * @brief Codex client emit.
 * @details The process layer should not know about GTK widgets. It emits small
 *          typed events and leaves rendering, buttons, and tab reload behavior
 *          to the panel.
 * @param client The client instance that owns the protocol state.
 * @param event The event supplied by the caller.
 * @param text The text fragment supplied by the caller.
 */
static void codex_client_emit(CodexClient *client,
                              CodexClientEvent event,
                              const char *text) {
    if (client && client->event_func) {
        client->event_func(client, event, text, client->user_data);
    }
}

/**
 * @brief Codex client set state.
 * @details State changes are reported immediately because the panel uses them
 *          to enable Send/Stop and to show connection errors. The detail string
 *          stays optional so normal ready/stopped transitions stay quiet.
 * @param client The client instance that owns the protocol state.
 * @param state The state supplied by the caller.
 * @param detail The detail supplied by the caller.
 */
static void codex_client_set_state(CodexClient *client,
                                   CodexClientState state,
                                   const char *detail) {
    if (!client) return;
    client->state = state;
    if (client->status_func) {
        client->status_func(client, state, detail, client->user_data);
    }
}

/**
 * @brief Codex client write.
 * @details All writes take ownership of the generated protocol line. That keeps
 *          call sites simple and makes failed writes clean up the same way as
 *          successful writes.
 * @param client The client instance that owns the protocol state.
 * @param message The message supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean codex_client_write(CodexClient *client, char *message) {
    if (!client || !client->input || !message) {
        g_free(message);
        return FALSE; /**< False. */
    }
    g_autoptr(GError) error = NULL;
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
    }
    return written; /**< Written. */
}

/**
 * @brief Codex client start thread.
 * @details A thread is scoped to the current working directory. We send the cwd
 *          explicitly so Codex can reason about files relative to the same root
 *          Graptoς is showing.
 * @param client The client instance that owns the protocol state.
 */
static void codex_client_start_thread(CodexClient *client) {
    JsonNode *params = codex_protocol_object_params();
    JsonObject *object = json_node_get_object(params);
    json_object_set_string_member(object, "cwd", client->cwd);
    char *request = codex_protocol_request(THREAD_START_REQUEST_ID,
                                           "thread/start", params);
    json_node_free(params);
    (void)codex_client_write(client, request);
}

/**
 * @brief Codex client decline request.
 * @details If Codex asks for an approval while one is already pending, we deny
 *          the extra request. The UI only presents one decision at a time, and
 *          silently queueing permission prompts would be unsafe.
 * @param client The client instance that owns the protocol state.
 * @param id The id supplied by the caller.
 */
static void codex_client_decline_request(CodexClient *client, guint64 id) {
    JsonNode *result = codex_protocol_object_params();
    json_object_set_string_member(json_node_get_object(result),
                                  "decision", "decline");
    char *response = codex_protocol_response(id, result);
    json_node_free(result);
    (void)codex_client_write(client, response);
}

/**
 * @brief Codex client clear approval.
 * @details Only one approval prompt is active at a time. Clearing all related
 *          fields together avoids the old bug where a later allow/deny click
 *          had an id but no method, or a method but no request node.
 * @param client The client instance that owns the protocol state.
 */
static void codex_client_clear_approval(CodexClient *client) {
    if (!client) return;
    client->approval_request_id = 0u;
    g_clear_pointer(&client->approval_request_id_node, json_node_free);
    g_clear_pointer(&client->approval_method, g_free);
}

/**
 * @brief Codex client response for id.
 * @details Some protocol ids are not plain integers. We preserve the original
 *          id node from the request so the response matches exactly what Codex
 *          sent instead of coercing it through `guint64`.
 * @param id_node The id node supplied by the caller.
 * @param result The result supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *codex_client_response_for_id(JsonNode *id_node, JsonNode *result) {
    if (!id_node || !result) return NULL;
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "id");
    json_builder_add_value(builder, json_node_copy(id_node));
    json_builder_set_member_name(builder, "result");
    json_builder_add_value(builder, json_node_copy(result));
    json_builder_end_object(builder);

    JsonGenerator *generator = json_generator_new();
    JsonNode *root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    char *json = json_generator_to_data(generator, NULL);
    char *line = g_strconcat(json, "\n", NULL);

    g_free(json);
    json_node_free(root);
    g_object_unref(generator);
    g_object_unref(builder);
    return line; /**< Line. */
}

/**
 * @brief Codex json object member.
 * @details Json-GLib warns when callers ask for the wrong node type. This helper
 *          checks shape first so optional protocol fields can be probed without
 *          producing critical logs.
 * @param object The object supplied by the caller.
 * @param member The member supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static JsonObject *codex_json_object_member(JsonObject *object,
                                            const char *member) {
    if (!object || !member || !json_object_has_member(object, member)) return NULL;
    JsonNode *node = json_object_get_member(object, member);
    return node && JSON_NODE_HOLDS_OBJECT(node) ? json_node_get_object(node) : NULL;
}

/**
 * @brief Codex json array member.
 * @details Protocol fields have changed shape over time. Returning NULL for a
 *          missing or non-array member lets newer and older Codex servers share
 *          the same parsing path.
 * @param object The object supplied by the caller.
 * @param member The member supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static JsonArray *codex_json_array_member(JsonObject *object,
                                          const char *member) {
    if (!object || !member || !json_object_has_member(object, member)) return NULL;
    JsonNode *node = json_object_get_member(object, member);
    return node && JSON_NODE_HOLDS_ARRAY(node) ? json_node_get_array(node) : NULL;
}

/**
 * @brief Codex client method is approval.
 * @details The app-server has used both item-scoped and older approval method
 *          names. Keeping the compatibility list here keeps the rest of the
 *          client focused on one approval flow.
 * @param method The method supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean codex_client_method_is_approval(const char *method) {
    return method &&
        (g_str_equal(method, "item/commandExecution/requestApproval") ||
         g_str_equal(method, "item/fileChange/requestApproval") ||
         g_str_equal(method, "execCommandApproval") ||
         g_str_equal(method, "applyPatchApproval"));
}

/**
 * @brief Codex client method uses review decision.
 * @details Older approval methods expect different decision words. This check
 *          isolates that translation so the UI can still use stable button
 *          decisions like accept, decline, and cancel.
 * @param method The method supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean codex_client_method_uses_review_decision(const char *method) {
    return method &&
        (g_str_equal(method, "execCommandApproval") ||
         g_str_equal(method, "applyPatchApproval"));
}

/**
 * @brief Codex client response decision.
 * @details Button decisions are Graptoς vocabulary. This function maps them to
 *          the vocabulary required by the active Codex protocol method.
 * @param method The method supplied by the caller.
 * @param decision The decision supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *codex_client_response_decision(const char *method,
                                                  const char *decision) {
    if (!codex_client_method_uses_review_decision(method)) return decision;
    if (g_str_equal(decision, "accept")) return "approved";
    if (g_str_equal(decision, "acceptForSession")) return "approved_for_session";
    if (g_str_equal(decision, "decline")) return "denied";
    if (g_str_equal(decision, "cancel")) return "abort";
    return NULL; /**< Null. */
}

/**
 * @brief Codex client command text.
 * @details Commands may arrive as a string or argv array. We normalize them
 *          into display text only; the real command remains inside the Codex
 *          request and is never reconstructed from this string.
 * @param params The params supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *codex_client_command_text(JsonObject *params) {
    if (!params) return NULL;
    const char *command = json_object_get_string_member_with_default(
        params, "command", NULL);
    if (command) return g_strdup(command);
    JsonArray *command_array = codex_json_array_member(params, "command");
    if (!command_array) return NULL;
    GString *out = g_string_new(NULL);
    if (!out) return NULL;
    for (guint i = 0u; i < json_array_get_length(command_array); i++) {
        const char *part = json_array_get_string_element(command_array, i);
        if (!part) continue;
        if (out->len > 0u) g_string_append_c(out, ' ');
        g_string_append(out, part);
    }
    return g_string_free(out, FALSE);
}

/**
 * @brief Codex client file change count.
 * @details File-change requests have appeared as an object map and as an array.
 *          Counting both shapes keeps the approval dialog useful across Codex
 *          protocol versions.
 * @param params The params supplied by the caller.
 * @return The computed value requested by the caller.
 */
static guint codex_client_file_change_count(JsonObject *params) {
    if (!params) return 0u;
    JsonObject *changes = codex_json_object_member(params, "fileChanges");
    if (changes) return json_object_get_size(changes);
    JsonArray *array = codex_json_array_member(params, "changes");
    return array ? json_array_get_length(array) : 0u;
}

/**
 * @brief Codex client approval detail.
 * @details Approval messages arrive in a few protocol shapes. We normalize them
 *          into one human sentence so the dialog can stay simple and the user
 *          can see whether the request is command execution or file mutation.
 * @param method The method supplied by the caller.
 * @param params The params supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *codex_client_approval_detail(const char *method,
                                          JsonObject *params) {
    const char *reason = params
        ? json_object_get_string_member_with_default(params, "reason", NULL)
        : NULL;
    const char *grant_root = params
        ? json_object_get_string_member_with_default(params, "grantRoot", NULL)
        : NULL;
    if (g_str_equal(method, "item/commandExecution/requestApproval") ||
        g_str_equal(method, "execCommandApproval")) {
        char *command = codex_client_command_text(params);
        char *detail = g_strdup_printf("Allow command%s%s%s",
                                       command ? ": " : reason ? ": " : "",
                                       command ? command : reason ? reason : "",
                                       grant_root ? " (extra permissions requested)" : "");
        g_free(command);
        return detail; /**< Detail. */
    }
    guint count = codex_client_file_change_count(params);
    if (grant_root && grant_root[0] != '\0') {
        return g_strdup_printf(
            "Allow file changes outside the workspace root: %s",
            grant_root);
    }
    if (count > 0u) {
        return g_strdup_printf("Allow proposed file changes: %u file%s",
                               count, count == 1u ? "" : "s");
    }
    return g_strdup_printf("Allow proposed file changes%s%s",
                           reason ? ": " : "", reason ? reason : "");
}

/**
 * @brief Codex client item summary.
 * @details Activity events can be verbose. We collapse command and file-change
 *          items into one line so the panel gives progress without becoming a
 *          raw protocol log.
 * @param item The item supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
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
        JsonArray *changes = codex_json_array_member(item, "changes");
        guint count = changes ? json_array_get_length(changes) : 0u;
        return g_strdup_printf("File changes%s%s: %u file%s",
                               status ? " " : "", status ? status : "",
                               count, count == 1u ? "" : "s");
    }
    return g_strdup_printf("%s%s%s", type, status ? ": " : "",
                           status ? status : "started");
}

/**
 * @brief Codex client handle notification.
 * @details Notifications are the streaming side of a turn. This function keeps
 *          deltas, activity, approvals, diffs, and completion state in one
 *          switchboard so the panel sees ordered UI events.
 * @param client The client instance that owns the protocol state.
 * @param object The object supplied by the caller.
 */
static void codex_client_handle_notification(CodexClient *client,
                                             JsonObject *object) {
    const char *method = json_object_get_string_member_with_default(
        object, "method", NULL);
    JsonObject *params = codex_json_object_member(object, "params");
    if (!method || !params) return;

    if (g_str_equal(method, "turn/diff/updated")) {
        codex_client_emit(client, CODEX_EVENT_DIFF_UPDATED,
                          json_object_get_string_member_with_default(
                              params, "diff", ""));
        return;
    }

    if (g_str_equal(method, "item/started") ||
        g_str_equal(method, "item/completed")) {
        JsonObject *item = codex_json_object_member(params, "item");
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
        codex_client_clear_approval(client);
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
        JsonObject *turn = codex_json_object_member(params, "turn");
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
    JsonObject *turn = codex_json_object_member(params, "turn");
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

/**
 * @brief Codex client handle message.
 * @details A line can be a notification, a response, an approval request, or an
 *          error. The parser keeps those branches explicit because mixing them
 *          was how allow/deny clicks previously got ignored.
 * @param client The client instance that owns the protocol state.
 * @param root The root supplied by the caller.
 */
static void codex_client_handle_message(CodexClient *client, JsonNode *root) {
    JsonObject *object = json_node_get_object(root);
    if (!json_object_has_member(object, "id")) {
        codex_client_handle_notification(client, object);
        return;
    }
    JsonNode *id_node = json_object_get_member(object, "id");
    gint64 id = 0;
    if (id_node && JSON_NODE_HOLDS_VALUE(id_node)) {
        GType id_type = json_node_get_value_type(id_node);
        if (id_type == G_TYPE_INT64 || id_type == G_TYPE_INT ||
            id_type == G_TYPE_UINT || id_type == G_TYPE_UINT64 ||
            id_type == G_TYPE_LONG || id_type == G_TYPE_ULONG) {
            id = json_node_get_int(id_node);
        }
    }
    if (json_object_has_member(object, "method")) {
        const char *method = json_object_get_string_member_with_default(
            object, "method", "");
        JsonObject *params = codex_json_object_member(object, "params");
        gboolean approval = codex_client_method_is_approval(method);
        if (approval && !client->approval_request_id_node) {
            client->approval_request_id = (guint64)id;
            client->approval_request_id_node = id_node ? json_node_copy(id_node) : NULL;
            client->approval_method = g_strdup(method);
            char *detail = codex_client_approval_detail(method, params);
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
            JsonObject *failure = codex_json_object_member(object, "error");
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
        JsonObject *result = codex_json_object_member(object, "result");
        JsonObject *turn = result && json_object_has_member(result, "turn")
            ? codex_json_object_member(result, "turn") : NULL;
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

    JsonObject *result = codex_json_object_member(object, "result");
    JsonObject *thread = result
        ? codex_json_object_member(result, "thread") : NULL;
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

/**
 * @brief Codex client line ready.
 * @details This is the async read boundary. It owns one reference from
 *          `codex_client_read_next`, parses exactly one JSON line, schedules the
 *          next read if the client is still alive, then releases that reference.
 * @param source The source supplied by the caller.
 * @param result The result supplied by the caller.
 * @param user_data The callback context passed through GTK signal data.
 */
static void codex_client_line_ready(GObject *source,
                                    GAsyncResult *result,
                                    gpointer user_data) {
    CodexClient *client = user_data;
    g_autoptr(GError) error = NULL;
    gsize length = 0u;
    g_autofree char *line = g_data_input_stream_read_line_finish(G_DATA_INPUT_STREAM(source),
                                                                 result, &length, &error);
    if (!line) {
        if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            codex_client_set_state(client, CODEX_CLIENT_FAILED,
                                   error ? error->message : "Codex exited");
        }
        codex_client_unref(client);
        return;
    }

    JsonNode *root = NULL;
    if (codex_protocol_parse(line, &root, &error)) {
        codex_client_handle_message(client, root);
        json_node_free(root);
    }
    if (!client->disposing) codex_client_read_next(client);
    codex_client_unref(client);
}

/**
 * @brief Codex client read next.
 * @details There is always at most one pending stdout read. That makes teardown
 *          manageable: cancellation wakes the one callback, which then drops its
 *          temporary client reference.
 * @param client The client instance that owns the protocol state.
 */
static void codex_client_read_next(CodexClient *client) {
    if (!client || !client->output || !client->cancellable) return;
    g_data_input_stream_read_line_async(client->output,
                                        G_PRIORITY_DEFAULT,
                                        client->cancellable,
                                        codex_client_line_ready,
                                        codex_client_ref(client));
}

/**
 * @brief Codex client new.
 * @details Construction only creates local state. Starting the subprocess is a
 *          separate step so the panel can wire callbacks before any events are
 *          emitted.
 */
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

/**
 * @brief Codex client set event func.
 * @details Status and event callbacks are split because connection state and
 *          turn output drive different parts of the panel.
 * @param client The client instance that owns the protocol state.
 * @param event_func The event func supplied by the caller.
 */
void codex_client_set_event_func(CodexClient *client,
                                 CodexClientEventFunc event_func) {
    if (client) client->event_func = event_func;
}

/**
 * @brief Codex client send prompt.
 * @details A new prompt is rejected while a turn is pending or active. The UI
 *          gets one live turn per thread, which keeps Stop and approval state
 *          unambiguous.
 * @param client The client instance that owns the protocol state.
 * @param prompt The prompt supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
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

/**
 * @brief Codex client interrupt.
 * @details Codex cannot interrupt a turn until the turn id is known. If the user
 *          clicks Stop during the start gap, we remember that intent and send
 *          the interrupt as soon as `turn/started` arrives.
 * @param client The client instance that owns the protocol state.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
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

/**
 * @brief Codex client new thread.
 * @details New thread clears the current thread id and asks the app-server for
 *          a fresh thread in the same running process. We do not restart Codex
 *          just to reset conversation context.
 * @param client The client instance that owns the protocol state.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
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

/**
 * @brief Codex client resume thread.
 * @details Resume is allowed only while idle. Switching thread identity during
 *          a turn would make later deltas and approvals ambiguous.
 * @param client The client instance that owns the protocol state.
 * @param thread_id The thread id supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
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

/**
 * @brief Codex client resolve approval.
 * @details The stored request id node is consumed here. We clear approval state
 *          before writing the response so a failed write cannot leave buttons
 *          connected to stale permission data.
 * @param client The client instance that owns the protocol state.
 * @param decision The decision supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean codex_client_resolve_approval(CodexClient *client,
                                       const char *decision) {
    if (!client || !client->approval_request_id_node || !decision) return FALSE;
    if (!g_str_equal(decision, "accept") &&
        !g_str_equal(decision, "acceptForSession") &&
        !g_str_equal(decision, "decline") &&
        !g_str_equal(decision, "cancel")) return FALSE;
    const char *response_decision = codex_client_response_decision(
        client->approval_method, decision);
    if (!response_decision) return FALSE;
    JsonNode *result = codex_protocol_object_params();
    json_object_set_string_member(json_node_get_object(result),
                                  "decision", response_decision);
    char *response = codex_client_response_for_id(client->approval_request_id_node,
                                                  result);
    json_node_free(result);
    codex_client_clear_approval(client);
    gboolean sent = codex_client_write(client, response);
    if (sent) codex_client_emit(client, CODEX_EVENT_APPROVAL_RESOLVED, NULL);
    return sent;
}

/**
 * @brief Codex client start.
 * @details Startup launches `codex app-server --stdio`, begins reading stdout,
 *          then sends initialize. Stderr is silenced here because protocol logs
 *          and UI events should come from JSON, not mixed terminal output.
 * @param client The client instance that owns the protocol state.
 * @param cwd The cwd supplied by the caller.
 */
void codex_client_start(CodexClient *client, const char *cwd) {
    if (!client || client->process) return;
    codex_client_set_state(client, CODEX_CLIENT_CONNECTING, NULL);
    client->cwd = g_strdup(cwd ? cwd : ".");
    client->cancellable = g_cancellable_new();

    g_autoptr(GError) error = NULL;
    g_autoptr(GSubprocessLauncher) launcher = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDIN_PIPE |
        G_SUBPROCESS_FLAGS_STDOUT_PIPE |
        G_SUBPROCESS_FLAGS_STDERR_SILENCE);
    client->process = g_subprocess_launcher_spawn(launcher, &error,
                                                   "codex", "app-server",
                                                   "--stdio", NULL);
    if (!client->process) {
        codex_client_set_state(client, CODEX_CLIENT_FAILED,
                               error ? error->message : "could not start Codex");
        return;
    }
    client->input = g_object_ref(g_subprocess_get_stdin_pipe(client->process));
    client->output = g_data_input_stream_new(
        g_subprocess_get_stdout_pipe(client->process));
    codex_client_read_next(client);

    JsonNode *params = codex_protocol_object_params();
    JsonObject *object = json_node_get_object(params);
    JsonObject *info = json_object_new();
    json_object_set_string_member(info, "name", "graptos");
    json_object_set_string_member(info, "title", "Graptoς");
    json_object_set_string_member(info, "version", APP_VERSION);
    json_object_set_object_member(object, "clientInfo", info);
    char *request = codex_protocol_request(INITIALIZE_REQUEST_ID,
                                           "initialize", params);
    json_node_free(params);
    (void)codex_client_write(client, request);
}

/**
 * @brief Codex client stop.
 * @details Stop is intentionally blunt. The subprocess is an external tool, so
 *          cancellation and force-exit are safer than leaving a half-connected
 *          server with old approvals or turn ids.
 * @param client The client instance that owns the protocol state.
 */
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

/**
 * @brief Codex client free.
 * @details Free disables callbacks before stopping the process. That prevents
 *          late async reads from trying to update a panel that is already being
 *          destroyed.
 * @param client The client instance that owns the protocol state.
 */
void codex_client_free(CodexClient *client) {
    if (!client) return;
    client->disposing = TRUE;
    client->status_func = NULL;
    client->event_func = NULL;
    client->user_data = NULL;
    codex_client_stop(client);
    codex_client_unref(client);
}

/**
 * @brief Codex client get state.
 * @details Callers get STOPPED for NULL so UI refresh code can be defensive
 *          without adding null checks at every label/button update.
 */
CodexClientState codex_client_get_state(const CodexClient *client) {
    return client ? client->state : CODEX_CLIENT_STOPPED;
}

/**
 * @brief Codex client get thread id.
 * @details The returned pointer belongs to the client. The panel only uses it
 *          for display/history context and must not free it.
 * @param client The client instance that owns the protocol state.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
const char *codex_client_get_thread_id(const CodexClient *client) {
    return client ? client->thread_id : NULL;
}
