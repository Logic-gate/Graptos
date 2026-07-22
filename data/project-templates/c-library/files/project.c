/**
 * @file src/${project_ident}.c
 * @brief Implementation for ${project_name}.
 * @details The first source file keeps one simple exported function so the
 *          generated library can be built and tested immediately.
 */

#include "${project_ident}.h"

/**
 * @brief Return the library display name.
 * @details The function gives the starter test a real public API call without
 *          choosing any domain-specific behavior for the generated project.
 * @return The project display name.
 */
const char *${project_ident}_name(void) {
    return "${project_name}";
}
