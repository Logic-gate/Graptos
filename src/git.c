/**
 * @file src/git.c
 * @brief Git integration public API and implementation composition unit.
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
 * @brief Cleaf git max output macro.
 */
#define CLEAF_GIT_MAX_OUTPUT (1024u * 1024u)

/**
 * @brief Git type definition.
 */
typedef enum {
    CLEAF_GIT_ERROR_NONE, /**< Cleaf git error none. */
    CLEAF_GIT_ERROR_NOT_REPO, /**< Cleaf git error not repo. */
    CLEAF_GIT_ERROR_AUTH, /**< Cleaf git error auth. */
    CLEAF_GIT_ERROR_NO_REMOTE, /**< Cleaf git error no remote. */
    CLEAF_GIT_ERROR_NETWORK, /**< Cleaf git error network. */
    CLEAF_GIT_ERROR_CONFLICT, /**< Cleaf git error conflict. */
    CLEAF_GIT_ERROR_LOCAL_CHANGES, /**< Cleaf git error local changes. */
    CLEAF_GIT_ERROR_NON_FAST_FORWARD, /**< Cleaf git error non fast forward. */
    CLEAF_GIT_ERROR_DETACHED_HEAD, /**< Cleaf git error detached head. */
    CLEAF_GIT_ERROR_NOTHING_TO_COMMIT, /**< Cleaf git error nothing to commit. */
    CLEAF_GIT_ERROR_GIT_MISSING, /**< Cleaf git error git missing. */
    CLEAF_GIT_ERROR_OTHER /**< Cleaf git error other. */
} CleafGitErrorKind;

/**
 * @brief Git type definition.
 */
typedef struct {
    int exit_code; /**< Exit code. */
    char *out; /**< Out. */
    char *err; /**< Err. */
    CleafGitErrorKind kind; /**< Kind. */
    char *message; /**< Message. */
} CleafGitResult;

#include "git/git_core.inc.c"
#include "git/git_actions.inc.c"
#include "git/git_credentials.inc.c"
