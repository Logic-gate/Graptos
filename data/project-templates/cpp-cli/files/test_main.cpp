#include "${project_ident}.hpp"

#include <cassert>
#include <string>

/// The starter test uses the standard library only so no test framework is
/// required before the project chooses one.
int main() {
    assert(${project_ident}::greeting() == std::string("Hello from ${project_name}"));
    return 0;
}
