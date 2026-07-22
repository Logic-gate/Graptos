/**
 * @file src/syntax_diagnostics.c
 * @brief Syntax registry diagnostic text generation.
 * @details The syntax layer gives Graptoς useful language behavior without requiring LSP.
 *          YAML keeps the language definitions editable, while this code handles loading,
 *          highlighting, diagnostics, and safe fallbacks.
 */

#include "syntax.h"

/**
 * @brief Syntax diagnostics.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @param syntaxes The syntaxes supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *syntax_diagnostics(GPtrArray *syntaxes) {
    GString *out = g_string_new("Loaded syntax definitions:\n");
    if (!out) return g_strdup("No diagnostic data.");
    if (!syntaxes || syntaxes->len == 0u) {
        g_string_append(out, "  none\n");
        return g_string_free(out, FALSE);
    }
    for (guint i = 0; i < syntaxes->len; i++) {
        SyntaxDef *syntax = g_ptr_array_index(syntaxes, i);
        if (!syntax) continue;
        g_string_append_printf(out,
                               "  - %s [%s] (%u rules, %u completions, %u pairs, %u line-pairs, index: %s, imports: %s)\n",
                               syntax->name ? syntax->name : "Unnamed",
                               syntax->icon ? syntax->icon : "mime",
                               syntax->rules ? syntax->rules->len : 0u,
                               syntax->completions ? syntax->completions->len : 0u,
                               syntax->close_pairs ? syntax->close_pairs->len : 0u,
                               syntax->line_close_pairs ? syntax->line_close_pairs->len : 0u,
                               syntax->index_enabled ? "on" : "off",
                               syntax->import_style ? syntax->import_style : "generic");
        if (syntax->extensions && syntax->extensions->len > 0u) {
            g_string_append(out, "      extensions: ");
            for (guint e = 0u; e < syntax->extensions->len; e++) {
                g_string_append_printf(out, "%s%s", e ? ", " : "", (char *)g_ptr_array_index(syntax->extensions, e));
            }
            g_string_append_c(out, '\n');
        }
        if (syntax->filenames && syntax->filenames->len > 0u) {
            g_string_append(out, "      filenames: ");
            for (guint f = 0u; f < syntax->filenames->len; f++) {
                g_string_append_printf(out, "%s%s", f ? ", " : "", (char *)g_ptr_array_index(syntax->filenames, f));
            }
            g_string_append_c(out, '\n');
        }
    }
    return g_string_free(out, FALSE);
}
