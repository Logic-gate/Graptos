#include <glib.h>
#include <string.h>

#include "../src/codex_protocol.h"
#include "../src/syntax.h"

static void test_codex_protocol_request_round_trip(void) {
    JsonNode *params = codex_protocol_object_params();
    JsonObject *params_object = json_node_get_object(params);
    json_object_set_string_member(params_object, "cwd", "/tmp/cleaf");

    char *line = codex_protocol_request(42u, "thread/start", params);
    g_assert_nonnull(line);
    g_assert_true(g_str_has_suffix(line, "\n"));

    JsonNode *root = NULL;
    GError *error = NULL;
    g_assert_true(codex_protocol_parse(line, &root, &error));
    g_assert_no_error(error);
    g_assert_nonnull(root);

    JsonObject *object = json_node_get_object(root);
    g_assert_cmpint(json_object_get_int_member(object, "id"), ==, 42);
    g_assert_cmpstr(json_object_get_string_member(object, "method"), ==, "thread/start");

    JsonObject *parsed_params = json_object_get_object_member(object, "params");
    g_assert_nonnull(parsed_params);
    g_assert_cmpstr(json_object_get_string_member(parsed_params, "cwd"), ==, "/tmp/cleaf");

    json_node_free(root);
    g_free(line);
    json_node_free(params);
}

static void test_codex_protocol_rejects_invalid_json(void) {
    JsonNode *root = NULL;
    GError *error = NULL;

    g_assert_false(codex_protocol_parse("{not json", &root, &error));
    g_assert_null(root);
    g_assert_nonnull(error);
    g_assert_cmpint(error->domain, ==, JSON_PARSER_ERROR);
    g_clear_error(&error);
}

static void test_syntax_diagnostics_empty(void) {
    GPtrArray *syntaxes = g_ptr_array_new();
    char *text = syntax_diagnostics(syntaxes);

    g_assert_nonnull(text);
    g_assert_nonnull(strstr(text, "Loaded syntax definitions:"));
    g_assert_nonnull(strstr(text, "none"));

    g_free(text);
    g_ptr_array_unref(syntaxes);
}

static void test_syntax_diagnostics_summary(void) {
    GPtrArray *syntaxes = g_ptr_array_new();
    GPtrArray *extensions = g_ptr_array_new();
    GPtrArray *rules = g_ptr_array_new();
    GPtrArray *completions = g_ptr_array_new();
    GPtrArray *close_pairs = g_ptr_array_new();
    GPtrArray *line_close_pairs = g_ptr_array_new();

    g_ptr_array_add(extensions, "c");
    g_ptr_array_add(extensions, "h");
    g_ptr_array_add(completions, "include");

    SyntaxDef syntax = {0};
    syntax.name = "C";
    syntax.icon = "c";
    syntax.extensions = extensions;
    syntax.rules = rules;
    syntax.completions = completions;
    syntax.close_pairs = close_pairs;
    syntax.line_close_pairs = line_close_pairs;
    syntax.index_enabled = TRUE;
    syntax.import_style = "c";

    g_ptr_array_add(syntaxes, &syntax);
    char *text = syntax_diagnostics(syntaxes);

    g_assert_nonnull(text);
    g_assert_nonnull(strstr(text, "C [c]"));
    g_assert_nonnull(strstr(text, "1 completions"));
    g_assert_nonnull(strstr(text, "extensions: c, h"));
    g_assert_nonnull(strstr(text, "index: on"));

    g_free(text);
    g_ptr_array_unref(syntaxes);
    g_ptr_array_unref(extensions);
    g_ptr_array_unref(rules);
    g_ptr_array_unref(completions);
    g_ptr_array_unref(close_pairs);
    g_ptr_array_unref(line_close_pairs);
}

int main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/codex/protocol/request_round_trip", test_codex_protocol_request_round_trip);
    g_test_add_func("/codex/protocol/rejects_invalid_json", test_codex_protocol_rejects_invalid_json);
    g_test_add_func("/syntax/diagnostics/empty", test_syntax_diagnostics_empty);
    g_test_add_func("/syntax/diagnostics/summary", test_syntax_diagnostics_summary);
    return g_test_run();
}
