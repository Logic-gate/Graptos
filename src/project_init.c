/**
 * @file src/project_init.c
 * @brief Declarative project initialization core.
 * @details This file is deliberately free of GTK widget code. It turns trusted
 *          template data into a resolved plan and applies that plan with normal
 *          GLib filesystem calls, which keeps preview, tests, and UI behavior on
 *          the same path.
 */

#include "project_init.h"

#include <errno.h>
#include <glib/gstdio.h>
#include <string.h>
#include <sys/stat.h>

/**
 * @brief Maximum rendered template file size.
 * @details Project templates should be small source skeletons. Bounding rendered
 *          reads prevents a mistaken template from loading arbitrary large files
 *          into the UI preview or apply path.
 */
#define GRAPTOS_PROJECT_INIT_MAX_FILE_BYTES (4u * 1024u * 1024u)

#include "project_init/project_init_model.inc.c"
#include "project_init/project_init_parser.inc.c"
#include "project_init/project_init_plan.inc.c"
#include "project_init/project_init_apply.inc.c"
