/**
 * @file src/ui_css.c
 * @brief Static and dynamic CSS provider implementation.
 * @details Theme values should be traceable. We translate config into GTK CSS here so
 *          colors and fonts do not get scattered through feature code where they are hard
 *          to reason about later.
 */

#include "ui.h"

#include <glib.h>
#include <glib/gstdio.h>

/*
 * The dynamic provider is replaced whenever config colors change.  Keeping the
 * pointer in this translation unit prevents duplicate providers from stacking
 * on the display and producing stale first-launch colours.
 */
static GtkCssProvider *dynamic_provider;

#include "ui/ui_base_css.inc.c"
#include "ui/ui_editor_css.inc.c"
