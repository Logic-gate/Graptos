/**
 * @file beancount_tools.c
 * @brief Native plugin for Beancount ledger editing.
 * @details The plugin keeps Beancount-specific work outside Graptoς core:
 *          command execution uses installed bean-* tools, diagnostics are
 *          published through the public plugin ABI, and completions are built
 *          from the active buffer plus bounded include files.
 */

#include "plugin_api.h"

#include <glib/gstdio.h>
#include <ctype.h>
#include <string.h>

#define BEANCOUNT_MAX_OUTPUT_BYTES (128u * 1024u)
#define BEANCOUNT_MAX_COMPLETIONS 64u
#define BEANCOUNT_MAX_INCLUDE_FILES 32u
#define BEANCOUNT_MAX_INCLUDE_BYTES (2u * 1024u * 1024u)

/**
 * @brief Beancount balance query used by the report preview.
 */
#define BEANCOUNT_BALANCE_QUERY "BALANCES"

/**
 * @brief Completion dictionary built from ledger text.
 */
typedef struct {
    GHashTable *accounts; /**< Account names from postings and open directives. */
    GHashTable *commodities; /**< Commodity symbols from directives and amounts. */
    GHashTable *payees; /**< Payee strings from transactions. */
    GHashTable *narrations; /**< Narration strings from transactions. */
    GHashTable *tags; /**< Tag tokens including #. */
    GHashTable *links; /**< Link tokens including ^. */
    GHashTable *metadata; /**< Metadata keys including trailing colon. */
    GHashTable *balances; /**< Account to commodity balance tables. */
    GHashTable *visited; /**< Included files already parsed. */
    guint files; /**< Number of external include files parsed. */
    gsize bytes; /**< Total include bytes parsed. */
} BeanIndex;

/**
 * @brief One parsed balance report row.
 */
typedef struct {
    char *account; /**< Beancount account name. */
    char *balance; /**< Balance text returned by bean-query. */
} BeanReportRow;

/**
 * @brief One balance report section.
 */
typedef struct {
    char *root; /**< Account root shown as the section heading. */
    GPtrArray *rows; /**< BeanReportRow*. */
    GHashTable *totals; /**< Commodity to double subtotal. */
} BeanReportSection;

/**
 * @brief Parsed balance sheet report.
 */
typedef struct {
    char *ledger_path; /**< Ledger file used for bean-query. */
    char *ledger_name; /**< Ledger basename. */
    char *generated_at; /**< Human-readable generation timestamp. */
    char *raw_output; /**< Full bean-query output. */
    GPtrArray *sections; /**< BeanReportSection*. */
} BeanReport;

static const char *DIRECTIVES[] = {
    "open", "close", "commodity", "pad", "balance", "event", "price",
    "note", "document", "query", "custom", "txn", "option", "plugin",
    "include", "pushtag", "poptag"
};

gboolean graptos_plugin_register(GraptosPluginHost *host);

/**
 * @brief Return whether a path looks like a Beancount ledger.
 * @param path Active file path.
 * @return TRUE when the extension is supported.
 */
static gboolean is_beancount_path(const char *path) {
    return path &&
           (g_str_has_suffix(path, ".beancount") ||
            g_str_has_suffix(path, ".bean"));
}

/**
 * @brief Return whether the active editor is a Beancount editor.
 * @param context Command context supplied by Graptoς.
 * @return TRUE when Beancount commands should apply.
 */
static gboolean is_beancount_context(GraptosPluginCommandContext *context) {
    const char *syntax = graptos_plugin_context_syntax_name(context);
    if (g_strcmp0(syntax, "Beancount") == 0) return TRUE;
    char *path = graptos_plugin_context_file_path(context);
    gboolean match = is_beancount_path(path);
    g_free(path);
    return match;
}

/**
 * @brief Add an owned copy to a string set.
 * @param set Hash table used as a set.
 * @param value Candidate text.
 */
static void set_add(GHashTable *set, const char *value) {
    if (!set || !value || !value[0]) return;
    g_hash_table_add(set, g_strdup(value));
}

/**
 * @brief Return whether a Beancount account token starts here.
 * @param text Text to inspect.
 * @return TRUE when the token begins with a known account root.
 */
static gboolean starts_account_root(const char *text) {
    return g_str_has_prefix(text, "Assets:") ||
           g_str_has_prefix(text, "Liabilities:") ||
           g_str_has_prefix(text, "Equity:") ||
           g_str_has_prefix(text, "Income:") ||
           g_str_has_prefix(text, "Expenses:");
}

/**
 * @brief Free one account balance table.
 * @param data Commodity balance table.
 */
static void balance_table_free(gpointer data) {
    GHashTable *table = data;
    if (table) g_hash_table_destroy(table);
}

/**
 * @brief Return the balance table for an account.
 * @param index Ledger index.
 * @param account Account name.
 * @return Commodity balance table owned by the index.
 */
static GHashTable *balance_table_for_account(BeanIndex *index,
                                             const char *account) {
    if (!index || !account || !account[0]) return NULL;
    GHashTable *table = g_hash_table_lookup(index->balances, account);
    if (!table) {
        table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
        g_hash_table_insert(index->balances, g_strdup(account), table);
    }
    return table;
}

/**
 * @brief Add one amount to an account commodity balance.
 * @param index Ledger index.
 * @param account Posting account.
 * @param commodity Commodity symbol.
 * @param amount Posting amount.
 */
static void add_balance(BeanIndex *index,
                        const char *account,
                        const char *commodity,
                        double amount) {
    if (!index || !account || !commodity || !commodity[0]) return;
    GHashTable *table = balance_table_for_account(index, account);
    if (!table) return;
    double *slot = g_hash_table_lookup(table, commodity);
    if (!slot) {
        slot = g_new0(double, 1);
        g_hash_table_insert(table, g_strdup(commodity), slot);
    }
    *slot += amount;
}

/**
 * @brief Return whether a child account belongs under a parent account.
 * @param child Candidate child account.
 * @param parent Parent account under hover.
 * @return TRUE when child is parent or below parent.
 */
static gboolean account_is_under(const char *child, const char *parent) {
    if (!child || !parent || !parent[0]) return FALSE;
    gsize len = strlen(parent);
    return g_strcmp0(child, parent) == 0 ||
           (g_str_has_prefix(child, parent) && child[len] == ':');
}

/**
 * @brief Scan a posting amount into account balances.
 * @details This intentionally handles simple Beancount postings locally. It
 *          does not evaluate costs or conversions; amounts are summed by
 *          commodity exactly as written.
 * @param index Ledger index receiving balances.
 * @param line Ledger line without comments.
 */
static void scan_posting_balance(BeanIndex *index, const char *line) {
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (!starts_account_root(p)) return;

    const char *account_start = p;
    while (*p && (g_ascii_isalnum(*p) || *p == ':' || *p == '_' || *p == '-')) p++;
    char *account = g_strndup(account_start, (gsize)(p - account_start));
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) {
        g_free(account);
        return;
    }

    char *end = NULL;
    double amount = g_ascii_strtod(p, &end);
    if (end == p) {
        g_free(account);
        return;
    }
    p = end;
    while (*p == ' ' || *p == '\t') p++;
    const char *commodity_start = p;
    while (*p && (g_ascii_isupper(*p) || g_ascii_isdigit(*p) || *p == '.' ||
                  *p == '_' || *p == '-')) {
        p++;
    }
    if (p > commodity_start) {
        char *commodity = g_strndup(commodity_start,
                                    (gsize)(p - commodity_start));
        add_balance(index, account, commodity, amount);
        g_free(commodity);
    }
    g_free(account);
}

/**
 * @brief Extract and store account-like tokens from one line.
 * @param index Completion index receiving tokens.
 * @param line Ledger line.
 */
static void scan_accounts(BeanIndex *index, const char *line) {
    const char *p = line;
    while (p && *p) {
        while (*p && !g_ascii_isalpha(*p)) p++;
        if (!starts_account_root(p)) {
            while (*p && (g_ascii_isalnum(*p) || *p == '_' || *p == '-')) p++;
            continue;
        }
        const char *start = p;
        while (*p && (g_ascii_isalnum(*p) || *p == ':' || *p == '_' || *p == '-')) p++;
        char *account = g_strndup(start, (gsize)(p - start));
        set_add(index->accounts, account);
        g_free(account);
    }
}

/**
 * @brief Extract and store commodity-like uppercase symbols from one line.
 * @param index Completion index receiving tokens.
 * @param line Ledger line.
 */
static void scan_commodities(BeanIndex *index, const char *line) {
    const char *p = line;
    while (p && *p) {
        while (*p && !g_ascii_isupper(*p)) p++;
        const char *start = p;
        while (*p && (g_ascii_isupper(*p) || g_ascii_isdigit(*p) || *p == '.')) p++;
        if ((p - start) >= 2) {
            char *commodity = g_strndup(start, (gsize)(p - start));
            set_add(index->commodities, commodity);
            g_free(commodity);
        }
    }
}

/**
 * @brief Extract and store Beancount tags or links from one line.
 * @param set Hash table used as a set.
 * @param line Ledger line.
 * @param marker Token marker, usually # or ^.
 */
static void scan_marked_tokens(GHashTable *set, const char *line, char marker) {
    const char *p = line;
    while (p && *p) {
        if (*p++ != marker) continue;
        const char *start = p - 1;
        while (*p && (g_ascii_isalnum(*p) || *p == '_' || *p == '-')) p++;
        if (p - start > 1) {
            char *token = g_strndup(start, (gsize)(p - start));
            set_add(set, token);
            g_free(token);
        }
    }
}

/**
 * @brief Extract metadata key and quoted strings from one line.
 * @param index Completion index receiving values.
 * @param line Ledger line.
 */
static void scan_metadata_and_strings(BeanIndex *index, const char *line) {
    const char *trim = line;
    while (*trim == ' ' || *trim == '\t') trim++;
    const char *colon = strchr(trim, ':');
    if (colon && colon > trim && !strchr(trim, ' ') && !strchr(trim, '\t')) {
        char *key = g_strndup(trim, (gsize)(colon - trim + 1));
        set_add(index->metadata, key);
        g_free(key);
    }

    const char *first = strchr(line, '"');
    if (!first) return;
    const char *first_end = strchr(first + 1, '"');
    if (!first_end) return;
    char *payee = g_strndup(first + 1, (gsize)(first_end - first - 1));
    set_add(index->payees, payee);
    g_free(payee);

    const char *second = strchr(first_end + 1, '"');
    if (!second) return;
    const char *second_end = strchr(second + 1, '"');
    if (!second_end) return;
    char *narration = g_strndup(second + 1, (gsize)(second_end - second - 1));
    set_add(index->narrations, narration);
    g_free(narration);
}

/**
 * @brief Parse one include directive.
 * @param line Ledger line.
 * @return Owned include path, or NULL.
 */
static char *include_path_from_line(const char *line) {
    const char *trim = line;
    while (*trim == ' ' || *trim == '\t') trim++;
    if (!g_str_has_prefix(trim, "include")) return NULL;
    const char *quote = strchr(trim, '"');
    if (!quote) return NULL;
    const char *end = strchr(quote + 1, '"');
    if (!end || end == quote + 1) return NULL;
    return g_strndup(quote + 1, (gsize)(end - quote - 1));
}

static void index_file(BeanIndex *index, const char *path);

/**
 * @brief Parse ledger text into the completion index.
 * @param index Completion index.
 * @param text Ledger text.
 * @param base_dir Directory used to resolve include directives.
 */
static void index_text(BeanIndex *index, const char *text, const char *base_dir) {
    char **lines = g_strsplit(text ? text : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        char *line = lines[i];
        char *comment = strchr(line, ';');
        if (comment) *comment = '\0';
        scan_accounts(index, line);
        scan_posting_balance(index, line);
        scan_commodities(index, line);
        scan_marked_tokens(index->tags, line, '#');
        scan_marked_tokens(index->links, line, '^');
        scan_metadata_and_strings(index, line);

        char *include_path = include_path_from_line(line);
        if (include_path && base_dir) {
            char *resolved = g_path_is_absolute(include_path)
                ? g_strdup(include_path)
                : g_build_filename(base_dir, include_path, NULL);
            index_file(index, resolved);
            g_free(resolved);
        }
        g_free(include_path);
    }
    g_strfreev(lines);
}

/**
 * @brief Parse one included ledger file within fixed safety limits.
 * @param index Completion index.
 * @param path Include path to parse.
 */
static void index_file(BeanIndex *index, const char *path) {
    if (!index || !path || index->files >= BEANCOUNT_MAX_INCLUDE_FILES ||
        index->bytes >= BEANCOUNT_MAX_INCLUDE_BYTES) {
        return;
    }
    char *canonical = g_canonicalize_filename(path, NULL);
    if (g_hash_table_contains(index->visited, canonical)) {
        g_free(canonical);
        return;
    }
    g_hash_table_add(index->visited, g_strdup(canonical));

    char *contents = NULL;
    gsize len = 0u;
    if (g_file_get_contents(canonical, &contents, &len, NULL)) {
        index->files++;
        index->bytes += len;
        if (index->bytes <= BEANCOUNT_MAX_INCLUDE_BYTES) {
            char *base_dir = g_path_get_dirname(canonical);
            index_text(index, contents, base_dir);
            g_free(base_dir);
        }
    }
    g_free(contents);
    g_free(canonical);
}

/**
 * @brief Create a new completion index.
 * @return Newly allocated index.
 */
static BeanIndex *bean_index_new(void) {
    BeanIndex *index = g_new0(BeanIndex, 1);
    index->accounts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->commodities = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->payees = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->narrations = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->tags = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->links = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->metadata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    index->balances = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, balance_table_free);
    index->visited = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    return index;
}

/**
 * @brief Free a completion index.
 * @param index Completion index.
 */
static void bean_index_free(BeanIndex *index) {
    if (!index) return;
    g_hash_table_destroy(index->accounts);
    g_hash_table_destroy(index->commodities);
    g_hash_table_destroy(index->payees);
    g_hash_table_destroy(index->narrations);
    g_hash_table_destroy(index->tags);
    g_hash_table_destroy(index->links);
    g_hash_table_destroy(index->metadata);
    g_hash_table_destroy(index->balances);
    g_hash_table_destroy(index->visited);
    g_free(index);
}

/**
 * @brief Return a saved Beancount path for file-backed commands.
 * @param context Command context supplied by Graptoς.
 * @param command_name Name used in user-facing errors.
 * @return Owned active path, or NULL when the command cannot run.
 */
static char *saved_beancount_path(GraptosPluginCommandContext *context,
                                  const char *command_name) {
    char *path = graptos_plugin_context_file_path(context);
    if (!is_beancount_context(context) || !path || !path[0]) {
        graptos_plugin_context_show_output(context,
                                           command_name,
                                           "No Beancount File",
                                           "Open a saved .bean or .beancount file first.");
        g_free(path);
        return NULL;
    }
    if (graptos_plugin_context_is_modified(context)) {
        graptos_plugin_context_show_output(context,
                                           command_name,
                                           "Save Required",
                                           "Save the ledger before running this file-backed Beancount command.");
        g_free(path);
        return NULL;
    }
    return path;
}

/**
 * @brief Run an external Beancount command.
 * @param cwd Working directory.
 * @param argv Null-terminated command vector.
 * @param stdout_text Captured stdout.
 * @param stderr_text Captured stderr.
 * @param status Process status.
 * @return TRUE when the process launched.
 */
static gboolean run_tool(const char *cwd,
                         char **argv,
                         char **stdout_text,
                         char **stderr_text,
                         gint *status) {
    GError *error = NULL;
    gboolean ok = g_spawn_sync(cwd,
                               argv,
                               NULL,
                               G_SPAWN_SEARCH_PATH,
                               NULL,
                               NULL,
                               stdout_text,
                               stderr_text,
                               status,
                               &error);
    if (!ok && error) {
        *stderr_text = g_strdup(error->message);
        g_clear_error(&error);
    }
    return ok;
}

/**
 * @brief Join process output into a bounded dialog body.
 * @param command Command text to show.
 * @param status Process wait status.
 * @param stdout_text Captured stdout.
 * @param stderr_text Captured stderr.
 * @return Newly allocated dialog body.
 */
static char *format_tool_output(const char *command,
                                gint status,
                                const char *stdout_text,
                                const char *stderr_text) {
    GString *out = g_string_new(NULL);
    g_string_append_printf(out, "Command: %s\nExit status: %d\n\nstdout:\n%s\n\nstderr:\n%s",
                           command,
                           status,
                           stdout_text && stdout_text[0] ? stdout_text : "(empty)",
                           stderr_text && stderr_text[0] ? stderr_text : "(empty)");
    if (out->len > BEANCOUNT_MAX_OUTPUT_BYTES) {
        g_string_truncate(out, BEANCOUNT_MAX_OUTPUT_BYTES);
        g_string_append(out, "\n\n[output truncated]");
    }
    return g_string_free(out, FALSE);
}
