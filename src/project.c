/**
 * @file src/project.c
 * @brief Project tree public API and implementation composition unit.
 * @details Projects change underneath the editor. We keep tree rows, filesystem context,
 *          and search helpers away from EditorTab so directory updates do not become
 *          buffer-management problems.
 */

#include "project.h"
#include "app.h"
#include "dialogs.h"
#include "git.h"
#include "ui.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

/**
 * @brief Graptoς project max nodes macro.
 */
#define GRAPTOS_PROJECT_MAX_NODES 5000u
/**
 * @brief Graptoς project max depth macro.
 */
#define GRAPTOS_PROJECT_MAX_DEPTH 12u

/**
 * @brief Project skip flag macro.
 */
#define PROJECT_SKIP_FLAG "__graptos_skip_row__"

/**
 * @brief Project type definition.
 */
typedef struct {
    EditorWindow *win; /**< Win. */
    guint nodes; /**< Nodes. */
} ProjectBuild;

/**
 * @brief Project type definition.
 */
typedef struct {
    char *path; /**< Path. */
    gboolean is_dir; /**< Is dir. */
} ProjectRow;

/**
 * @brief Project type definition.
 */
typedef struct {
    EditorWindow *win; /**< Win. */
    char *path; /**< Path. */
    gboolean is_dir; /**< Is dir. */
} ProjectAction;

/**
 * @brief Project tree rebuild.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 */
static void project_tree_rebuild(EditorWindow *win);
/**
 * @brief Watch a visible project directory for live tree refreshes.
 * @details Project tree code mirrors the filesystem while the user is interacting with expanded rows. The comment marks which part updates the model and which part preserves visible UI state.
 * @param win The win supplied by the caller.
 * @param path The filesystem path supplied by the caller.
 */
static void project_tree_watch_directory(EditorWindow *win, const char *path);

#include "project/project_util.inc.c"
#include "project/project_context.inc.c"
#include "project/project_rows.inc.c"
#include "project/project_tree.inc.c"
