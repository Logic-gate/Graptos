/**
 * @file src/formatter.h
 * @brief Syntax-aware source formatter API.
 * @details The formatter works on plain text and syntax metadata. Editor code
 *          owns GTK buffer ranges, while this layer owns lexical protection and
 *          layout decisions.
 */

#ifndef GRAPTOS_FORMATTER_H
#define GRAPTOS_FORMATTER_H

#include "syntax.h"

/**
 * @brief Editor indentation preferences used by the formatter.
 * @details These values come from the live editor tab, not from syntax files, so
 *          language definitions do not duplicate user preferences.
 */
typedef struct {
    guint tab_width; /**< Visual indentation width. */
    gboolean insert_spaces; /**< TRUE to indent with spaces; FALSE to indent with tabs. */
} GraptosFormatterPreferences;

/**
 * @brief Format a syntax-aware text fragment.
 * @details Returns unchanged text when formatting is unavailable or disabled.
 *          The caller owns the returned string.
 * @param syntax Active syntax definition.
 * @param preferences Active editor indentation preferences.
 * @param text Text fragment to format.
 * @param error Optional error output.
 * @return Newly allocated formatted text.
 */
char *graptos_format_text(const SyntaxDef *syntax,
                          const GraptosFormatterPreferences *preferences,
                          const char *text,
                          GError **error);

/**
 * @brief Find the surrounding brace block for a byte offset.
 * @details The search ignores braces inside strings, characters, comments, and
 *          preprocessor lines so the editor can format the logical block near
 *          the cursor instead of the whole file.
 * @param syntax Active syntax definition.
 * @param text Full buffer text.
 * @param cursor_byte_offset Cursor byte offset in text.
 * @param start_byte_out Output start byte offset.
 * @param end_byte_out Output end byte offset, exclusive.
 * @return TRUE when a containing block was found.
 */
gboolean graptos_format_find_surrounding_block(const SyntaxDef *syntax,
                                               const char *text,
                                               gsize cursor_byte_offset,
                                               gsize *start_byte_out,
                                               gsize *end_byte_out);

#endif
