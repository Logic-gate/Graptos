/**
 * @file src/completion.h
 * @brief Text completion candidate extraction helpers.
 * @details These helpers are deliberately small string tools. They give local completion a
 *          predictable base without tying it to any one editor tab or language-server
 *          implementation.
 */

#ifndef GRAPTOS_COMPLETION_H
#define GRAPTOS_COMPLETION_H

#include <gtk/gtk.h>
#include "syntax.h"

/**
 * @brief Graptoς completion max scan chars macro.
 */
#define GRAPTOS_COMPLETION_MAX_SCAN_CHARS 65536
/**
 * @brief Graptoς completion default max results macro.
 */
#define GRAPTOS_COMPLETION_DEFAULT_MAX_RESULTS 64u

/**
 * @brief Completion is word char.
 * @details Completion mixes Graptoς YAML, project index data, imports, and LSP results. The comment marks how the list is built without letting one source hide another.
 * @param ch The ch supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean completion_is_word_char(gunichar ch);
/**
 * @brief Completion prefix at cursor.
 * @details Completion mixes Graptoς YAML, project index data, imports, and LSP results. The comment marks how the list is built without letting one source hide another.
 * @param buffer The text buffer used for the operation.
 * @param prefix_start The prefix start supplied by the caller.
 * @param cursor The cursor supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *completion_prefix_at_cursor(GtkTextBuffer *buffer, GtkTextIter *prefix_start, GtkTextIter *cursor);
/**
 * @brief Completion candidates add.
 * @details Completion mixes Graptoς YAML, project index data, imports, and LSP results. The comment marks how the list is built without letting one source hide another.
 * @param out Output storage filled when the lookup succeeds.
 * @param word The symbol text being matched.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 */
void completion_candidates_add(GPtrArray *out, const char *word, const char *prefix, guint max_results);
/**
 * @brief Completion candidates new.
 * @details Completion mixes Graptoς YAML, project index data, imports, and LSP results. The comment marks how the list is built without letting one source hide another.
 * @param buffer The text buffer used for the operation.
 * @param syntax The syntax definition used by the editor path.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *completion_candidates_new(GtkTextBuffer *buffer, SyntaxDef *syntax, const char *prefix, guint max_results);

#endif /* GRAPTOS_COMPLETION_H */
