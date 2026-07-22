/**
 * @file tests/test_${project_ident}.c
 * @brief Starter test for ${project_name}.
 */

#include "${project_ident}.h"

#include <assert.h>
#include <string.h>

/**
 * @brief Verify the starter public symbol.
 * @return Zero when the assertion passes.
 */
int main(void) {
    assert(strcmp(${project_ident}_name(), "${project_name}") == 0);
    return 0;
}
