/**
 * @file src/editor_tab_core.c
 * @brief Graptoς editor tab core module.
 * @details Editor tabs hold the real editing surface. We split the implementation by
 *          lifecycle, input, rendering, preview, and transient UI because each part has
 *          different timing and cleanup pressure.
 */

#include "editor_tab_private.h"

#include "editor/editor_tab_lifecycle.inc.c"
#include "editor/editor_tab_highlight_scheduling.inc.c"
#include "editor/editor_tab_signals_preferences.inc.c"
