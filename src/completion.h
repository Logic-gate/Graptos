#ifndef CLEAF_COMPLETION_H
#define CLEAF_COMPLETION_H

#include <gtk/gtk.h>
#include "syntax.h"

#define CLEAF_COMPLETION_MAX_SCAN_CHARS 65536
#define CLEAF_COMPLETION_DEFAULT_MAX_RESULTS 64u

gboolean completion_is_word_char(gunichar ch);
char *completion_prefix_at_cursor(GtkTextBuffer *buffer, GtkTextIter *prefix_start, GtkTextIter *cursor);
void completion_candidates_add(GPtrArray *out, const char *word, const char *prefix, guint max_results);
GPtrArray *completion_candidates_new(GtkTextBuffer *buffer, SyntaxDef *syntax, const char *prefix, guint max_results);

#endif /* CLEAF_COMPLETION_H */
