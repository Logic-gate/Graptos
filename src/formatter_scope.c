/**
 * @file src/formatter_scope.c
 * @brief Formatter range discovery.
 * @details Editor commands should format the selected text or the nearest
 *          meaningful block, not the whole file. This module finds that block
 *          without depending on GTK.
 */

#include "formatter_private.h"

/**
 * @brief Find a surrounding brace block.
 * @details Braces inside protected lexical regions are ignored. The returned
 *          range includes the opening and closing braces.
 * @param syntax Active syntax definition.
 * @param text Full buffer text.
 * @param cursor_byte_offset Cursor byte offset.
 * @param start_byte_out Output block start.
 * @param end_byte_out Output block end, exclusive.
 * @return TRUE when a block contains the cursor.
 */
gboolean graptos_format_find_surrounding_block(const SyntaxDef *syntax,
                                               const char *text,
                                               gsize cursor_byte_offset,
                                               gsize *start_byte_out,
                                               gsize *end_byte_out) {
    if (!text || !start_byte_out || !end_byte_out) return FALSE;
    GArray *stack = g_array_new(FALSE, FALSE, sizeof(gsize));
    gboolean found = FALSE;
    gsize found_start = 0u;
    gsize found_end = 0u;

    for (gsize i = 0u; text[i]; i++) {
        if (graptos_formatter_offset_protected(syntax, text, i)) continue;
        if (text[i] == '{') {
            g_array_append_val(stack, i);
        } else if (text[i] == '}' && stack->len > 0u) {
            gsize open = g_array_index(stack, gsize, stack->len - 1u);
            g_array_remove_index(stack, stack->len - 1u);
            if (open <= cursor_byte_offset && cursor_byte_offset <= i + 1u) {
                found = TRUE;
                found_start = open;
                found_end = i + 1u;
            }
        }
    }

    g_array_free(stack, TRUE);
    if (!found) return FALSE;
    *start_byte_out = found_start;
    *end_byte_out = found_end;
    return TRUE;
}
