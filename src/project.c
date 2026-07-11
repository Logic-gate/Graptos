#include "project.h"
#include "app.h"
#include "dialogs.h"
#include "git.h"
#include "ui.h"

#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <string.h>

#define CLEAF_PROJECT_MAX_NODES 5000u
#define CLEAF_PROJECT_MAX_DEPTH 12u

#define PROJECT_SKIP_FLAG "__cleaf_skip_row__"

typedef struct {
    EditorWindow *win;
    guint nodes;
} ProjectBuild;

typedef struct {
    char *path;
    gboolean is_dir;
} ProjectRow;

typedef struct {
    EditorWindow *win;
    char *path;
    gboolean is_dir;
} ProjectAction;

static void project_tree_rebuild(EditorWindow *win);

#include "project/project_util.inc.c"
#include "project/project_context.inc.c"
#include "project/project_rows.inc.c"
#include "project/project_tree.inc.c"
