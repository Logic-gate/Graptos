/**
 * @file src/formatter_private.h
 * @brief Internal helpers for the Graptoς formatter.
 * @details Formatting is split by concern so lexical protection, structural
 *          layout, spacing, and range selection can evolve without touching GTK.
 */

#ifndef GRAPTOS_FORMATTER_PRIVATE_H
#define GRAPTOS_FORMATTER_PRIVATE_H

#include "formatter.h"

#include <string.h>

/**
 * @brief Mutable formatting pass state.
 * @details The lexer tracks enough C-like state to protect text that should not
 *          be structurally edited while layout code decides where whitespace
 *          belongs.
 */
typedef struct {
    const SyntaxDef *syntax; /**< Active syntax definition. */
    const SyntaxFormatting *formatting; /**< Active formatting policy. */
    GraptosFormatterPreferences preferences; /**< Resolved editor indentation policy. */
    GString *out; /**< Accumulated formatted text. */
    guint indent_level; /**< Current block indentation level. */
    guint current_column; /**< Current output column for wrap decisions. */
    guint parenthesis_depth; /**< Parenthesis depth outside protected scopes. */
    guint bracket_depth; /**< Bracket depth outside protected scopes. */
    guint brace_depth; /**< Brace depth outside protected scopes. */
    gboolean line_start; /**< TRUE when the next emitted token starts a line. */
    gboolean pending_space; /**< TRUE when collapsed whitespace is pending. */
} GraptosFormatterContext;

/**
 * @brief Append one indentation prefix.
 * @details Indentation is generated from editor preferences so syntax files do
 *          not carry tab-width or insert-spaces policy.
 * @param context Formatter context.
 * @param extra Additional indentation levels.
 */
void graptos_formatter_append_indent(GraptosFormatterContext *context,
                                     guint extra);

/**
 * @brief Format text with structural layout rules.
 * @details The layout pass owns newline placement and indentation while asking
 *          spacing helpers for token-local normalization.
 * @param context Formatter context.
 * @param text Text to format.
 */
void graptos_formatter_layout(GraptosFormatterContext *context,
                              const char *text);

/**
 * @brief Copy one protected lexical region.
 * @details Strings, characters, comments, and preprocessor lines are copied as
 *          user text because editing inside them changes meaning.
 * @param context Formatter context.
 * @param text Source text.
 * @param index Current byte index, advanced past the copied region.
 * @return TRUE when a protected region was copied.
 */
gboolean graptos_formatter_copy_protected(GraptosFormatterContext *context,
                                          const char *text,
                                          gsize *index);

/**
 * @brief Return whether a byte offset sits inside protected text.
 * @details Range discovery and layout both use the same protection rules so
 *          braces in strings and comments do not affect block decisions.
 * @param syntax Active syntax definition.
 * @param text Text being inspected.
 * @param byte_offset Byte offset to classify.
 * @return TRUE when the offset is inside a protected region.
 */
gboolean graptos_formatter_offset_protected(const SyntaxDef *syntax,
                                            const char *text,
                                            gsize byte_offset);

/**
 * @brief Append an operator with configured spacing.
 * @details Operator spacing is intentionally basic in v1; pointer declarations
 *          are handled separately so multiplication and declarators do not
 *          blindly share one rule.
 * @param context Formatter context.
 * @param text Source text.
 * @param index Current byte index.
 * @return TRUE when an operator was consumed.
 */
gboolean graptos_formatter_append_operator(GraptosFormatterContext *context,
                                           const char *text,
                                           gsize *index);

/**
 * @brief Append a pointer declarator star when the context looks declarative.
 * @details This is heuristic by design. It handles common C declarations while
 *          avoiding multiplication and dereference expressions where possible.
 * @param context Formatter context.
 * @param text Source text.
 * @param index Current byte index.
 * @return TRUE when a pointer star was consumed.
 */
gboolean graptos_formatter_append_pointer(GraptosFormatterContext *context,
                                          const char *text,
                                          gsize *index);

#endif
