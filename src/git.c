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

#define CLEAF_GIT_MAX_OUTPUT (1024u * 1024u)

typedef enum {
    CLEAF_GIT_ERROR_NONE,
    CLEAF_GIT_ERROR_NOT_REPO,
    CLEAF_GIT_ERROR_AUTH,
    CLEAF_GIT_ERROR_NO_REMOTE,
    CLEAF_GIT_ERROR_NETWORK,
    CLEAF_GIT_ERROR_CONFLICT,
    CLEAF_GIT_ERROR_LOCAL_CHANGES,
    CLEAF_GIT_ERROR_NON_FAST_FORWARD,
    CLEAF_GIT_ERROR_DETACHED_HEAD,
    CLEAF_GIT_ERROR_NOTHING_TO_COMMIT,
    CLEAF_GIT_ERROR_GIT_MISSING,
    CLEAF_GIT_ERROR_OTHER
} CleafGitErrorKind;

typedef struct {
    int exit_code;
    char *out;
    char *err;
    CleafGitErrorKind kind;
    char *message;
} CleafGitResult;

#include "git/git_core.inc.c"
#include "git/git_actions.inc.c"
#include "git/git_credentials.inc.c"
