#include "codex_protocol.h"

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

char *codex_protocol_request(guint64 id,
                             const char *method,
                             JsonNode *params) {
    return codex_protocol_message(TRUE, id, method, params);
}

char *codex_protocol_notification(const char *method, JsonNode *params) {
    return codex_protocol_message(FALSE, 0u, method, params);
}

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

JsonNode *codex_protocol_object_params(void) {
    JsonNode *node = json_node_new(JSON_NODE_OBJECT);
    json_node_take_object(node, json_object_new());
    return node;
}

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
