/**
 * @file src/editor_tab_hover.c
 * @brief Graptoς editor tab hover module.
 * @details Hover UI is mostly timing and placement. We keep it away from the core tab
 *          lifecycle because tiny focus changes can make popovers feel stuck, too eager, or
 *          impossible to use.
 */

#include "editor_tab_private.h"

#include "editor/editor_tab_hover_refs.inc.c"
#include "editor/editor_tab_hover_events.inc.c"
#include "editor/editor_tab_context_menu.inc.c"
