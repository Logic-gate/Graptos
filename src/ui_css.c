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
static GtkCssProvider *custom_provider;

#include "ui/ui_base_css.inc.c"
#include "ui/ui_editor_css.inc.c"

/**
 * @brief Return editable theme CSS template content.
 * @details The template intentionally uses the public Graptoς CSS classes
 *          instead of generated internals, so users can keep the file across
 *          rebuilds while the theme editor gradually exposes more fields.
 * @return Static CSS template text.
 */
static const char *graptos_theme_css_template(void) {
    return
        "/* GRAPTOS THEME BEGIN */\n"
        "@define-color graptos_editor_bg #181a1f;\n"
        "@define-color graptos_editor_fg #d4d4d4;\n"
        "@define-color graptos_editor_gutter_bg #181a1f;\n"
        "@define-color graptos_editor_gutter_fg #8b949e;\n"
        "@define-color graptos_note_indicator #f9c74f;\n"
        "@define-color graptos_note_indicator_active #ffd166;\n"
        "@define-color graptos_note_popover_bg #181a1f;\n"
        "@define-color graptos_note_popover_fg #d4d4d4;\n"
        "@define-color graptos_note_popover_border #3a4050;\n"
        "@define-color graptos_sidebar_bg #111318;\n"
        "@define-color graptos_tabbar_bg #111318;\n"
        "@define-color graptos_tabbar_border #00000000;\n"
        "@define-color graptos_tab_active_bg #20232b;\n"
        "@define-color graptos_tab_active_fg #ffffff;\n"
        "@define-color graptos_tab_active_border #89b4fa;\n"
        "@define-color graptos_tab_tiled_indicator #89b4fa;\n"
        "@define-color graptos_topbar_bg #111318;\n"
        "@define-color graptos_bottombar_bg #111318;\n"
        "@define-color graptos_popover_bg #1b1f24;\n"
        "@define-color graptos_dialog_bg #1b1f24;\n"
        "@define-color graptos_accent #89b4fa;\n"
        "@define-color graptos_warning_bg #5f4b24;\n\n"
        ".graptos-root {\n"
        "  background: @graptos_editor_bg;\n"
        "  color: @graptos_editor_fg;\n"
        "}\n\n"
        ".graptos-editor, .graptos-editor text,\n"
        "textview.graptos-editor, textview.graptos-editor text,\n"
        "sourceview.graptos-editor, sourceview.graptos-editor text {\n"
        "  background: @graptos_editor_bg;\n"
        "  color: @graptos_editor_fg;\n"
        "}\n\n"
        ".graptos-gutter {\n"
        "  background: @graptos_editor_gutter_bg;\n"
        "  color: @graptos_editor_gutter_fg;\n"
        "}\n\n"
        ".graptos-project-pane, .graptos-project-tree, .graptos-project-tree row {\n"
        "  background: @graptos_sidebar_bg;\n"
        "}\n\n"
        ".graptos-root notebook > header,\n"
        ".graptos-root notebook > header.top > tabs > tab {\n"
        "  background: @graptos_tabbar_bg;\n"
        "  border-left-color: @graptos_tabbar_border;\n"
        "  border-right-color: @graptos_tabbar_border;\n"
        "  border-top-color: @graptos_tabbar_border;\n"
        "  border-bottom-color: @graptos_tabbar_border;\n"
        "}\n\n"
        ".graptos-root notebook > header.top {\n"
        "  border-color: @graptos_tabbar_border;\n"
        "  border-bottom-color: @graptos_tab_active_border;\n"
        "}\n"
        ".graptos-root notebook > header.top > tabs > tab:checked {\n"
        "  background: @graptos_tab_active_bg;\n"
        "  color: @graptos_tab_active_fg;\n"
        "  border-left-color: @graptos_tabbar_border;\n"
        "  border-right-color: @graptos_tabbar_border;\n"
        "  border-top-color: @graptos_tabbar_border;\n"
        "  border-bottom-color: @graptos_tab_active_border;\n"
        "}\n\n"
        ".graptos-top { background: @graptos_topbar_bg; }\n"
        ".graptos-bottom, .graptos-search-panel, .graptos-tool-panel {\n"
        "  background: @graptos_bottombar_bg;\n"
        "}\n\n"
        "popover.graptos-context-popover, popover.graptos-tools-popover,\n"
        "popover.graptos-completion-popover, popover.graptos-hover-popover {\n"
        "  background: @graptos_popover_bg;\n"
        "}\n\n"
        "popover.graptos-note-popover, popover.graptos-note-popover textview,\n"
        "popover.graptos-note-popover textview text,\n"
        "popover.graptos-note-popover .graptos-note-status {\n"
        "  background: @graptos_note_popover_bg;\n"
        "  color: @graptos_note_popover_fg;\n"
        "  border-color: @graptos_note_popover_border;\n"
        "}\n\n"
        ".graptos-dialog-root { background: @graptos_dialog_bg; }\n"
        ".graptos-tab-tiled { box-shadow: inset 0 -2px @graptos_tab_tiled_indicator; }\n"
        ".graptos-completion-list row:selected { background: @graptos_accent; }\n"
        ".graptos-diagnostic-warning { background: @graptos_warning_bg; }\n"
        "/* GRAPTOS THEME END */\n";
}

/**
 * @brief Write an editable theme CSS template.
 * @details Existing files are preserved unless overwrite is requested. The
 *          parent directory is created on demand so first launch can expose the
 *          post-compile CSS entrypoint without requiring manual setup.
 * @param path The destination path.
 * @param overwrite TRUE to replace an existing file.
 * @return TRUE when the template exists after the call; otherwise FALSE.
 */
gboolean graptos_write_theme_css_template(const char *path,
                                          gboolean overwrite) {
    if (!path || path[0] == '\0') return FALSE;
    if (!overwrite && g_file_test(path, G_FILE_TEST_IS_REGULAR)) return TRUE;

    g_autofree char *dir = g_path_get_dirname(path);
    if (!dir || g_mkdir_with_parents(dir, 0700) != 0) return FALSE;

    return g_file_set_contents(path,
                               graptos_theme_css_template(),
                               -1,
                               NULL);
}

/**
 * @brief Remove the active theme CSS provider.
 * @details Reloading themes replaces providers instead of stacking them so
 *          deleted or changed CSS files cannot keep stale rules alive.
 * @param display The GTK display that owns CSS providers.
 */
static void graptos_remove_theme_css_provider(GdkDisplay *display) {
    if (!display || !custom_provider) return;
    gtk_style_context_remove_provider_for_display(
        display, GTK_STYLE_PROVIDER(custom_provider));
    g_clear_object(&custom_provider);
}

/**
 * @brief Apply theme CSS after generated fallback CSS.
 * @details CSS file creation belongs to config migration and the Theme dialog.
 *          The provider only loads an existing active theme file so missing
 *          paths cannot create stale or empty override files.
 * @param path The CSS file path supplied by config.
 * @param enabled TRUE when theme CSS loading should be active.
 */
void graptos_apply_theme_css(const char *path, gboolean enabled) {
    GdkDisplay *display = gdk_display_get_default();
    if (!display) return;

    graptos_remove_theme_css_provider(display);
    if (!enabled || !path || path[0] == '\0') return;
    if (!g_file_test(path, G_FILE_TEST_IS_REGULAR)) return;

    g_autofree char *css = NULL;
    gsize length = 0u;
    g_autoptr(GError) error = NULL;
    if (!g_file_get_contents(path, &css, &length, &error) || !css) {
        g_warning("Could not load Graptoς theme CSS %s: %s",
                  path,
                  error ? error->message : "unknown error");
        return;
    }

    custom_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(custom_provider, css);
    gtk_style_context_add_provider_for_display(
        display,
        GTK_STYLE_PROVIDER(custom_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 20u);
}
