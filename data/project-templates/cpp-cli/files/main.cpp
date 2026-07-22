#include "${project_ident}.hpp"

#include <iostream>

/// Main stays thin so application behavior can move into testable code.
int main() {
    std::cout << ${project_ident}::greeting() << '\n';
    return 0;
}
