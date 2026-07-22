/**
 * @file src/codex_protocol.c
 * @brief Codex JSON line protocol helpers.
 * @details AI touches the parts of the app where mistakes are expensive: files,
 *          permissions, markdown, and long-running processes. We keep protocol, client,
 *          panel, and review logic separated so approval and cleanup paths are easy to
 *          audit.
 */

#include "codex_protocol.h"

/**
 * @brief Codex protocol message.
 * @details Requests and notifications are the same JSON shape except for the
 *          id. Keeping that construction shared prevents approval, chat, and
 *          lifecycle messages from drifting into subtly different wire formats.
 * @param has_id The has id supplied by the caller.
 * @param id The id supplied by the caller.
 * @param method The method supplied by the caller.
 * @param params The params supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *codex_protocol_message(gboolean has_id,
                                    guint64 id,
                                    const char *method,
                                    JsonNode *params) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    if (method) {
        json_builder_set_member_name(builder, "method");
        json_builder_add_string_value(builder, method);
    }
    if (has_id) {
        json_builder_set_member_name(builder, "id");
        json_builder_add_int_value(builder, (gint64)id);
    }
    if (params) {
        json_builder_set_member_name(builder, "params");
        json_builder_add_value(builder, json_node_copy(params));
    }
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
    return line;
}

/**
 * @brief Codex protocol request.
 * @details Requests carry an id because Codex will answer them later. The
 *          caller owns that id so approval and thread lifecycle replies can be
 *          matched without guessing from method names.
 * @param id The id supplied by the caller.
 * @param method The method supplied by the caller.
 * @param params The params supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *codex_protocol_request(guint64 id,
                             const char *method,
                             JsonNode *params) {
    return codex_protocol_message(TRUE, id, method, params);
}

/**
 * @brief Codex protocol notification.
 * @details Notifications are fire-and-forget messages. We keep them on the same
 *          builder path as requests so params are encoded consistently even
 *          when no response is expected.
 * @param method The method supplied by the caller.
 * @param params The params supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *codex_protocol_notification(const char *method, JsonNode *params) {
    return codex_protocol_message(FALSE, 0u, method, params);
}

/**
 * @brief Codex protocol response.
 * @details Approval decisions go back as JSON-RPC-style responses. This helper
 *          keeps the result envelope boring so the client code can focus on the
 *          permission decision rather than JSON plumbing.
 * @param id The id supplied by the caller.
 * @param result The result supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *codex_protocol_response(guint64 id, JsonNode *result) {
    JsonBuilder *builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "id");
    json_builder_add_int_value(builder, (gint64)id);
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
    return line;
}

/**
 * @brief Codex protocol object params.
 * @details Most protocol calls start with an object-shaped params node. Returning
 *          an owned empty object avoids repeating the JsonNode/Object ownership
 *          dance at every call site.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
JsonNode *codex_protocol_object_params(void) {
    JsonNode *node = json_node_new(JSON_NODE_OBJECT);
    json_node_take_object(node, json_object_new());
    return node;
}

/**
 * @brief Codex protocol parse.
 * @details The subprocess speaks JSON lines. We copy the parsed root out of the
 *          parser so callers can own the message independently of the parser
 *          lifetime and decide how strict they want to be about its contents.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param root_out Output storage filled when the operation can provide a value.
 * @param error Return location for an optional error; left untouched on success unless GTK sets it.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean codex_protocol_parse(const char *line,
                              JsonNode **root_out,
                              GError **error) {
    g_return_val_if_fail(root_out != NULL, FALSE);
    *root_out = NULL;
    if (!line || line[0] == '\0') return FALSE;

    JsonParser *parser = json_parser_new();
    gboolean parsed = json_parser_load_from_data(parser, line, -1, error);
    if (parsed) {
        JsonNode *root = json_parser_get_root(parser);
        if (root && JSON_NODE_HOLDS_OBJECT(root)) *root_out = json_node_copy(root);
        else parsed = FALSE;
    }
    g_object_unref(parser);
    return parsed && *root_out != NULL;
}
