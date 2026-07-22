#ifndef ${project_macro}_HPP
#define ${project_macro}_HPP

#include <string>

/// The namespace keeps starter code out of the global scope without forcing a
/// larger directory layout on a new project.
namespace ${project_ident} {

/// Return the default CLI greeting.
inline std::string greeting() {
    return "Hello from ${project_name}";
}

}  // namespace ${project_ident}

#endif
