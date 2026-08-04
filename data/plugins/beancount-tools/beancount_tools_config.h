/**
 * @file beancount_tools_config.h
 * @brief Compile-time feature selection for the Beancount Tools plugin.
 *
 * Build examples:
 *
 *   cc -fPIC -shared -I../../../src beancount_tools.c \
 *     $(pkg-config --cflags --libs glib-2.0) \
 *     -o lib/libgraptos_beancount_tools.so
 *
 *   cc -fPIC -shared -I../../../src \
 *     -DBEANCOUNT_TOOLS_ENABLE_REPORT=0 beancount_tools.c \
 *     $(pkg-config --cflags --libs glib-2.0) \
 *     -o lib/libgraptos_beancount_tools.so
 */
#ifndef BEANCOUNT_TOOLS_CONFIG_H
#define BEANCOUNT_TOOLS_CONFIG_H

#ifndef BEANCOUNT_TOOLS_ENABLE_COMPLETION
#define BEANCOUNT_TOOLS_ENABLE_COMPLETION 1
#endif

#ifndef BEANCOUNT_TOOLS_ENABLE_HOVER
#define BEANCOUNT_TOOLS_ENABLE_HOVER 1
#endif

#ifndef BEANCOUNT_TOOLS_ENABLE_COMMANDS
#define BEANCOUNT_TOOLS_ENABLE_COMMANDS 1
#endif

#ifndef BEANCOUNT_TOOLS_ENABLE_REPORT
#define BEANCOUNT_TOOLS_ENABLE_REPORT 1
#endif

#endif
