#ifndef CLEAF_CONFIG_H
#define CLEAF_CONFIG_H

#include "app.h"

void cleaf_config_load(EditorWindow *win);
void cleaf_config_save(EditorWindow *win);
char *cleaf_config_path(void);

#endif /* CLEAF_CONFIG_H */
