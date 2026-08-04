/**
 * @file src/config.h
 * @brief Persistent Graptoς configuration loading and saving.
 * @details Configuration is the shared contract between defaults, manual edits, and live
 *          theme changes. We parse it once here so individual features do not invent their
 *          own config behavior.
 */

#ifndef GRAPTOS_CONFIG_H
#define GRAPTOS_CONFIG_H

#include "app.h"

/**
 * @brief Graptoς config load.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @param win The win supplied by the caller.
 */
void graptos_config_load(EditorWindow *win);
/**
 * @brief Graptoς config save.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @param win The win supplied by the caller.
 */
void graptos_config_save(EditorWindow *win);
/**
 * @brief Graptoς config path.
 * @details Configuration values are user data, not internal constants. The comment makes the fallback path explicit so missing keys do not overwrite intentional manual edits.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *graptos_config_path(void);
/**
 * @brief Resolve the configured LaTeX executable.
 * @details The window config wins, then the legacy GRAPTOS_LATEX_COMMAND
 *          environment variable, then auto-detection.
 * @param win The window whose config is read.
 * @return Owned command, or NULL when no command is available.
 */
char *graptos_latex_resolve_command(EditorWindow *win);
/**
 * @brief Build a LaTeX argv from configured arguments.
 * @details Arguments are parsed with shell quoting rules but are executed
 *          directly without a shell. Supported placeholders are {output_dir}
 *          and {source_path}.
 * @param win The window whose config is read.
 * @param command Resolved LaTeX executable.
 * @param output_dir Output directory placeholder value.
 * @param source_path Source path placeholder value.
 * @param error Error output for invalid argument syntax.
 * @return NULL-terminated argv owned by the caller, or NULL.
 */
char **graptos_latex_build_argv(EditorWindow *win,
                                const char *command,
                                const char *output_dir,
                                const char *source_path,
                                GError **error);
/**
 * @brief Load theme values from a Graptoς CSS theme file.
 * @details CSS is the primary theme source. This parser reads the managed
 *          `@define-color graptos_*` values and font metadata comments into the
 *          window's in-memory theme fields.
 * @param win The window receiving theme values.
 * @param path The CSS theme file path.
 * @return TRUE when the CSS file was read.
 */
gboolean graptos_theme_css_load_into_window(EditorWindow *win,
                                            const char *path);
/**
 * @brief Save the window theme into a Graptoς CSS theme file.
 * @details The managed theme block is replaced while user CSS outside that
 *          block is preserved. This keeps the Theme dialog structured without
 *          taking ownership of hand-written CSS.
 * @param win The window whose theme values are written.
 * @param path The CSS theme file path.
 * @param preserve_custom TRUE to keep CSS outside the managed block.
 * @return TRUE when the CSS reached disk.
 */
gboolean graptos_theme_css_save_from_window(EditorWindow *win,
                                            const char *path,
                                            gboolean preserve_custom);

#endif /* GRAPTOS_CONFIG_H */
