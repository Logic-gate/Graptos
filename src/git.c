/**
 * @file src/git.c
 * @brief Git integration public API and implementation composition unit.
 * @details Git already knows the repository truth. Graptoς adds UI, status parsing,
 *          credentials, and command wiring, but we avoid building a half-Git inside the
 *          editor.
 */

#include "git.h"
#include "app.h"
#include "dialogs.h"
#include "editor_tab.h"
#include "project.h"
#include "syntax.h"
#include "ui.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdarg.h>
#include <string.h>

/**
 * @brief Graptoς git max output macro.
 */
#define GRAPTOS_GIT_MAX_OUTPUT (1024u * 1024u)

/**
 * @brief Git type definition.
 */
typedef enum {
    GRAPTOS_GIT_ERROR_NONE, /**< Graptoς git error none. */
    GRAPTOS_GIT_ERROR_NOT_REPO, /**< Graptoς git error not repo. */
    GRAPTOS_GIT_ERROR_AUTH, /**< Graptoς git error auth. */
    GRAPTOS_GIT_ERROR_NO_REMOTE, /**< Graptoς git error no remote. */
    GRAPTOS_GIT_ERROR_NETWORK, /**< Graptoς git error network. */
    GRAPTOS_GIT_ERROR_CONFLICT, /**< Graptoς git error conflict. */
    GRAPTOS_GIT_ERROR_LOCAL_CHANGES, /**< Graptoς git error local changes. */
    GRAPTOS_GIT_ERROR_NON_FAST_FORWARD, /**< Graptoς git error non fast forward. */
    GRAPTOS_GIT_ERROR_DETACHED_HEAD, /**< Graptoς git error detached head. */
    GRAPTOS_GIT_ERROR_NOTHING_TO_COMMIT, /**< Graptoς git error nothing to commit. */
    GRAPTOS_GIT_ERROR_GIT_MISSING, /**< Graptoς git error git missing. */
    GRAPTOS_GIT_ERROR_OTHER /**< Graptoς git error other. */
} GraptosGitErrorKind;

/**
 * @brief Git type definition.
 */
typedef struct {
    int exit_code; /**< Exit code. */
    char *out; /**< Out. */
    char *err; /**< Err. */
    GraptosGitErrorKind kind; /**< Kind. */
    char *message; /**< Message. */
} GraptosGitResult;

#include "git/git_core.inc.c"
#include "git/git_actions.inc.c"
#include "git/git_credentials.inc.c"
