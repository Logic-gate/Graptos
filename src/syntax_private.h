/**
 * @file src/syntax_private.h
 * @brief Internal syntax parser and highlighter declarations.
 * @details The syntax layer gives Graptoς useful language behavior without requiring LSP.
 *          YAML keeps the language definitions editable, while this code handles loading,
 *          highlighting, diagnostics, and safe fallbacks.
 */

#ifndef GRAPTOS_SYNTAX_PRIVATE_H
#define GRAPTOS_SYNTAX_PRIVATE_H

#include "syntax.h"

/**
 * @brief Syntax def new default.
 * @details Syntax data comes from YAML rules but is applied to live buffers. The comment calls out the narrow contract between static language metadata and mutable editor state.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
SyntaxDef *syntax_def_new_default(void);

#endif /* GRAPTOS_SYNTAX_PRIVATE_H */
