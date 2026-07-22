/**
 * @file src/formatter.c
 * @brief Public entry point for syntax-aware formatting.
 * @details This file resolves policy and owns allocation. The actual lexer,
 *          range, layout, and spacing work stays in smaller formatter modules.
 */

#include "formatter_private.h"

/**
 * @brief Normalize formatter preferences.
 * @details Editor values can be zero while a tab is being created. Formatting
 *          uses stable defaults in that case instead of leaking invalid widths
 *          into indentation.
 * @param input Preferences supplied by the editor.
 * @return Safe formatter preferences.
 */
static GraptosFormatterPreferences normalize_preferences(const GraptosFormatterPreferences *input) {
    GraptosFormatterPreferences out = {0};
    out.tab_width = input && input->tab_width > 0u ? input->tab_width : 4u;
    if (out.tab_width > 16u) out.tab_width = 16u;
    out.insert_spaces = input ? input->insert_spaces : TRUE;
    return out;
}

/**
 * @brief Return whether the shared backend currently implements a profile.
 * @details Graptoς accepts several profile names in syntax YAML so the schema
 *          can describe every language family. Formatting only runs for the
 *          implemented brace profiles until the matching generic engines exist.
 * @param formatting Active formatting policy.
 * @return TRUE when formatting can safely run.
 */
static gboolean formatter_profile_supported(const SyntaxFormatting *formatting) {
    if (!formatting) return FALSE;
    if (!formatting->profile || formatting->profile[0] == '\0') return TRUE;
    return g_strcmp0(formatting->profile, "brace_semicolon") == 0 ||
           g_strcmp0(formatting->profile, "brace_optional_semicolon") == 0;
}

/**
 * @brief Format a syntax-aware text fragment.
 * @details Disabled syntaxes return a duplicate of the input. This makes editor
 *          integration simple and keeps unsupported languages non-destructive.
 * @param syntax Active syntax definition.
 * @param preferences Active editor indentation preferences.
 * @param text Text fragment to format.
 * @param error Optional error output.
 * @return Newly allocated formatted text.
 */
char *graptos_format_text(const SyntaxDef *syntax,
                          const GraptosFormatterPreferences *preferences,
                          const char *text,
                          GError **error) {
    (void)error;
    if (!text) return g_strdup("");
    if (!syntax || !syntax->formatting || !syntax->formatting->enabled ||
        !formatter_profile_supported(syntax->formatting)) {
        return g_strdup(text);
    }

    GraptosFormatterContext context = {0};
    context.syntax = syntax;
    context.formatting = syntax->formatting;
    context.preferences = normalize_preferences(preferences);
    context.out = g_string_new(NULL);
    context.line_start = TRUE;
    graptos_formatter_layout(&context, text);

    gsize input_len = strlen(text);
    while (input_len > 0u && context.out->len > 1u &&
           context.out->str[context.out->len - 1u] == '\n' &&
           text[input_len - 1u] != '\n') {
        g_string_truncate(context.out, context.out->len - 1u);
    }
    return g_string_free(context.out, FALSE);
}
