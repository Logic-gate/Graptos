/**
 * @file src/import_complete.c
 * @brief Import completion public API and candidate collection.
 */

#include "import_complete_private.h"

#include <string.h>


/**
 * @brief Compare strings.
 */
static gint compare_strings(gconstpointer a, gconstpointer b) {
    const char *sa = *(char * const *)a;
    const char *sb = *(char * const *)b;
    return g_ascii_strcasecmp(sa ? sa : "", sb ? sb : "");
}

/**
 * @brief Text line before cursor.
 */
static char *text_line_before_cursor(EditorTab *tab) {
    if (!tab || !tab->buffer) return NULL;
    GtkTextIter cursor;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, mark);
    GtkTextIter start = cursor;
    gtk_text_iter_set_line_offset(&start, 0);
    return gtk_text_buffer_get_text(tab->buffer, &start, &cursor, FALSE);
}

/**
 * @brief List has word.
 */
static gboolean list_has_word(GPtrArray *list, const char *line,
                              char **after_out) {
    if (!list || !line) return FALSE;
    for (guint i = 0u; i < list->len; i++) {
        const char *word = g_ptr_array_index(list, i);
        if (!word || word[0] == '\0') continue;
        gsize len = strlen(word);
        if (g_ascii_strncasecmp(line, word, len) != 0) continue;
        if (line[len] != '\0' && !g_ascii_isspace(line[len])) continue;
        if (after_out) {
            const char *after = line + len;
            while (*after && g_ascii_isspace(*after)) after++;
            *after_out = g_strdup(after);
        }
        return TRUE;
    }
    return FALSE;
}

/**
 * @brief Split path token.
 */
static void split_path_token(ImportParse *ctx, gboolean dot_modules) {
    const char *token = ctx->token ? ctx->token : "";
    const char *sep = strrchr(token, '/');
    if (!sep && dot_modules) sep = strrchr(token, '.');
    if (!sep) {
        ctx->dir_part = g_strdup("");
        ctx->base_part = g_strdup(token);
        return;
    }
    ctx->dir_part = g_strndup(token, (gsize)(sep - token + 1));
    ctx->base_part = g_strdup(sep + 1);
}

/**
 * @brief Strip quote tail.
 */
static char *strip_quote_tail(const char *text) {
    if (!text) return g_strdup("");
    char *out = g_strdup(text);
    char *single = strchr(out, '\'');
    char *dbl = strchr(out, '"');
    char *end = NULL;
    if (single && dbl) end = single < dbl ? single : dbl;
    else end = single ? single : dbl;
    if (end) *end = '\0';
    return g_strstrip(out);
}

/**
 * @brief Parse include line.
 */
static gboolean parse_include_line(const char *line, ImportParse *ctx) {
    const char *p = strstr(line, "#include");
    if (!p) return FALSE;
    const char *angle = strchr(p, '<');
    const char *quote = strchr(p, '"');
    const char *start = NULL;
    char close = '\0';
    if (quote && (!angle || quote < angle)) {
        start = quote + 1;
        close = '"';
        ctx->system_path = FALSE;
    } else if (angle) {
        start = angle + 1;
        close = '>';
        ctx->system_path = TRUE;
    }
    if (!start) return FALSE;
    ctx->kind = IMPORT_CTX_PATH;
    ctx->token = g_strdup(start);
    char *end = strchr(ctx->token, close);
    if (end) *end = '\0';
    split_path_token(ctx, FALSE);
    return TRUE;
}

/**
 * @brief Find keyword span.
 */
static const char *find_keyword_span(const char *line, GPtrArray *keywords,
                                     const char **kw_start_out) {
    if (!line || !keywords) return NULL;
    for (guint i = 0u; i < keywords->len; i++) {
        const char *kw = g_ptr_array_index(keywords, i);
        if (!kw || kw[0] == '\0') continue;
        const char *p = line;
        gsize len = strlen(kw);
        while ((p = g_strstr_len(p, -1, kw)) != NULL) {
            gboolean left_ok = p == line || !g_ascii_isalnum((guchar)p[-1]);
            gboolean right_ok = !g_ascii_isalnum((guchar)p[len]) &&
                                 p[len] != '_';
            if (left_ok && right_ok) {
                if (kw_start_out) *kw_start_out = p;
                return p + len;
            }
            p += len;
        }
    }
    return NULL;
}

/**
 * @brief Parse quoted import.
 */
static gboolean parse_quoted_import(const char *line, SyntaxDef *syntax,
                                    ImportParse *ctx) {
    if (!line || !syntax) return FALSE;
    if (!find_keyword_span(line, syntax->import_keywords, NULL) &&
        !find_keyword_span(line, syntax->import_from_keywords, NULL)) {
        return FALSE;
    }
    const char *single = strrchr(line, '\'');
    const char *dbl = strrchr(line, '"');
    const char *q = NULL;
    if (single && dbl) q = single > dbl ? single : dbl;
    else q = single ? single : dbl;
    if (!q) return FALSE;

    ctx->token = strip_quote_tail(q + 1);
    ctx->kind = IMPORT_CTX_MODULES;
    if (g_str_has_prefix(ctx->token, ".") ||
        g_str_has_prefix(ctx->token, "/")) {
        ctx->kind = IMPORT_CTX_PATH;
    }
    split_path_token(ctx, FALSE);
    return TRUE;
}

/**
 * @brief Parse member import.
 */
static gboolean parse_member_import(const char *line, SyntaxDef *syntax,
                                    ImportParse *ctx) {
    char *after_from = NULL;
    if (!list_has_word(syntax->import_from_keywords, line, &after_from)) {
        return FALSE;
    }

    const char *member_start = NULL;
    const char *after_member = find_keyword_span(after_from,
        syntax->import_member_keywords, &member_start);
    if (!after_member || !member_start) {
        ctx->kind = IMPORT_CTX_MODULES;
        ctx->token = g_strdup(after_from);
        char *space = strpbrk(ctx->token, " \t");
        if (space) *space = '\0';
        g_strstrip(ctx->token);
        split_path_token(ctx, syntax->import_dot_modules);
        g_free(after_from);
        return TRUE;
    }

    char *module = g_strndup(after_from, (gsize)(member_start - after_from));
    ctx->kind = IMPORT_CTX_MEMBERS;
    ctx->module = g_strdup(g_strstrip(module));
    ctx->token = g_strdup(after_member);
    if (ctx->token) {
        char *comma = strrchr(ctx->token, ',');
        if (comma) {
            char *last = g_strdup(comma + 1);
            g_free(ctx->token);
            ctx->token = last;
        }
        g_strstrip(ctx->token);
    }
    split_path_token(ctx, FALSE);
    g_free(module);
    g_free(after_from);
    return TRUE;
}

/**
 * @brief Parse module import.
 */
static gboolean parse_module_import(const char *line, SyntaxDef *syntax,
                                    ImportParse *ctx) {
    char *after = NULL;
    if (!list_has_word(syntax->import_keywords, line, &after)) return FALSE;
    ctx->kind = IMPORT_CTX_MODULES;
    ctx->token = g_strdup(after);
    if (ctx->token) {
        char *comma = strrchr(ctx->token, ',');
        if (comma) {
            char *last = g_strdup(comma + 1);
            g_free(ctx->token);
            ctx->token = last;
        }
        g_strstrip(ctx->token);
    }
    split_path_token(ctx, syntax->import_dot_modules);
    g_free(after);
    return TRUE;
}

/**
 * @brief Import parse clear.
 */
void import_parse_clear(ImportParse *ctx) {
    if (!ctx) return;
    g_free(ctx->module);
    g_free(ctx->token);
    g_free(ctx->dir_part);
    g_free(ctx->base_part);
    memset(ctx, 0, sizeof(*ctx));
}

/**
 * @brief Import parse current line.
 */
gboolean import_parse_current_line(EditorTab *tab, ImportParse *ctx) {
    if (!tab || !ctx) return FALSE;
    SyntaxDef *syntax = tab->active_syntax;
    char *line = text_line_before_cursor(tab);
    if (!line) return FALSE;
    char *trimmed = g_strstrip(line);
    gboolean ok = parse_include_line(trimmed, ctx);
    if (!ok && syntax) ok = parse_quoted_import(trimmed, syntax, ctx);
    if (!ok && syntax) ok = parse_member_import(trimmed, syntax, ctx);
    if (!ok && syntax) ok = parse_module_import(trimmed, syntax, ctx);
    g_free(line);
    return ok;
}


/**
 * @brief Import completion tab is import context.
 */
gboolean import_completion_tab_is_import_context(EditorTab *tab) {
    ImportParse ctx = {0};
    gboolean ok = import_parse_current_line(tab, &ctx);
    import_parse_clear(&ctx);
    return ok;
}

/**
 * @brief Import completion candidates for tab.
 */
GPtrArray *import_completion_candidates_for_tab(EditorTab *tab,
                                                const char *prefix,
                                                guint max_results) {
    if (max_results == 0u) max_results = CLEAF_IMPORT_COMPLETION_MAX_RESULTS;
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    if (!out) return NULL;
    ImportParse ctx = {0};
    if (!import_parse_current_line(tab, &ctx)) return out;
    if (prefix && prefix[0] && ctx.base_part) {
        g_free(ctx.base_part);
        ctx.base_part = g_strdup(prefix);
    }
    import_collect_candidates(tab, &ctx, out, max_results);
    g_ptr_array_sort(out, compare_strings);
    import_parse_clear(&ctx);
    return out;
}
