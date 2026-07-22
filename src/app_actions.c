/**
 * @file src/app_actions.c
 * @brief Application action implementation composition unit.
 * @details The app window is the meeting point for most features. We keep that coordination
 *          here so editor tabs, project state, dialogs, terminals, Git, and AI do not all
 *          learn about each other directly.
 */

#include "app_private.h"

#include <pango/pangocairo.h>
#include <string.h>

#include "app/app_file_search_actions.inc.c"
#include "app/app_preference_actions.inc.c"
#include "app/app_about_view_actions.inc.c"
