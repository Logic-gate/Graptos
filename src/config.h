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

#endif /* GRAPTOS_CONFIG_H */
