/**
 * @file src/index.c
 * @brief Project/reference index public API and implementation composition unit.
 * @details The index is our local memory of the project. It trades perfect semantic
 *          knowledge for speed and predictability, which is the right fallback when
 *          external tooling is absent or incomplete.
 */

#include "index.h"
#include "editor_tab.h"
#include "app.h"
#include "project.h"

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

/*
 * The reference index is an interactive editor aid, not a build-system parser.
 * These caps keep project indexing from competing with text input on large
 * repositories or generated files.
 */
#define GRAPTOS_INDEX_MAX_FILE_BYTES (768u * 1024u)
/**
 * @brief Graptoς index max project files macro.
 */
#define GRAPTOS_INDEX_MAX_PROJECT_FILES 600u
/**
 * @brief Graptoς index max include depth macro.
 */
#define GRAPTOS_INDEX_MAX_INCLUDE_DEPTH 2u
/**
 * @brief Graptoς index max line macro.
 */
#define GRAPTOS_INDEX_MAX_LINE 512u

#include "index/index_collect.inc.c"
#include "index/index_candidates.inc.c"
#include "index/index_references.inc.c"
