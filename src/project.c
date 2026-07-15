/**
 * @file src/project.c
 * @brief Project tree public API and implementation composition unit.
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
 * @brief Cleaf project max nodes macro.
 */
#define CLEAF_PROJECT_MAX_NODES 5000u
/**
 * @brief Cleaf project max depth macro.
 */
#define CLEAF_PROJECT_MAX_DEPTH 12u

/**
 * @brief Project skip flag macro.
 */
#define PROJECT_SKIP_FLAG "__cleaf_skip_row__"

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
 */
static void project_tree_rebuild(EditorWindow *win);

#include "project/project_util.inc.c"
#include "project/project_context.inc.c"
#include "project/project_rows.inc.c"
#include "project/project_tree.inc.c"
