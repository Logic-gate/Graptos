#ifndef CLEAF_CODEX_REVIEW_H
#define CLEAF_CODEX_REVIEW_H

#include <glib.h>

typedef struct _EditorWindow EditorWindow;

void codex_review_open_diff(EditorWindow *win, const char *diff);
gboolean codex_review_revert(EditorWindow *win,
                             const char *diff,
                             GError **error);

#endif /* CLEAF_CODEX_REVIEW_H */
