/**
 * @file src/ui/ui_base_css.inc.c
 * @brief Graptoς ui base css module.
 * @details Theme values should be traceable. We translate config into GTK CSS here so
 *          colors and fonts do not get scattered through feature code where they are hard
 *          to reason about later.
 */

/**
 * @brief Return the Inconsolata font directory for development or installed runs.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *graptos_inconsolata_font_dir(void) {
    const char *dev_dir = "data/fonts/Inconsolata";
    if (g_file_test(dev_dir, G_FILE_TEST_IS_DIR)) return g_strdup(dev_dir);

    char *installed = g_build_filename(DATADIR, "fonts", "Inconsolata",
                                       NULL);
    if (installed && g_file_test(installed, G_FILE_TEST_IS_DIR)) {
        return installed;
    }

    g_free(installed);
    return NULL;
}

/**
 * @brief Append one Inconsolata font-face rule when the file exists.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param css The css supplied by the caller.
 * @param font_dir The font dir supplied by the caller.
 * @param file_name The file name supplied by the caller.
 * @param style The style supplied by the caller.
 * @param weight The weight supplied by the caller.
 */
static void graptos_append_inconsolata_face(GString *css,
                                            const char *font_dir,
                                            const char *file_name,
                                            const char *style,
                                            const char *weight) {
    if (!css || !font_dir || !file_name || !style || !weight) return;

    g_autofree char *path = g_build_filename(font_dir, file_name, NULL);
    if (!path || !g_file_test(path, G_FILE_TEST_IS_REGULAR)) return;

    g_autofree char *uri = g_filename_to_uri(path, NULL, NULL);
    if (!uri) return;

    g_string_append_printf(css,
        "@font-face {"
        " font-family: 'Inconsolata';"
        " font-style: %s;"
        " font-weight: %s;"
        " src: url('%s');"
        "}\n",
        style, weight, uri);
}

/**
 * @brief Append bundled Inconsolata font faces.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 * @param css The css supplied by the caller.
 */
static void graptos_append_inconsolata_faces(GString *css) {
    g_autofree char *font_dir = graptos_inconsolata_font_dir();
    if (!font_dir) return;

    graptos_append_inconsolata_face(css, font_dir,
                                    "static/Inconsolata-Regular.ttf",
                                    "normal", "400");
    graptos_append_inconsolata_face(css, font_dir,
                                    "static/Inconsolata-Regular.ttf",
                                    "italic", "400");
    graptos_append_inconsolata_face(css, font_dir,
                                    "static/Inconsolata-Bold.ttf",
                                    "normal", "700");
    graptos_append_inconsolata_face(css, font_dir,
                                    "static/Inconsolata-Bold.ttf",
                                    "italic", "700");
}

/**
 * @brief Apply global Graptoς CSS and register bundled Inconsolata font faces.
 * @details Shared UI helpers keep Graptoς styling and GTK ownership rules in one place. The comment notes the small contract each wrapper protects for callers.
 */
void graptos_apply_css(void) {
    static const char *css_chunks[] = {
        "window.graptos-window,"
        "window.graptos-window > contents,"
        "window.graptos-window > contents > *,"
        "window.graptos-window decoration,"
        "window.graptos-window:backdrop decoration {"
        "  border-radius: 0;"
        "  border-top-left-radius: 0;"
        "  border-top-right-radius: 0;"
        "  border-bottom-left-radius: 0;"
        "  border-bottom-right-radius: 0;"
        "  box-shadow: none;"
        "}"
        ".graptos-root, .graptos-root * {"
        "  border-radius: 0;"
        "  border-top-left-radius: 0;"
        "  border-top-right-radius: 0;"
        "  border-bottom-left-radius: 0;"
        "  border-bottom-right-radius: 0;"
        "  box-shadow: none;"
        "}"
        ".graptos-root box, .graptos-root paned, .graptos-root stack,"
        ".graptos-root revealer, .graptos-root frame, .graptos-root viewport,"
        ".graptos-root scrolledwindow, .graptos-root textview,"
        ".graptos-root textview text, .graptos-root listbox,"
        ".graptos-root listbox row, .graptos-root notebook,"
        ".graptos-root notebook * {"
        "  border-radius: 0;"
        "  box-shadow: none;"
        "}"
        "scrolledwindow.graptos-editor-scroll,"
        "scrolledwindow.graptos-minimap-scroll,"
        ".graptos-tab-page,"
        ".graptos-editor-area,"
        ".graptos-editor-overlay,"
        "textview.graptos-editor,"
        "textview.graptos-editor text,"
        "textview.graptos-minimap,"
        "textview.graptos-minimap text,"
        "textview.graptos-preview,"
        "textview.graptos-preview text,"
        "window.graptos-preview-window,"
        "window.graptos-preview-window > contents,"
        "window.graptos-preview-window > contents > *,"
        "window.graptos-preview-window decoration,"
        ".graptos-root viewport,"
        ".graptos-root notebook,"
        ".graptos-root notebook > stack {"
        "  border-radius: 0;"
        "  box-shadow: none;"
        "  border: none;"
        "}"
        ".graptos-preview-header {"
        "  min-height: 24px;"
        "  padding: 2px 4px;"
        "  border: none;"
        "  box-shadow: none;"
        "}"
        "button.graptos-preview-detach-button {"
        "  min-width: 22px;"
        "  min-height: 22px;"
        "  padding: 2px;"
        "  border: none;"
        "  border-radius: 0;"
        "  box-shadow: none;"
        "}"
        ".graptos-top {"
        "  border-bottom: 1px solid alpha(currentColor, 0.18);"
        "  padding: 3px 6px;"
        "}"
        ".graptos-title { font-weight: 600; padding-left: 14px; padding-right: 10px; }"
        ".graptos-root notebook { border: none; }"
        ".graptos-root notebook > header { border: none; }"
        ".graptos-root notebook > header.top { border-bottom: 1px solid alpha(currentColor, 0.18); }"
        ".graptos-root notebook > header.top > tabs > tab {"
        "  min-height: 34px;"
        "  padding: 0;"
        "  margin: 0 1px 0 0;"
        "  border: none;"
        "  border-right: 1px solid alpha(currentColor, 0.12);"
        "  border-radius: 0;"
        "  box-shadow: none;"
        "}"
        ".graptos-root notebook > header.top > tabs > tab * { border-radius: 0; box-shadow: none; }"
        ".graptos-root notebook > header.top > tabs > tab:checked { border-bottom: 2px solid #8bd5ca; }"
        ".graptos-tab { padding: 6px 16px; min-width: 160px; }"
        ".graptos-tab-tiled { box-shadow: inset 0 -2px #cba6f7; }"
        ".graptos-tab-close {"
        "  padding: 0 4px;"
        "  min-width: 18px;"
        "  min-height: 18px;"
        "  border: none;"
        "  background: transparent;"
        "}"
        ".graptos-tab-close:hover { border-radius: 0; }",
        ".graptos-root textview border { color: alpha(currentColor, 0.55); }"
        ".graptos-gutter { }"
        ".graptos-editor-tiles { border-top: 1px solid alpha(currentColor, 0.14); }"
        ".graptos-editor-tile-pane { border-right: 1px solid alpha(currentColor, 0.18); }"
        ".graptos-editor-tile-title { padding: 4px 8px; opacity: 0.72; }"
        ".graptos-minimap, .graptos-minimap text { font-family: monospace; font-size: 2.6pt; }"
        ".graptos-preview, .graptos-preview text { font-size: 11pt; }"
        "scrolledwindow.graptos-preview-scroll { border-left: 1px solid alpha(currentColor, 0.18); }"
        ".graptos-project-pane { border-right: 1px solid alpha(currentColor, 0.18); }"
        ".graptos-project-section { font-size: 9pt; }"
        ".graptos-project-hint { opacity: 0.65; font-size: 8pt; padding-bottom: 3px; }"
        ".graptos-project-tree { padding: 6px; border: none; }"
        ".graptos-project-tree row { min-height: 24px; }"
        ".graptos-project-icon { opacity: 0.78; font-size: 10pt; }"
        ".graptos-project-lock { opacity: 0.62; margin: 0 2px; }"
        ".graptos-git-status { font-weight: 800; min-width: 18px; opacity: 1.0; }"
        ".graptos-git-status-modified { color: #f9c74f; }"
        ".graptos-git-status-added { color: #57cc99; }"
        ".graptos-git-status-deleted { color: #ff6b6b; }"
        ".graptos-git-status-renamed { color: #4cc9f0; }"
        ".graptos-git-status-conflict { color: #ff4d6d; }"
        ".graptos-git-status-untracked { color: #77bdfb; }"
        ".graptos-git-status-staged { color: #cba6f7; }"
        ".graptos-search-panel { border-top: 1px solid alpha(currentColor, 0.18); padding: 5px 8px; }"
        ".graptos-tool-panel { border-top: 1px solid alpha(currentColor, 0.18); padding: 5px 8px; min-height: 34px; }"
        ".graptos-tool-title { opacity: 0.72; font-size: 8pt; font-weight: 700; padding: 0 4px; }"
        ".graptos-bottom { border-top: 1px solid alpha(currentColor, 0.18); padding: 3px 6px; }"
        ".graptos-bottom-controls { border: none; min-height: 34px; }"
        ".graptos-bottom-strip { padding: 0 2px; }"
        ".graptos-bottom-group { padding: 0 2px; }"
        ".graptos-bottom-title { opacity: 0.68; font-size: 8pt; font-weight: 700; padding: 0 4px; }"
        ".graptos-switch-row { padding: 0 1px; }"
        ".graptos-switch-label { opacity: 0.82; font-size: 8.5pt; padding: 0 2px; }"
        ".graptos-status { opacity: 0.85; padding: 3px 6px; }"
        ".graptos-status-error { font-weight: 700; }"
        ".graptos-codex-panel {"
        "  border-left: 1px solid alpha(currentColor, 0.18);"
        "  padding: 8px;"
        "}"
        ".graptos-codex-status { opacity: 0.68; font-size: 8.5pt; }"
        ".graptos-codex-panel textview { font-size: 10pt; }"
        ".graptos-codex-panel scrolledwindow {"
        "  border: 1px solid alpha(currentColor, 0.16);"
        "}"
        ".graptos-codex-approval {"
        "  border: 1px solid #d6a84b;"
        "  background: alpha(#d6a84b, 0.10);"
        "  padding: 8px;"
        "}"
        ".graptos-codex-review {"
        "  border: 1px solid alpha(#77bdfb, 0.55);"
        "  background: alpha(#77bdfb, 0.08);"
        "  padding: 8px;"
        "}"
        ".graptos-terminal-panel {"
        "  border-top: 1px solid alpha(currentColor, 0.18);"
        "}"
        ".graptos-terminal-header {"
        "  padding: 3px 8px;"
        "  border-bottom: 1px solid alpha(currentColor, 0.14);"
        "}"
        ".graptos-terminal-title {"
        "  opacity: 0.78;"
        "  font-size: 8.5pt;"
        "  font-weight: 700;"
        "}"
        ".graptos-terminal-height {"
        "  opacity: 0.65;"
        "  font-size: 8pt;"
        "  padding: 0 4px;"
        "}"
        ".graptos-terminal {"
        "  padding: 4px;"
        "}",
        ".graptos-indent-status { opacity: 0.72; padding: 3px 8px; }"
        ".graptos-menu-title { font-weight: 600; padding: 4px 2px; }"
        ".graptos-about-wordmark {"
        "  font-family: 'Inconsolata', monospace;"
        "  font-size: 30pt;"
        "  font-weight: 700;"
        "  color: currentColor;"
        "  padding: 10px 0 4px 0;"
        "}"
        ".graptos-about-subtitle {"
        "  font-family: 'Inconsolata', monospace;"
        "  font-style: italic;"
        "  opacity: 0.78;"
        "  padding: 0 0 8px 0;"
        "}"
        ".graptos-shortcuts-scroll { border: none; }"
        ".graptos-shortcuts-key {"
        "  font-family: 'Inconsolata', monospace;"
        "  font-weight: 700;"
        "  opacity: 0.92;"
        "}"
        ".graptos-shortcuts-action { opacity: 0.82; }"
        ".graptos-pref-tab { padding: 4px 14px; }"
        ".graptos-menu-small { opacity: 0.75; padding: 2px 2px; }"
        "button.graptos-flat {"
        "  padding: 3px 8px;"
        "  border-radius: 0;"
        "  background: transparent;"
        "}"
        "button.graptos-flat:hover { background: alpha(currentColor, 0.10); }"
        ".graptos-flat-dialog { border: none; box-shadow: none; outline: none; }"
        ".graptos-dialog-actions { padding-top: 4px; }"
        "button.graptos-dialog-action { min-height: 28px; padding: 4px 10px; }"
        "button.graptos-icon-tool { min-width: 28px; min-height: 28px; padding: 3px; }"
        "button.graptos-icon-tool image { -gtk-icon-size: 16px; }"
        ".graptos-root button { padding: 4px 8px; border-radius: 0; }"
        ".graptos-root button:hover { border-radius: 0; }"
        ".graptos-root entry { min-height: 24px; border-radius: 0; }"
        ".graptos-root dropdown, .graptos-root combobox, .graptos-root switch { border-radius: 0; }"
        ".graptos-root popover, .graptos-root popover contents, .graptos-root menu, .graptos-root menuitem { border-radius: 0; box-shadow: none; }"
        "popover.graptos-completion-popover,"
        "popover.graptos-hover-popover,"
        "popover.graptos-context-popover,"
        "popover.graptos-tools-popover {"
        "  border: none;"
        "  border-radius: 0;"
        "  box-shadow: none;"
        "}"
        "popover.graptos-completion-popover contents,"
        "popover.graptos-hover-popover contents,"
        "popover.graptos-context-popover contents,"
        "popover.graptos-tools-popover contents {"
        "  border: none;"
        "  border-radius: 0;"
        "  padding: 0;"
        "  box-shadow: none;"
        "}"
        "popover.graptos-completion-popover, popover.graptos-hover-popover,"
        "popover.graptos-context-popover, popover.graptos-tools-popover {"
        "  background-clip: border-box;"
        "}"
        "popover.graptos-completion-popover arrow,"
        "popover.graptos-hover-popover arrow,"
        "popover.graptos-context-popover arrow,"
        "popover.graptos-tools-popover arrow {"
        "  border: none;"
        "  background: transparent;"
        "  box-shadow: none;"
        "}"
        ".graptos-completion-scroll { border: none; }"
        ".graptos-completion-list, .graptos-ref-list { border: none; border-radius: 0; }"
        ".graptos-completion-list row, .graptos-ref-list row { border-radius: 0; padding: 1px; }"
        ".graptos-completion-list row:selected { background: #89b4fa; color: #11111b; }"
        ".graptos-completion-title { font-family: monospace; font-weight: 650; }"
        ".graptos-completion-detail { opacity: 0.72; font-size: 8.5pt; font-family: monospace; }"
        ".graptos-ref-list row:hover { border: none; outline: none; box-shadow: none; }"
        ".graptos-ref-heading { color: #cba6f7; font-weight: 700; }"
        ".graptos-ref-title { color: #89dceb; font-family: monospace; font-weight: 600; }"
        ".graptos-ref-kind { opacity: 0.72; font-style: italic; font-size: 8.5pt; }"
        ".graptos-ref-snippet { font-family: monospace; font-size: 9pt; }"
        ".graptos-color-label { padding: 4px; }"
    };

    GString *css = g_string_new(NULL);
    if (!css) return;
    graptos_append_inconsolata_faces(css);
    for (guint i = 0u; i < G_N_ELEMENTS(css_chunks); i++) {
        g_string_append(css, css_chunks[i]);
    }

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(provider, css->str);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    g_string_free(css, TRUE);
}
