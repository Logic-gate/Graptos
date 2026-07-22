/**
 * @file src/app_core.c
 * @brief Application window implementation composition unit.
 * @details The window is too large to read comfortably as one file, but it still needs one
 *          private compilation boundary. We include the focused window pieces here so state
 *          stays shared without turning every helper into public API.
 */

#include "app_private.h"

#include <stdarg.h>

#include "app/app_window_state.inc.c"
#include "app/app_window_lifecycle.inc.c"
#include "app/app_window_tiles.inc.c"
#include "app/app_window_tabs.inc.c"
#include "app/app_window_events.inc.c"
