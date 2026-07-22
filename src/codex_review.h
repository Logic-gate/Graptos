/**
 * @file src/codex_review.h
 * @brief Codex diff review tab helpers.
 * @details AI touches the parts of the app where mistakes are expensive: files,
 *          permissions, markdown, and long-running processes. We keep protocol, client,
 *          panel, and review logic separated so approval and cleanup paths are easy to
 *          audit.
 */

#ifndef GRAPTOS_CODEX_REVIEW_H
#define GRAPTOS_CODEX_REVIEW_H

#include <glib.h>

/**
 * @brief Codex review type definition.
 */
typedef struct _EditorWindow EditorWindow;

/**
 * @brief Codex review open diff.
 * @details Opens a locked tab containing a Codex-produced diff.
 * @param win The win supplied by the caller.
 * @param diff The diff supplied by the caller.
 */
void codex_review_open_diff(EditorWindow *win, const char *diff);
/**
 * @brief Codex review revert.
 * @details Applies the reverse patch from the active project root.
 * @param win The win supplied by the caller.
 * @param diff The diff supplied by the caller.
 * @param error Return location for an optional error; left untouched on success unless GTK sets it.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean codex_review_revert(EditorWindow *win,
                             const char *diff,
                             GError **error);

#endif /* GRAPTOS_CODEX_REVIEW_H */
