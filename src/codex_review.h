/**
 * @file src/codex_review.h
 * @brief Codex diff review tab helpers.
 */

#ifndef CLEAF_CODEX_REVIEW_H
#define CLEAF_CODEX_REVIEW_H

#include <glib.h>

/**
 * @brief Codex review type definition.
 */
typedef struct _EditorWindow EditorWindow;

/**
 * @brief Codex review open diff.
 */
void codex_review_open_diff(EditorWindow *win, const char *diff);
/**
 * @brief Codex review revert.
 */
gboolean codex_review_revert(EditorWindow *win,
                             const char *diff,
                             GError **error);

#endif /* CLEAF_CODEX_REVIEW_H */
