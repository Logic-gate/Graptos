/**
 * @file src/main.c
 * @brief Entry point for ${project_name}.
 * @details The generated CLI keeps application code in src/ so headers, tests,
 *          and future modules can grow without moving the first file later.
 */

#include "${project_ident}.h"

#include <stdio.h>

/**
 * @brief Run the command-line application.
 * @details The starter executable prints a stable project identifier so the
 *          generated build files have an immediate visible output.
 * @return Zero when execution succeeds.
 */
int main(void) {
    puts("${project_name}");
    return 0;
}
