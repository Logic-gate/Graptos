/**
 * @file src/import_complete.c
 * @brief Import completion public API and candidate collection.
 * @details Import completion is intentionally pragmatic. We resolve enough project-local
 *          structure to be helpful when LSP is unavailable, without pretending this is a
 *          compiler for every language.
 */

#include "import_complete_private.h"

#include <string.h>


/**
 * @brief Compare strings.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param a Pointer to the first string pointer.
 * @param b Pointer to the second string pointer.
 * @return The computed value requested by the caller.
 */
static gint compare_strings(gconstpointer a, gconstpointer b) {
    const char *sa = *(char * const *)a;
    const char *sb = *(char * const *)b;
    return g_ascii_strcasecmp(sa ? sa : "", sb ? sb : "");
}

/**
 * @brief Return whether a character can be part of a simple identifier.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param ch The ch supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean simple_identifier_char(char ch) {
    return g_ascii_isalnum((guchar)ch) || ch == '_';
}

/**
 * @brief Return whether a character can start a simple identifier.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param ch The ch supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean simple_identifier_start(char ch) {
    return g_ascii_isalpha((guchar)ch) || ch == '_';
}

/**
 * @brief Text line before cursor.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
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
 * @brief Text before the insertion cursor.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *text_before_cursor(EditorTab *tab) {
    if (!tab || !tab->buffer) return NULL;
    GtkTextIter start;
    GtkTextIter cursor;
    GtkTextMark *mark = gtk_text_buffer_get_insert(tab->buffer);
    gtk_text_buffer_get_start_iter(tab->buffer, &start);
    gtk_text_buffer_get_iter_at_mark(tab->buffer, &cursor, mark);
    return gtk_text_buffer_get_text(tab->buffer, &start, &cursor, FALSE);
}

/**
 * @brief List has word.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param list The list supplied by the caller.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param after_out Output storage filled when the operation can provide a value.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param ctx The ctx supplied by the caller.
 * @param dot_modules The dot modules supplied by the caller.
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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param text The text fragment supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param ctx The ctx supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param keywords The keywords supplied by the caller.
 * @param kw_start_out Output storage filled when the operation can provide a value.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param syntax The syntax definition used by the editor path.
 * @param ctx The ctx supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param syntax The syntax definition used by the editor path.
 * @param ctx The ctx supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param syntax The syntax definition used by the editor path.
 * @param ctx The ctx supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param ctx The ctx supplied by the caller.
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
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param ctx The ctx supplied by the caller.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
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
 * @brief Parse the identifier immediately before an assignment.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param eq The eq supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *alias_before_assignment(const char *line, const char *eq) {
    if (!line || !eq || eq <= line) return NULL;
    const char *p = eq;
    while (p > line && g_ascii_isspace((guchar)*(p - 1))) p--;
    const char *end = p;
    while (p > line && simple_identifier_char(*(p - 1))) p--;
    if (p == end || !simple_identifier_start(*p)) return NULL;
    return g_strndup(p, (gsize)(end - p));
}

/**
 * @brief Extract a quoted module path after a position.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param p The p supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *quoted_module_after(const char *p) {
    if (!p) return NULL;
    const char *single = strchr(p, '\'');
    const char *dbl = strchr(p, '"');
    const char *q = NULL;
    if (single && dbl) q = single < dbl ? single : dbl;
    else q = single ? single : dbl;
    if (!q) return NULL;
    char quote = *q++;
    const char *end = strchr(q, quote);
    if (!end || end == q) return NULL;
    return g_strndup(q, (gsize)(end - q));
}

/**
 * @brief Find an import keyword occurrence in a line.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param syntax The syntax definition used by the editor path.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static const char *line_import_keyword_position(SyntaxDef *syntax,
                                                const char *line) {
    if (!syntax || !syntax->import_keywords || !line) return NULL;
    for (guint i = 0u; i < syntax->import_keywords->len; i++) {
        const char *keyword = g_ptr_array_index(syntax->import_keywords, i);
        if (!keyword || keyword[0] == '\0') continue;
        const char *hit = strstr(line, keyword);
        if (hit) return hit;
    }
    return NULL;
}

/**
 * @brief Try to map assignment import syntax to an alias target.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param syntax The syntax definition used by the editor path.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param base The base supplied by the caller.
 * @param target_out Output storage filled when the operation can provide a value.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean alias_from_assignment_import(SyntaxDef *syntax,
                                             const char *line,
                                             const char *base,
                                             char **target_out) {
    if (!syntax || !line || !base || !target_out) return FALSE;
    const char *eq = strchr(line, '=');
    if (!eq) return FALSE;
    char *alias = alias_before_assignment(line, eq);
    if (!alias) return FALSE;
    gboolean alias_matches = strcmp(alias, base) == 0;
    g_free(alias);
    if (!alias_matches) return FALSE;

    const char *keyword = line_import_keyword_position(syntax, eq);
    if (!keyword) return FALSE;
    char *module = quoted_module_after(keyword);
    if (!module) return FALSE;
    *target_out = module;
    return TRUE;
}

/**
 * @brief Try to map Python-style import-as syntax to an alias target.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param base The base supplied by the caller.
 * @param target_out Output storage filled when the operation can provide a value.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean alias_from_import_as(const char *line,
                                     const char *base,
                                     char **target_out) {
    if (!line || !base || !target_out) return FALSE;
    char **tokens = g_strsplit_set(line, " \t", -1);
    gboolean ok = FALSE;
    for (guint i = 0u; tokens && tokens[i]; i++) {
        if (tokens[i][0] == '\0') continue;
        if (strcmp(tokens[i], "import") != 0) continue;
        const char *module = NULL;
        const char *alias = NULL;
        for (guint j = i + 1u; tokens[j]; j++) {
            if (tokens[j][0] == '\0') continue;
            if (!module) {
                module = tokens[j];
                continue;
            }
            if (strcmp(tokens[j], "as") == 0) {
                for (guint k = j + 1u; tokens[k]; k++) {
                    if (tokens[k][0] != '\0') {
                        alias = tokens[k];
                        break;
                    }
                }
                break;
            }
        }
        if (module && alias && strcmp(alias, base) == 0) {
            *target_out = g_strdup(module);
            ok = TRUE;
            break;
        }
    }
    g_strfreev(tokens);
    return ok;
}

/**
 * @brief Try to map JavaScript import-star syntax to an alias target.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param base The base supplied by the caller.
 * @param target_out Output storage filled when the operation can provide a value.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
static gboolean alias_from_import_star(const char *line,
                                       const char *base,
                                       char **target_out) {
    if (!line || !base || !target_out) return FALSE;
    if (!strstr(line, "import") || !strstr(line, " from ")) return FALSE;

    char *needle = g_strdup_printf(" as %s", base);
    gboolean has_alias = needle && strstr(line, needle);
    g_free(needle);
    if (!has_alias) return FALSE;

    char *module = quoted_module_after(strstr(line, " from "));
    if (!module) return FALSE;
    *target_out = module;
    return TRUE;
}

/**
 * @brief Resolve a visible completion base to an imported module/file target.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param base The base supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
static char *resolve_member_base_target(EditorTab *tab, const char *base) {
    SyntaxDef *syntax = tab ? tab->active_syntax : NULL;
    if (!tab || !syntax || !base || base[0] == '\0') return NULL;

    char *text = text_before_cursor(tab);
    if (!text) return NULL;

    char *target = NULL;
    char **lines = g_strsplit(text, "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        char *line = g_strdup(lines[i]);
        char *trimmed = g_strstrip(line);
        if (alias_from_assignment_import(syntax, trimmed, base, &target) ||
            alias_from_import_star(trimmed, base, &target) ||
            alias_from_import_as(trimmed, base, &target)) {
            g_free(line);
            break;
        }
        g_free(line);
    }

    g_strfreev(lines);
    g_free(text);
    if (target) return target;

    if (syntax->import_static_modules) {
        for (guint i = 0u; i < syntax->import_static_modules->len; i++) {
            const char *row = g_ptr_array_index(syntax->import_static_modules, i);
            const char *eq = row ? strchr(row, '=') : NULL;
            if (!eq) continue;
            gsize len = (gsize)(eq - row);
            if (len == strlen(base) && strncmp(row, base, len) == 0) {
                return g_strdup(base);
            }
        }
    }

    return NULL;
}


/**
 * @brief Import completion tab is import context.
 * @details Import completion is intentionally heuristic because every language writes imports differently. The comment calls out the guarded parsing path and the ownership of returned candidates.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean import_completion_tab_is_import_context(EditorTab *tab) {
    ImportParse ctx = {0};
    gboolean ok = import_parse_current_line(tab, &ctx);
    import_parse_clear(&ctx);
    return ok;
}

/**
 * @brief Import completion candidates for tab.
 * @details Import completion works from the current line instead of the whole
 *          file when possible. That keeps suggestions responsive and avoids
 *          pretending we have compiler-grade knowledge for every language.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *import_completion_candidates_for_tab(EditorTab *tab,
                                                const char *prefix,
                                                guint max_results) {
    if (max_results == 0u) max_results = GRAPTOS_IMPORT_COMPLETION_MAX_RESULTS;
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

/**
 * @brief Return member candidates for an imported alias or module.
 * @details Member completion starts with the name before the dot, then resolves
 *          that name back to the import target we can inspect. It is a useful
 *          fallback for simple project modules, not a replacement for semantic
 *          language-server completion.
 * @param tab The editor tab whose buffer or widgets are being inspected.
 * @param base The base supplied by the caller.
 * @param prefix The prefix supplied by the caller.
 * @param max_results The max results supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
GPtrArray *import_completion_member_candidates_for_tab(EditorTab *tab,
                                                       const char *base,
                                                       const char *prefix,
                                                       guint max_results) {
    if (max_results == 0u) max_results = GRAPTOS_IMPORT_COMPLETION_MAX_RESULTS;
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    if (!out) return NULL;
    if (!tab || !base || base[0] == '\0') return out;

    char *target = resolve_member_base_target(tab, base);
    if (!target) return out;

    ImportParse ctx = {0};
    ctx.kind = IMPORT_CTX_MEMBERS;
    ctx.module = g_strdup(target);
    ctx.token = g_strdup(prefix ? prefix : "");
    ctx.base_part = g_strdup(prefix ? prefix : "");
    import_collect_candidates(tab, &ctx, out, max_results);
    g_ptr_array_sort(out, compare_strings);
    import_parse_clear(&ctx);
    g_free(target);
    return out;
}
