/**
 * @file unit_tests.c
 * @brief Unit tests for protocol serialization and syntax diagnostics helpers.
 *
 * These tests intentionally avoid constructing GTK windows.  They exercise
 * deterministic utility code that can run in a normal headless process.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

#include "../src/codex_protocol.h"
#include "../src/formatter.h"
#include "../src/project_init.h"
#include "../src/syntax.h"

/**
 * @brief Recursively remove a temporary test path.
 * @details Project-init apply tests create real files so atomic cleanup can be
 *          verified. The helper only receives paths created by the test case.
 * @param path File or directory path to remove.
 */
static void test_remove_tree(const char *path) {
    if (!path) return;
    if (g_file_test(path, G_FILE_TEST_IS_DIR) &&
        !g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
        GDir *dir = g_dir_open(path, 0, NULL);
        if (dir) {
            const char *name = NULL;
            while ((name = g_dir_read_name(dir))) {
                g_autofree char *child = g_build_filename(path, name, NULL);
                test_remove_tree(child);
            }
            g_dir_close(dir);
        }
        (void)g_rmdir(path);
    } else {
        (void)g_unlink(path);
    }
}

/**
 * @brief Create a temporary template directory.
 * @details Tests write a small manifest and source file rather than depending
 *          on installed data, which keeps parser failures tied to the test
 *          fixture being exercised.
 * @param manifest The manifest text to write.
 * @param source The template source text to write.
 * @return The temporary template directory.
 */
static char *test_template_dir(const char *manifest, const char *source) {
    g_autoptr(GError) error = NULL;
    char *dir = g_dir_make_tmp("graptos-project-init-XXXXXX", &error);
    g_assert_no_error(error);
    g_assert_nonnull(dir);
    g_autofree char *files = g_build_filename(dir, "files", NULL);
    g_assert_cmpint(g_mkdir_with_parents(files, 0755), ==, 0);
    g_autofree char *manifest_path = g_build_filename(dir, "template.yaml", NULL);
    g_autofree char *source_path = g_build_filename(files, "main.c", NULL);
    g_assert_true(g_file_set_contents(manifest_path, manifest, -1, &error));
    g_assert_no_error(error);
    g_assert_true(g_file_set_contents(source_path, source, -1, &error));
    g_assert_no_error(error);
    return dir;
}

/**
 * @brief Verifies that a Codex protocol request survives serialize/parse.
 *
 * The test builds a small JSON-RPC style request, serializes it to the
 * line-oriented protocol format, parses it back, and checks the id, method,
 * and params payload.
 */
static void test_codex_protocol_request_round_trip(void) {
    JsonNode *params = codex_protocol_object_params();
    JsonObject *params_object = json_node_get_object(params);
    json_object_set_string_member(params_object, "cwd", "/tmp/graptos");

    char *line = codex_protocol_request(42u, "thread/start", params);
    g_assert_nonnull(line);
    g_assert_true(g_str_has_suffix(line, "\n"));

    JsonNode *root = NULL;
    g_autoptr(GError) error = NULL;
    g_assert_true(codex_protocol_parse(line, &root, &error));
    g_assert_no_error(error);
    g_assert_nonnull(root);

    JsonObject *object = json_node_get_object(root);
    g_assert_cmpint(json_object_get_int_member(object, "id"), ==, 42);
    g_assert_cmpstr(json_object_get_string_member(object, "method"), ==, "thread/start");

    JsonObject *parsed_params = json_object_get_object_member(object, "params");
    g_assert_nonnull(parsed_params);
    g_assert_cmpstr(json_object_get_string_member(parsed_params, "cwd"), ==, "/tmp/graptos");

    json_node_free(root);
    g_free(line);
    json_node_free(params);
}

/**
 * @brief Verifies malformed JSON is rejected with a JSON parser error.
 *
 * This covers the negative path for the protocol parser so callers can rely on
 * a false return value and a populated GError when the input line is invalid.
 */
static void test_codex_protocol_rejects_invalid_json(void) {
    JsonNode *root = NULL;
    g_autoptr(GError) error = NULL;

    g_assert_false(codex_protocol_parse("{not json", &root, &error));
    g_assert_null(root);
    g_assert_nonnull(error);
    g_assert_cmpint(error->domain, ==, JSON_PARSER_ERROR);
}

/**
 * @brief Verifies syntax diagnostics describe an empty syntax registry.
 *
 * The diagnostics text should still be useful when no syntax definitions are
 * loaded, so the test checks that the output contains the standard heading and
 * the explicit "none" marker.
 */
static void test_syntax_diagnostics_empty(void) {
    GPtrArray *syntaxes = g_ptr_array_new();
    char *text = syntax_diagnostics(syntaxes);

    g_assert_nonnull(text);
    g_assert_nonnull(strstr(text, "Loaded syntax definitions:"));
    g_assert_nonnull(strstr(text, "none"));

    g_free(text);
    g_ptr_array_unref(syntaxes);
}

/**
 * @brief Verifies syntax diagnostics summarize a populated syntax definition.
 *
 * A synthetic SyntaxDef is assembled in memory and passed to the diagnostics
 * formatter.  The test checks visible summary fields such as name, icon,
 * completion count, extensions, and index status.
 */
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

/**
 * @brief Create a minimal C-like syntax for formatter tests.
 * @details The fixture uses the same structural fields as real syntax files so
 *          tests catch accidental formatter-only structural configuration.
 * @param enabled TRUE to enable formatting.
 * @param brace_style Brace placement policy.
 * @param pointer_alignment Pointer alignment policy.
 * @return Newly allocated syntax definition.
 */
static SyntaxDef *test_formatter_syntax(gboolean enabled,
                                        const char *brace_style,
                                        const char *pointer_alignment) {
    SyntaxDef *syntax = g_new0(SyntaxDef, 1);
    g_assert_nonnull(syntax);
    syntax->name = g_strdup("C test");
    syntax->line_comment = g_strdup("//");
    syntax->indent_openers = g_ptr_array_new_with_free_func(g_free);
    syntax->indent_closers = g_ptr_array_new_with_free_func(g_free);
    syntax->statement_required_enders = g_ptr_array_new_with_free_func(g_free);
    syntax->rules = g_ptr_array_new_with_free_func(syntax_rule_free);
    g_ptr_array_add(syntax->indent_openers, g_strdup("{"));
    g_ptr_array_add(syntax->indent_closers, g_strdup("}"));
    g_ptr_array_add(syntax->statement_required_enders, g_strdup(";"));
    syntax->formatting = g_new0(SyntaxFormatting, 1);
    syntax->formatting->enabled = enabled;
    syntax->formatting->scope = g_strdup("selection_or_block");
    syntax->formatting->profile = g_strdup("brace_semicolon");
    syntax->formatting->block_style = g_strdup("braces");
    syntax->formatting->statement_style = g_strdup("semicolon");
    syntax->formatting->brace_style = g_strdup(brace_style ? brace_style : "attach");
    syntax->formatting->one_statement_per_line = TRUE;
    syntax->formatting->space_before_block_opener = TRUE;
    syntax->formatting->space_after_comma = TRUE;
    syntax->formatting->space_after_control_keyword = TRUE;
    syntax->formatting->space_around_binary_operators = TRUE;
    syntax->formatting->space_around_logical_operators = TRUE;
    syntax->formatting->pointer_alignment = g_strdup(pointer_alignment ? pointer_alignment : "name");
    syntax->formatting->logical_operators = g_ptr_array_new_with_free_func(g_free);
    syntax->formatting->binary_operators = g_ptr_array_new_with_free_func(g_free);
    syntax->formatting->unary_prefix_operators = g_ptr_array_new_with_free_func(g_free);
    syntax->formatting->protected_scopes = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(syntax->formatting->logical_operators, g_strdup("&&"));
    g_ptr_array_add(syntax->formatting->logical_operators, g_strdup("||"));
    g_ptr_array_add(syntax->formatting->unary_prefix_operators, g_strdup("!"));
    g_ptr_array_add(syntax->formatting->protected_scopes, g_strdup("comment"));
    g_ptr_array_add(syntax->formatting->protected_scopes, g_strdup("string"));
    g_ptr_array_add(syntax->formatting->protected_scopes, g_strdup("character"));
    g_ptr_array_add(syntax->formatting->protected_scopes, g_strdup("preprocessor"));
    syntax->formatting->continuation_indent = 4u;
    syntax->formatting->max_column = 100u;
    syntax->formatting->max_blank_lines = 1u;
    syntax->formatting->wrap_after_comma = FALSE;
    syntax->formatting->preserve_blank_lines = TRUE;
    return syntax;
}

/**
 * @brief Format text using spaces and a four-column indent.
 * @details Tests use one helper so expected output differences stay tied to the
 *          formatter behavior rather than fixture boilerplate.
 * @param syntax Syntax fixture.
 * @param input Source text.
 * @return Newly allocated formatted text.
 */
static char *test_format_spaces(SyntaxDef *syntax, const char *input) {
    GraptosFormatterPreferences prefs = {
        .tab_width = 4u,
        .insert_spaces = TRUE,
    };
    return graptos_format_text(syntax, &prefs, input, NULL);
}

/**
 * @brief Verifies disabled formatting returns text unchanged.
 * @details Languages without an enabled formatting section must be non-
 *          destructive when Ctrl+J is pressed.
 */
static void test_formatter_disabled(void) {
    SyntaxDef *syntax = test_formatter_syntax(FALSE, "attach", "name");
    char *formatted = test_format_spaces(syntax, "int main(){return 0;}");
    g_assert_cmpstr(formatted, ==, "int main(){return 0;}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies one-line functions are split and indented.
 * @details This covers the acceptance example and proves braces, statements,
 *          and indentation cooperate in one pass.
 */
static void test_formatter_one_line_function(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    char *formatted = test_format_spaces(
        syntax,
        "GtkWidget *menu_small_label(const char *text) { GtkWidget *label = gtk_label_new(text ? text : \"\"); return label; }");
    g_assert_cmpstr(formatted, ==,
                    "GtkWidget *menu_small_label(const char *text) {\n"
                    "    GtkWidget *label = gtk_label_new(text ? text : \"\");\n"
                    "    return label;\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies for-loop semicolons are not statement split points.
 * @details Parenthesis depth protects `for (...)` headers from the normal
 *          statement terminator rule.
 */
static void test_formatter_for_semicolons(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    char *formatted = test_format_spaces(syntax, "for (guint i = 0; i < count; i++) { process(i); }");
    g_assert_cmpstr(formatted, ==,
                    "for (guint i = 0; i < count; i++) {\n"
                    "    process(i);\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies protected lexical regions stay intact.
 * @details Braces, semicolons, commas, and operators inside strings and comments
 *          are copied as text, not treated as structure.
 */
static void test_formatter_protected_regions(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    char *formatted = test_format_spaces(
        syntax,
        "void f(){const char *s=\"{x;y,a+b}\"; // keep { ; }\nreturn;}");
    g_assert_cmpstr(formatted, ==,
                    "void f() {\n"
                    "    const char *s = \"{x;y,a+b}\";\n"
                    "    // keep { ; }\n"
                    "    return;\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies Allman brace style.
 * @details Brace placement is formatting policy while the brace token still
 *          comes from the existing syntax opener list.
 */
static void test_formatter_allman_braces(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "allman", "name");
    char *formatted = test_format_spaces(syntax, "if (ready) { run(); }");
    g_assert_cmpstr(formatted, ==,
                    "if (ready)\n"
                    "{\n"
                    "    run();\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies pointer alignment modes.
 * @details The star is aligned only when the surrounding line looks like a
 *          declaration, not blindly for every multiplication operator.
 */
static void test_formatter_pointer_alignment(void) {
    SyntaxDef *name = test_formatter_syntax(TRUE, "attach", "name");
    SyntaxDef *type = test_formatter_syntax(TRUE, "attach", "type");
    SyntaxDef *middle = test_formatter_syntax(TRUE, "attach", "middle");
    char *name_out = test_format_spaces(name, "char* text;");
    char *type_out = test_format_spaces(type, "char * text;");
    char *middle_out = test_format_spaces(middle, "char*text;");
    g_assert_cmpstr(name_out, ==, "char *text;");
    g_assert_cmpstr(type_out, ==, "char* text;");
    g_assert_cmpstr(middle_out, ==, "char * text;");
    g_free(name_out);
    g_free(type_out);
    g_free(middle_out);
    syntax_def_free(name);
    syntax_def_free(type);
    syntax_def_free(middle);
}

/**
 * @brief Verifies tab indentation can be emitted.
 * @details Formatter indentation follows editor preferences rather than syntax
 *          data, so insert-spaces false must produce tabs.
 */
static void test_formatter_tabs(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    GraptosFormatterPreferences prefs = {
        .tab_width = 8u,
        .insert_spaces = FALSE,
    };
    char *formatted = graptos_format_text(syntax, &prefs, "void f(){return;}", NULL);
    g_assert_cmpstr(formatted, ==,
                    "void f() {\n"
                    "\treturn;\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies preserved blank lines are capped.
 * @details Users reported the formatter creating visual gaps. The formatter now
 *          has an explicit max_blank_lines policy instead of unlimited blank
 *          preservation.
 */
static void test_formatter_max_blank_lines(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    syntax->formatting->max_blank_lines = 1u;
    char *formatted = test_format_spaces(syntax, "void f(){\n\n\nreturn;\n}");
    g_assert_cmpstr(formatted, ==,
                    "void f() {\n"
                    "\n"
                    "    return;\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies long comma-separated lines wrap at configured columns.
 * @details The first wrapping rule targets function declarations and calls by
 *          breaking after commas once max_column is exceeded.
 */
static void test_formatter_wrap_after_comma(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    syntax->formatting->max_column = 72u;
    syntax->formatting->wrap_after_comma = TRUE;
    char *formatted = test_format_spaces(
        syntax,
        "gboolean graptos_formatter_copy_protected(GraptosFormatterContext *context, const char *text, gsize *index) { return TRUE; }");
    g_assert_cmpstr(formatted, ==,
                    "gboolean graptos_formatter_copy_protected(GraptosFormatterContext *context,\n"
                    "    const char *text, gsize *index) {\n"
                    "    return TRUE;\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies binary logical operators use their own spacing rule.
 * @details Arithmetic spacing and boolean spacing are separate style choices.
 *          Unary `!` stays tight because `! ready` is not Graptoς style.
 */
static void test_formatter_logical_operator_spacing(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    syntax->formatting->space_around_binary_operators = FALSE;
    syntax->formatting->space_around_logical_operators = TRUE;
    char *formatted = test_format_spaces(syntax, "if(a&&b||!c){return;}");
    g_assert_cmpstr(formatted, ==,
                    "if(a && b || !c) {\n"
                    "    return;\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies unsupported generic profiles are non-destructive.
 * @details Graptoς can parse profile names before every generic engine exists.
 *          Unimplemented profiles must return the original text instead of
 *          forcing C-like formatting onto indentation or markup languages.
 */
static void test_formatter_unsupported_profile(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    g_free(syntax->formatting->profile);
    syntax->formatting->profile = g_strdup("indent_colon");
    char *formatted = test_format_spaces(syntax, "if ready:\nprint(value)");
    g_assert_cmpstr(formatted, ==, "if ready:\nprint(value)");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies word logical operators only match token boundaries.
 * @details Zig and PHP can declare operators like `and` and `or`. The generic
 *          matcher must not rewrite ordinary identifiers containing those
 *          byte sequences.
 */
static void test_formatter_word_logical_operator_spacing(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    g_ptr_array_set_size(syntax->formatting->logical_operators, 0u);
    g_ptr_array_add(syntax->formatting->logical_operators, g_strdup("and"));
    g_ptr_array_add(syntax->formatting->logical_operators, g_strdup("or"));
    char *formatted = test_format_spaces(syntax, "if(android and orange){return;}");
    g_assert_cmpstr(formatted, ==,
                    "if(android and orange) {\n"
                    "    return;\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies backtick strings are copied as protected text.
 * @details JavaScript, TypeScript, and Go can carry braces and semicolons inside
 *          backtick strings. The shared formatter must not interpret those
 *          bytes as block structure.
 */
static void test_formatter_backtick_string_protection(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    char *formatted = test_format_spaces(syntax, "void f(){const char *s=`{x;y}`;return;}");
    g_assert_cmpstr(formatted, ==,
                    "void f() {\n"
                    "    const char *s = `{x;y}`;\n"
                    "    return;\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies extra comment tokens can be inferred from comment rules.
 * @details PHP supports `#` comments even though the legacy syntax model only
 *          has one `line_comment` field. Comment-scoped rules let the formatter
 *          protect that line without adding language-specific backend code.
 */
static void test_formatter_comment_rule_hash_protection(void) {
    SyntaxDef *syntax = test_formatter_syntax(TRUE, "attach", "name");
    SyntaxRule *rule = g_new0(SyntaxRule, 1);
    rule->name = g_strdup("comment-line");
    rule->scope = g_strdup("comment");
    rule->pattern = g_strdup("//.*$|#.*$");
    g_ptr_array_add(syntax->rules, rule);
    char *formatted = test_format_spaces(syntax, "void f(){# keep { ; }\nreturn;}");
    g_assert_cmpstr(formatted, ==,
                    "void f() {\n"
                    "    # keep { ; }\n"
                    "    return;\n"
                    "}");
    g_free(formatted);
    syntax_def_free(syntax);
}

/**
 * @brief Verifies supported project-init conditions.
 * @details The condition language is intentionally tiny because template
 *          selection gates filesystem writes. The test covers the accepted
 *          boolean and equality forms.
 */
static void test_project_init_conditions(void) {
    GHashTable *vars = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(vars, g_strdup("include_tests"), g_strdup("true"));
    g_hash_table_insert(vars, g_strdup("build_system"), g_strdup("make"));
    g_autoptr(GError) error = NULL;

    g_assert_true(graptos_project_condition_eval("include_tests", vars, &error));
    g_assert_no_error(error);
    g_assert_false(graptos_project_condition_eval("not include_tests", vars, &error));
    g_assert_true(graptos_project_condition_eval("build_system == \"make\"", vars, &error));
    g_assert_true(graptos_project_condition_eval("build_system != \"cmake\"", vars, &error));
    g_hash_table_unref(vars);
}

/**
 * @brief Verifies placeholder rendering rejects unknown variables.
 * @details Unknown placeholders must fail during planning rather than creating
 *          broken files that look successful in the preview.
 */
static void test_project_init_render_unknown_variable(void) {
    GHashTable *vars = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(vars, g_strdup("project_name"), g_strdup("Demo"));
    g_autoptr(GError) error = NULL;
    char *text = graptos_project_render_string("${missing}", vars, &error);
    g_assert_null(text);
    g_assert_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                   GRAPTOS_PROJECT_INIT_ERROR_UNKNOWN_VARIABLE);
    g_hash_table_unref(vars);
}

/**
 * @brief Verifies project path validation.
 * @details Paths from templates are security-sensitive. The validator should
 *          normalize harmless components and reject traversal or absolute paths.
 */
static void test_project_init_path_validation(void) {
    g_autoptr(GError) error = NULL;
    g_autofree char *normalized = NULL;
    g_assert_true(graptos_project_path_is_safe("src/./main.c", &normalized, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(normalized, ==, "src/main.c");
    g_assert_false(graptos_project_path_is_safe("../file", NULL, &error));
    g_assert_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                   GRAPTOS_PROJECT_INIT_ERROR_INVALID_PATH);
    g_clear_error(&error);
    g_assert_false(graptos_project_path_is_safe("/etc/passwd", NULL, &error));
    g_assert_error(error, GRAPTOS_PROJECT_INIT_ERROR,
                   GRAPTOS_PROJECT_INIT_ERROR_INVALID_PATH);
}

/**
 * @brief Verifies parsing and planning a minimal template.
 * @details The plan should contain resolved paths and rendered content; the
 *          template itself keeps placeholders and conditions.
 */
static void test_project_init_parse_and_plan(void) {
    const char *manifest =
        "version: 1\n"
        "id: c-test\n"
        "name: C Test\n"
        "language: C\n"
        "variables:\n"
        "  executable_name:\n"
        "    label: Executable name\n"
        "    type: text\n"
        "    default: ${project_slug}\n"
        "files:\n"
        "  - path: src/${project_ident}.c\n"
        "    template: files/main.c\n"
        "open:\n"
        "  project: .\n"
        "  file: src/${project_ident}.c\n";
    g_autofree char *dir = test_template_dir(manifest, "hello ${project_name}\n");
    g_autoptr(GError) error = NULL;
    GraptosProjectTemplate *template_def =
        graptos_project_template_load(dir, GRAPTOS_PROJECT_TEMPLATE_USER, &error);
    g_assert_no_error(error);
    g_assert_nonnull(template_def);
    g_autofree char *parent = g_dir_make_tmp("graptos-project-parent-XXXXXX", &error);
    g_assert_no_error(error);
    GraptosProjectPlan *plan =
        graptos_project_plan_build(template_def, "Demo App", parent, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(plan);
    g_assert_cmpuint(plan->files->len, ==, 1);
    GraptosProjectPlannedFile *file = g_ptr_array_index(plan->files, 0);
    g_assert_cmpstr(file->path, ==, "src/demo_app.c");
    gsize size = 0u;
    const char *bytes = g_bytes_get_data(file->content, &size);
    g_assert_cmpmem(bytes, size, "hello Demo App\n", strlen("hello Demo App\n"));
    graptos_project_plan_free(plan);
    graptos_project_template_free(template_def);
    test_remove_tree(parent);
    test_remove_tree(dir);
}

/**
 * @brief Verifies applying a project plan creates files atomically.
 * @details The apply test disables git so it only checks Graptoς-controlled
 *          filesystem behavior and rendered file content.
 */
static void test_project_init_apply_success(void) {
    const char *manifest =
        "version: 1\n"
        "id: c-apply\n"
        "name: C Apply\n"
        "language: C\n"
        "variables:\n"
        "  initialize_git:\n"
        "    label: Initialize Git\n"
        "    type: boolean\n"
        "    default: false\n"
        "directories:\n"
        "  - src\n"
        "files:\n"
        "  - path: src/main.c\n"
        "    template: files/main.c\n"
        "actions:\n"
        "  - type: git-init\n"
        "    when: initialize_git\n";
    g_autofree char *dir = test_template_dir(manifest, "${project_macro}\n");
    g_autoptr(GError) error = NULL;
    GraptosProjectTemplate *template_def =
        graptos_project_template_load(dir, GRAPTOS_PROJECT_TEMPLATE_USER, &error);
    g_assert_no_error(error);
    g_autofree char *parent = g_dir_make_tmp("graptos-project-apply-XXXXXX", &error);
    g_assert_no_error(error);
    GraptosProjectPlan *plan =
        graptos_project_plan_build(template_def, "Apply App", parent, NULL, &error);
    g_assert_no_error(error);
    g_assert_true(graptos_project_plan_apply(plan, &error));
    g_assert_no_error(error);
    g_autofree char *created = g_build_filename(parent, "apply-app", "src", "main.c", NULL);
    g_autofree char *contents = NULL;
    g_assert_true(g_file_get_contents(created, &contents, NULL, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(contents, ==, "APPLY_APP\n");
    graptos_project_plan_free(plan);
    graptos_project_template_free(template_def);
    test_remove_tree(parent);
    test_remove_tree(dir);
}

/**
 * @brief Verifies the commented built-in test template stays valid.
 * @details The documentation template intentionally uses comments on every
 *          meaningful manifest line. Loading and planning it here catches
 *          parser drift, rendered files, raw copied files, and conditional
 *          branches before the template reaches the Init Project dialog.
 */
static void test_project_init_builtin_template_test(void) {
    g_autoptr(GError) error = NULL;
    GraptosProjectTemplate *template_def =
        graptos_project_template_load("data/project-templates/template-test",
                                      GRAPTOS_PROJECT_TEMPLATE_BUILTIN,
                                      &error);
    g_assert_no_error(error);
    g_assert_nonnull(template_def);

    g_autofree char *parent = g_dir_make_tmp("graptos-project-template-test-XXXXXX", &error);
    g_assert_no_error(error);

    GraptosProjectPlan *plan =
        graptos_project_plan_build(template_def, "Docs Demo", parent, NULL, &error);
    g_assert_no_error(error);
    g_assert_nonnull(plan);
    g_assert_cmpuint(plan->directories->len, ==, 2u);
    g_assert_cmpuint(plan->files->len, ==, 4u);

    GraptosProjectPlannedFile *readme = g_ptr_array_index(plan->files, 0u);
    gsize readme_size = 0u;
    const char *readme_bytes = g_bytes_get_data(readme->content, &readme_size);
    g_assert_nonnull(g_strstr_len(readme_bytes, (gssize)readme_size, "Hello Docs Demo"));

    GraptosProjectPlannedFile *literal = g_ptr_array_index(plan->files, 3u);
    gsize literal_size = 0u;
    const char *literal_bytes = g_bytes_get_data(literal->content, &literal_size);
    g_assert_nonnull(g_strstr_len(literal_bytes, (gssize)literal_size, "${project_name}"));

    g_autoptr(GHashTable) overrides =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_replace(overrides, g_strdup("build_style"), g_strdup("advanced"));
    g_hash_table_replace(overrides, g_strdup("include_notes"), g_strdup("false"));
    GraptosProjectPlan *advanced =
        graptos_project_plan_build(template_def, "Docs Demo", parent, overrides, &error);
    g_assert_no_error(error);
    g_assert_nonnull(advanced);
    g_assert_cmpuint(advanced->directories->len, ==, 2u);
    g_assert_cmpuint(advanced->files->len, ==, 4u);

    graptos_project_plan_free(advanced);
    graptos_project_plan_free(plan);
    graptos_project_template_free(template_def);
    test_remove_tree(parent);
}

/**
 * @brief Verifies the language project templates stay loadable.
 * @details The Init Project dialog discovers templates at runtime. This test
 *          plans the new built-ins directly so a bad manifest, invalid path, or
 *          missing source file fails before users see an empty template list.
 */
static void test_project_init_language_templates(void) {
    const char *ids[] = {
        "zig-cli",
        "python-cli",
        "cpp-cli",
        "rust-cli",
    };
    const char *open_files[] = {
        "src/main.zig",
        "src/graptos_demo/cli.py",
        "src/main.cpp",
        "src/main.rs",
    };
    g_autoptr(GError) error = NULL;
    g_autofree char *parent = g_dir_make_tmp("graptos-project-language-templates-XXXXXX", &error);
    g_assert_no_error(error);

    for (guint i = 0u; i < G_N_ELEMENTS(ids); i++) {
        g_autofree char *dir = g_build_filename("data/project-templates", ids[i], NULL);
        GraptosProjectTemplate *template_def =
            graptos_project_template_load(dir, GRAPTOS_PROJECT_TEMPLATE_BUILTIN, &error);
        g_assert_no_error(error);
        g_assert_nonnull(template_def);

        GraptosProjectPlan *plan =
            graptos_project_plan_build(template_def, "Graptos Demo", parent, NULL, &error);
        g_assert_no_error(error);
        g_assert_nonnull(plan);
        g_assert_cmpstr(plan->open_file, ==, open_files[i]);
        g_assert_cmpuint(plan->files->len, >, 0u);

        graptos_project_plan_free(plan);
        graptos_project_template_free(template_def);
    }

    test_remove_tree(parent);
}

/**
 * @brief Registers and runs the unit test suite.
 *
 * @param argc Command-line argument count supplied by the test runner.
 * @param argv Command-line argument vector supplied by the test runner.
 * @return The GLib test runner exit status.
 */
int main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/codex/protocol/request_round_trip", test_codex_protocol_request_round_trip);
    g_test_add_func("/codex/protocol/rejects_invalid_json", test_codex_protocol_rejects_invalid_json);
    g_test_add_func("/syntax/diagnostics/empty", test_syntax_diagnostics_empty);
    g_test_add_func("/syntax/diagnostics/summary", test_syntax_diagnostics_summary);
    g_test_add_func("/formatter/disabled", test_formatter_disabled);
    g_test_add_func("/formatter/one-line-function", test_formatter_one_line_function);
    g_test_add_func("/formatter/for-semicolons", test_formatter_for_semicolons);
    g_test_add_func("/formatter/protected-regions", test_formatter_protected_regions);
    g_test_add_func("/formatter/allman-braces", test_formatter_allman_braces);
    g_test_add_func("/formatter/pointer-alignment", test_formatter_pointer_alignment);
    g_test_add_func("/formatter/tabs", test_formatter_tabs);
    g_test_add_func("/formatter/max-blank-lines", test_formatter_max_blank_lines);
    g_test_add_func("/formatter/wrap-after-comma", test_formatter_wrap_after_comma);
    g_test_add_func("/formatter/logical-operator-spacing", test_formatter_logical_operator_spacing);
    g_test_add_func("/formatter/unsupported-profile", test_formatter_unsupported_profile);
    g_test_add_func("/formatter/word-logical-operator-spacing", test_formatter_word_logical_operator_spacing);
    g_test_add_func("/formatter/backtick-string-protection", test_formatter_backtick_string_protection);
    g_test_add_func("/formatter/comment-rule-hash-protection", test_formatter_comment_rule_hash_protection);
    g_test_add_func("/project-init/conditions", test_project_init_conditions);
    g_test_add_func("/project-init/render/unknown-variable", test_project_init_render_unknown_variable);
    g_test_add_func("/project-init/path/validation", test_project_init_path_validation);
    g_test_add_func("/project-init/parser/plan", test_project_init_parse_and_plan);
    g_test_add_func("/project-init/apply/success", test_project_init_apply_success);
    g_test_add_func("/project-init/templates/template-test", test_project_init_builtin_template_test);
    g_test_add_func("/project-init/templates/languages", test_project_init_language_templates);
    return g_test_run();
}
