/**
 * @file src/project_init_ui.c
 * @brief GTK dialog for declarative project initialization.
 * @details The dialog owns user input and preview state, while project_init.c
 *          owns parsing and filesystem work. That separation keeps long-running
 *          generation asynchronous without letting UI widgets leak into the
 *          apply engine.
 */

#include "app_private.h"
#include "project_init.h"

#include <glib/gstdio.h>
#include <stdarg.h>
#include <string.h>

#include "project_init_ui/project_init_ui_debug.inc.c"
#include "project_init_ui/project_init_ui_types.inc.c"
#include "project_init_ui/project_init_ui_lifecycle.inc.c"
#include "project_init_ui/project_init_ui_state.inc.c"
#include "project_init_ui/project_init_ui_apply.inc.c"
#include "project_init_ui/project_init_ui_navigation.inc.c"
#include "project_init_ui/project_init_ui_dialog.inc.c"
