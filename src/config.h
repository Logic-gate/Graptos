/**
 * @file src/config.h
 * @brief Persistent Cleaf configuration loading and saving.
 */

#ifndef CLEAF_CONFIG_H
#define CLEAF_CONFIG_H

#include "app.h"

/**
 * @brief Cleaf config load.
 */
void cleaf_config_load(EditorWindow *win);
/**
 * @brief Cleaf config save.
 */
void cleaf_config_save(EditorWindow *win);
/**
 * @brief Cleaf config path.
 */
char *cleaf_config_path(void);

#endif /* CLEAF_CONFIG_H */
