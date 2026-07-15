/**
 * @file src/ui_css.c
 * @brief Static and dynamic CSS provider implementation.
 */

#include "ui.h"

#include <glib.h>

/*
 * The dynamic provider is replaced whenever config colors change.  Keeping the
 * pointer in this translation unit prevents duplicate providers from stacking
 * on the display and producing stale first-launch colours.
 */
static GtkCssProvider *dynamic_provider;

#include "ui/ui_base_css.inc.c"
#include "ui/ui_editor_css.inc.c"
