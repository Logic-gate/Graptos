/**
 * @file src/completion.h
 * @brief Text completion candidate extraction helpers.
 */

#ifndef CLEAF_COMPLETION_H
#define CLEAF_COMPLETION_H

#include <gtk/gtk.h>
#include "syntax.h"

/**
 * @brief Cleaf completion max scan chars macro.
 */
#define CLEAF_COMPLETION_MAX_SCAN_CHARS 65536
/**
 * @brief Cleaf completion default max results macro.
 */
#define CLEAF_COMPLETION_DEFAULT_MAX_RESULTS 64u

/**
 * @brief Completion is word char.
 */
gboolean completion_is_word_char(gunichar ch);
/**
 * @brief Completion prefix at cursor.
 */
char *completion_prefix_at_cursor(GtkTextBuffer *buffer, GtkTextIter *prefix_start, GtkTextIter *cursor);
/**
 * @brief Completion candidates add.
 */
void completion_candidates_add(GPtrArray *out, const char *word, const char *prefix, guint max_results);
/**
 * @brief Completion candidates new.
 */
GPtrArray *completion_candidates_new(GtkTextBuffer *buffer, SyntaxDef *syntax, const char *prefix, guint max_results);

#endif /* CLEAF_COMPLETION_H */
