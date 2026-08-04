static void bean_report_row_free(gpointer data) {
    BeanReportRow *row = data;
    if (!row) return;
    g_free(row->account);
    g_free(row->balance);
    g_free(row);
}

/**
 * @brief Free one balance report section.
 * @param data Section to free.
 */
static void bean_report_section_free(gpointer data) {
    BeanReportSection *section = data;
    if (!section) return;
    g_free(section->root);
    if (section->rows) g_ptr_array_free(section->rows, TRUE);
    if (section->totals) g_hash_table_destroy(section->totals);
    g_free(section);
}

/**
 * @brief Free a parsed balance report.
 * @param report Report to free.
 */
static void bean_report_free(BeanReport *report) {
    if (!report) return;
    g_free(report->ledger_path);
    g_free(report->ledger_name);
    g_free(report->generated_at);
    g_free(report->raw_output);
    if (report->sections) g_ptr_array_free(report->sections, TRUE);
    g_free(report);
}

/**
 * @brief Return the root of an account.
 * @param account Account name.
 * @return Owned account root.
 */
static char *account_root(const char *account) {
    const char *colon = account ? strchr(account, ':') : NULL;
    if (!account || !account[0]) return g_strdup("Other");
    if (!colon || colon == account) return g_strdup("Other");
    return g_strndup(account, (gsize)(colon - account));
}

/**
 * @brief Return whether text is an account balance row.
 * @param text Candidate line.
 * @return TRUE when the line starts with a Beancount account root.
 */
static gboolean balance_row_starts_account(const char *text) {
    const char *p = text ? text : "";
    while (*p == ' ' || *p == '\t') p++;
    return starts_account_root(p);
}

/**
 * @brief Parse one bean-query balance row.
 * @param line Output line from bean-query.
 * @param account_out Owned account output.
 * @param balance_out Owned balance output.
 * @return TRUE when a row was parsed.
 */
static gboolean parse_balance_row(const char *line,
                                  char **account_out,
                                  char **balance_out) {
    if (account_out) *account_out = NULL;
    if (balance_out) *balance_out = NULL;
    if (!balance_row_starts_account(line)) return FALSE;
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    const char *start = p;
    while (*p && (g_ascii_isalnum(*p) || *p == ':' || *p == '_' || *p == '-')) p++;
    if (p == start) return FALSE;
    char *account = g_strndup(start, (gsize)(p - start));
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) {
        g_free(account);
        return FALSE;
    }
    char *balance = g_strdup(p);
    g_strstrip(balance);
    if (!balance[0]) {
        g_free(account);
        g_free(balance);
        return FALSE;
    }
    if (account_out) *account_out = account;
    else g_free(account);
    if (balance_out) *balance_out = balance;
    else g_free(balance);
    return TRUE;
}

/**
 * @brief Parse a simple amount and commodity from balance text.
 * @details Complex inventories are preserved in report rows but skipped for
 *          subtotal math when the first value is not a plain amount.
 * @param balance Balance text from bean-query.
 * @param amount_out Parsed amount.
 * @param commodity_out Owned commodity output.
 * @return TRUE when a simple amount and commodity were parsed.
 */
static gboolean parse_balance_amount(const char *balance,
                                     double *amount_out,
                                     char **commodity_out) {
    if (amount_out) *amount_out = 0.0;
    if (commodity_out) *commodity_out = NULL;
    const char *p = balance ? balance : "";
    while (*p == ' ' || *p == '\t' || *p == '(') p++;
    char *end = NULL;
    double amount = g_ascii_strtod(p, &end);
    if (end == p) return FALSE;
    p = end;
    while (*p == ' ' || *p == '\t') p++;
    const char *start = p;
    while (*p && (g_ascii_isupper(*p) || g_ascii_isdigit(*p) || *p == '.' ||
                  *p == '_' || *p == '-')) {
        p++;
    }
    if (p == start) return FALSE;
    if (amount_out) *amount_out = amount;
    if (commodity_out) *commodity_out = g_strndup(start, (gsize)(p - start));
    return TRUE;
}

/**
 * @brief Add one amount to a section subtotal.
 * @param section Report section.
 * @param commodity Commodity symbol.
 * @param amount Amount to add.
 */
static void report_section_add_total(BeanReportSection *section,
                                     const char *commodity,
                                     double amount) {
    if (!section || !section->totals || !commodity || !commodity[0]) return;
    double *slot = g_hash_table_lookup(section->totals, commodity);
    if (!slot) {
        slot = g_new0(double, 1);
        g_hash_table_insert(section->totals, g_strdup(commodity), slot);
    }
    *slot += amount;
}

/**
 * @brief Return a report section by root, creating it if needed.
 * @param report Report receiving the section.
 * @param root Account root.
 * @return Section owned by the report.
 */
static BeanReportSection *report_section_for_root(BeanReport *report,
                                                  const char *root) {
    if (!report || !report->sections) return NULL;
    for (guint i = 0u; i < report->sections->len; i++) {
        BeanReportSection *section = g_ptr_array_index(report->sections, i);
        if (section && g_strcmp0(section->root, root) == 0) return section;
    }
    BeanReportSection *section = g_new0(BeanReportSection, 1);
    section->root = g_strdup(root && root[0] ? root : "Other");
    section->rows = g_ptr_array_new_with_free_func(bean_report_row_free);
    section->totals = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_ptr_array_add(report->sections, section);
    return section;
}

/**
 * @brief Add one parsed row to a report.
 * @param report Report receiving the row.
 * @param account Account name.
 * @param balance Balance text.
 */
static void report_add_row(BeanReport *report,
                           const char *account,
                           const char *balance) {
    g_autofree char *root = account_root(account);
    BeanReportSection *section = report_section_for_root(report, root);
    if (!section) return;
    BeanReportRow *row = g_new0(BeanReportRow, 1);
    row->account = g_strdup(account);
    row->balance = g_strdup(balance);
    g_ptr_array_add(section->rows, row);

    double amount = 0.0;
    g_autofree char *commodity = NULL;
    if (parse_balance_amount(balance, &amount, &commodity)) {
        report_section_add_total(section, commodity, amount);
    }
}

/**
 * @brief Build a report model from bean-query output.
 * @param path Ledger path used for the query.
 * @param output Raw bean-query output.
 * @return Parsed report.
 */
static BeanReport *bean_report_parse(const char *path, const char *output) {
    BeanReport *report = g_new0(BeanReport, 1);
    report->ledger_path = g_strdup(path);
    report->ledger_name = g_path_get_basename(path ? path : "ledger.bean");
    report->raw_output = g_strdup(output ? output : "");
    report->sections = g_ptr_array_new_with_free_func(bean_report_section_free);
    GDateTime *now = g_date_time_new_now_local();
    report->generated_at = now
        ? g_date_time_format(now, "%Y-%m-%d %H:%M:%S")
        : g_strdup("unknown time");
    if (now) g_date_time_unref(now);

    char **lines = g_strsplit(output ? output : "", "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        g_autofree char *account = NULL;
        g_autofree char *balance = NULL;
        if (parse_balance_row(lines[i], &account, &balance)) {
            report_add_row(report, account, balance);
        }
    }
    g_strfreev(lines);
    return report;
}

/**
 * @brief Append section subtotal lines to a string.
 * @param out Output string.
 * @param totals Commodity totals.
 * @param prefix Prefix before each total line.
 */
static void append_report_totals(GString *out,
                                 GHashTable *totals,
                                 const char *prefix) {
    if (!out || !totals || g_hash_table_size(totals) == 0u) return;
    GList *commodities = g_hash_table_get_keys(totals);
    commodities = g_list_sort(commodities, (GCompareFunc)g_strcmp0);
    for (GList *l = commodities; l; l = l->next) {
        const char *commodity = l->data;
        double *amount = g_hash_table_lookup(totals, commodity);
        if (!amount) continue;
        g_string_append_printf(out,
                               "%s%.2f %s\n",
                               prefix ? prefix : "",
                               *amount,
                               commodity);
    }
    g_list_free(commodities);
}

/**
 * @brief Build plain-text preview body for a balance report.
 * @param report Parsed report.
 * @return Owned preview text.
 */
static char *bean_report_to_text(BeanReport *report) {
    if (!report) return g_strdup("No report.");
    GString *out = g_string_new(NULL);
    g_string_append_printf(out,
                           "Ledger: %s\nGenerated: %s\nSource: bean-query %s\n\n",
                           report->ledger_name ? report->ledger_name : "ledger",
                           report->generated_at ? report->generated_at : "unknown",
                           BEANCOUNT_BALANCE_QUERY);
    if (!report->sections || report->sections->len == 0u) {
        g_string_append(out, report->raw_output && report->raw_output[0]
            ? report->raw_output
            : "No balances returned.");
        return g_string_free(out, FALSE);
    }
    for (guint i = 0u; i < report->sections->len; i++) {
        BeanReportSection *section = g_ptr_array_index(report->sections, i);
        if (!section) continue;
        g_string_append_printf(out, "%s\n", section->root ? section->root : "Other");
        g_string_append(out, "----------------------------------------\n");
        for (guint j = 0u; section->rows && j < section->rows->len; j++) {
            BeanReportRow *row = g_ptr_array_index(section->rows, j);
            if (!row) continue;
            g_string_append_printf(out, "%-44s %s\n",
                                   row->account ? row->account : "",
                                   row->balance ? row->balance : "");
        }
        if (g_hash_table_size(section->totals) > 0u) {
            g_string_append(out, "Subtotal:\n");
            append_report_totals(out, section->totals, "  ");
        }
        g_string_append_c(out, '\n');
    }
    return g_string_free(out, FALSE);
}

/**
 * @brief Escape text for LaTeX output.
 * @param text Text to escape.
 * @return Owned escaped text.
 */
static char *latex_escape(const char *text) {
    GString *out = g_string_new(NULL);
    for (const char *p = text ? text : ""; *p; p++) {
        switch (*p) {
            case '\\': g_string_append(out, "\\textbackslash{}"); break;
            case '&': g_string_append(out, "\\&"); break;
            case '%': g_string_append(out, "\\%"); break;
            case '$': g_string_append(out, "\\$"); break;
            case '#': g_string_append(out, "\\#"); break;
            case '_': g_string_append(out, "\\_"); break;
            case '{': g_string_append(out, "\\{"); break;
            case '}': g_string_append(out, "\\}"); break;
            case '~': g_string_append(out, "\\textasciitilde{}"); break;
            case '^': g_string_append(out, "\\textasciicircum{}"); break;
            default: g_string_append_c(out, *p); break;
        }
    }
    return g_string_free(out, FALSE);
}

/**
 * @brief Append one escaped LaTeX table row.
 * @param out Output LaTeX string.
 * @param left Left column.
 * @param right Right column.
 */
static void latex_append_row(GString *out, const char *left, const char *right) {
    g_autofree char *a = latex_escape(left);
    g_autofree char *b = latex_escape(right);
    g_string_append_printf(out,
                           "\\texttt{%s}\\\\\n"
                           "\\hspace*{1em}%s\\\\[0.35em]\n",
                           a ? a : "",
                           b ? b : "");
}

/**
 * @brief Append one escaped two-column LaTeX row.
 * @param out Output LaTeX string.
 * @param left Left column.
 * @param right Right column.
 */
static void latex_append_table_row(GString *out,
                                   const char *left,
                                   const char *right) {
    g_autofree char *a = latex_escape(left);
    g_autofree char *b = latex_escape(right);
    g_string_append_printf(out, "%s & %s \\\\\n", a ? a : "", b ? b : "");
}

/**
 * @brief Append totals to a LaTeX table.
 * @param out Output LaTeX string.
 * @param totals Commodity totals.
 */
static void latex_append_totals(GString *out, GHashTable *totals) {
    if (!out || !totals || g_hash_table_size(totals) == 0u) return;
    GList *commodities = g_hash_table_get_keys(totals);
    commodities = g_list_sort(commodities, (GCompareFunc)g_strcmp0);
    for (GList *l = commodities; l; l = l->next) {
        const char *commodity = l->data;
        double *amount = g_hash_table_lookup(totals, commodity);
        if (!amount) continue;
        g_autofree char *value = g_strdup_printf("%.2f %s", *amount, commodity);
        latex_append_row(out, "Subtotal", value);
    }
    g_list_free(commodities);
}

/**
 * @brief Count parsed report rows.
 * @param report Parsed report.
 * @return Number of account rows.
 */
static guint bean_report_row_count(BeanReport *report) {
    guint rows = 0u;
    for (guint i = 0u; report && report->sections && i < report->sections->len; i++) {
        BeanReportSection *section = g_ptr_array_index(report->sections, i);
        rows += section && section->rows ? section->rows->len : 0u;
    }
    return rows;
}

/**
 * @brief Append all account rows as escaped two-column LaTeX rows.
 * @param out Output LaTeX string.
 * @param report Parsed report.
 */
static void latex_append_all_rows(GString *out, BeanReport *report) {
    if (!out || !report || !report->sections) return;
    for (guint i = 0u; i < report->sections->len; i++) {
        BeanReportSection *section = g_ptr_array_index(report->sections, i);
        if (!section || !section->rows) continue;
        for (guint j = 0u; j < section->rows->len; j++) {
            BeanReportRow *row = g_ptr_array_index(section->rows, j);
            if (!row) continue;
            latex_append_table_row(out, row->account, row->balance);
        }
    }
}

/**
 * @brief Append all section subtotals as escaped three-column LaTeX rows.
 * @param out Output LaTeX string.
 * @param report Parsed report.
 */
static void latex_append_all_totals(GString *out, BeanReport *report) {
    if (!out || !report || !report->sections) return;
    for (guint i = 0u; i < report->sections->len; i++) {
        BeanReportSection *section = g_ptr_array_index(report->sections, i);
        if (!section || !section->totals || g_hash_table_size(section->totals) == 0u) {
            continue;
        }
        GList *commodities = g_hash_table_get_keys(section->totals);
        commodities = g_list_sort(commodities, (GCompareFunc)g_strcmp0);
        for (GList *l = commodities; l; l = l->next) {
            const char *commodity = l->data;
            double *amount = g_hash_table_lookup(section->totals, commodity);
            if (!amount) continue;
            g_autofree char *root = latex_escape(section->root ? section->root : "Other");
            g_autofree char *escaped_commodity = latex_escape(commodity);
            g_string_append_printf(out,
                                   "%s & %.2f & %s \\\\\n",
                                   root ? root : "",
                                   *amount,
                                   escaped_commodity ? escaped_commodity : "");
        }
        g_list_free(commodities);
    }
}

/**
 * @brief Append generated report sections as LaTeX.
 * @param out Output LaTeX string.
 * @param report Parsed report.
 */
static void latex_append_sections(GString *out, BeanReport *report) {
    if (!out) return;
    if (!report || !report->sections || report->sections->len == 0u) {
        g_string_append(out, "\\begin{verbatim}\n");
        g_string_append(out, report && report->raw_output
            ? report->raw_output
            : "No balances returned.");
        g_string_append(out, "\n\\end{verbatim}\n");
        return;
    }

    for (guint i = 0u; i < report->sections->len; i++) {
        BeanReportSection *section = g_ptr_array_index(report->sections, i);
        if (!section) continue;
        g_autofree char *root = latex_escape(section->root ? section->root : "Other");
        g_string_append_printf(out,
                               "\\subsection*{%s}\n"
                               "\\begin{longtable}{p{0.96\\linewidth}}\n",
                               root);
        for (guint j = 0u; section->rows && j < section->rows->len; j++) {
            BeanReportRow *row = g_ptr_array_index(section->rows, j);
            if (!row) continue;
            latex_append_row(out, row->account, row->balance);
        }
        if (g_hash_table_size(section->totals) > 0u) {
            g_string_append(out, "\\hline\n");
            latex_append_totals(out, section->totals);
        }
        g_string_append(out, "\\end{longtable}\n");
    }
}

/**
 * @brief Append generated report sections as two-column LaTeX tables.
 * @param out Output LaTeX string.
 * @param report Parsed report.
 */
static void latex_append_table_sections(GString *out, BeanReport *report) {
    if (!out) return;
    if (!report || !report->sections || report->sections->len == 0u) {
        g_string_append(out, "\\begin{verbatim}\n");
        g_string_append(out, report && report->raw_output
            ? report->raw_output
            : "No balances returned.");
        g_string_append(out, "\n\\end{verbatim}\n");
        return;
    }

    for (guint i = 0u; i < report->sections->len; i++) {
        BeanReportSection *section = g_ptr_array_index(report->sections, i);
        if (!section) continue;
        g_autofree char *root = latex_escape(section->root ? section->root : "Other");
        g_string_append_printf(out,
                               "\\subsection*{%s}\n"
                               "\\begin{longtable}{p{0.44\\linewidth}@{\\hspace{1em}}p{0.46\\linewidth}}\n"
                               "\\textbf{Account} & \\textbf{Balance}\\\\\\hline\n",
                               root);
        for (guint j = 0u; section->rows && j < section->rows->len; j++) {
            BeanReportRow *row = g_ptr_array_index(section->rows, j);
            if (!row) continue;
            latex_append_table_row(out, row->account, row->balance);
        }
        if (g_hash_table_size(section->totals) > 0u) {
            g_string_append(out, "\\hline\n");
            GList *commodities = g_hash_table_get_keys(section->totals);
            commodities = g_list_sort(commodities, (GCompareFunc)g_strcmp0);
            for (GList *l = commodities; l; l = l->next) {
                const char *commodity = l->data;
                double *amount = g_hash_table_lookup(section->totals, commodity);
                if (!amount) continue;
                g_autofree char *value = g_strdup_printf("%.2f %s", *amount, commodity);
                latex_append_table_row(out, "Subtotal", value);
            }
            g_list_free(commodities);
        }
        g_string_append(out, "\\end{longtable}\n");
    }
}

/**
 * @brief Replace all occurrences of a placeholder in text.
 * @param text Source text.
 * @param placeholder Placeholder token.
 * @param replacement Replacement text.
 * @return Owned rendered text.
 */
static char *latex_template_replace(const char *text,
                                    const char *placeholder,
                                    const char *replacement) {
    GString *out = g_string_new(NULL);
    const char *cursor = text ? text : "";
    gsize placeholder_len = strlen(placeholder);
    while (TRUE) {
        const char *match = g_strstr_len(cursor, -1, placeholder);
        if (!match) {
            g_string_append(out, cursor);
            break;
        }
        g_string_append_len(out, cursor, (gssize)(match - cursor));
        g_string_append(out, replacement ? replacement : "");
        cursor = match + placeholder_len;
    }
    return g_string_free(out, FALSE);
}

/**
 * @brief Render report placeholders into a LaTeX template.
 * @param template_source Full LaTeX template.
 * @param report Parsed report.
 * @param sections Generated LaTeX sections.
 * @return Owned rendered LaTeX source.
 */
static char *latex_template_render(const char *template_source,
                                   BeanReport *report,
                                   const char *sections) {
    g_autofree char *ledger = latex_escape(report && report->ledger_name
        ? report->ledger_name
        : "ledger");
    g_autofree char *generated = latex_escape(report && report->generated_at
        ? report->generated_at
        : "unknown");
    g_autofree char *query = latex_escape(BEANCOUNT_BALANCE_QUERY);
    g_autofree char *raw = latex_escape(report && report->raw_output
        ? report->raw_output
        : "");
    g_autofree char *ledger_path = latex_escape(report && report->ledger_path
        ? report->ledger_path
        : "");
    g_autofree char *section_count = g_strdup_printf("%u",
                                                     report && report->sections
                                                         ? report->sections->len
                                                         : 0u);
    g_autofree char *row_count = g_strdup_printf("%u", bean_report_row_count(report));
    g_autoptr(GString) table_sections = g_string_new(NULL);
    g_autoptr(GString) rows = g_string_new(NULL);
    g_autoptr(GString) totals = g_string_new(NULL);
    latex_append_table_sections(table_sections, report);
    latex_append_all_rows(rows, report);
    latex_append_all_totals(totals, report);
    g_autofree char *rendered = latex_template_replace(template_source,
                                                       "{{ledger_name}}",
                                                       ledger);
    g_autofree char *with_generated = latex_template_replace(rendered,
                                                            "{{generated_at}}",
                                                            generated);
    g_autofree char *with_query = latex_template_replace(with_generated,
                                                        "{{query}}",
                                                        query);
    g_autofree char *with_sections = latex_template_replace(with_query,
                                                           "{{sections}}",
                                                           sections);
    g_autofree char *with_stacked = latex_template_replace(with_sections,
                                                          "{{sections_stacked}}",
                                                          sections);
    g_autofree char *with_table_sections = latex_template_replace(with_stacked,
                                                                 "{{sections_table}}",
                                                                 table_sections->str);
    g_autofree char *with_rows = latex_template_replace(with_table_sections,
                                                       "{{rows}}",
                                                       rows->str);
    g_autofree char *with_totals = latex_template_replace(with_rows,
                                                         "{{totals}}",
                                                         totals->str);
    g_autofree char *with_raw = latex_template_replace(with_totals,
                                                      "{{raw_output}}",
                                                      raw);
    g_autofree char *with_path = latex_template_replace(with_raw,
                                                       "{{ledger_path}}",
                                                       ledger_path);
    g_autofree char *with_section_count = latex_template_replace(with_path,
                                                                "{{section_count}}",
                                                                section_count);
    return latex_template_replace(with_section_count,
                                  "{{row_count}}",
                                  row_count);
}

/**
 * @brief Return ledger-local LaTeX template path.
 * @param report Parsed report.
 * @return Owned path, or NULL.
 */
static char *latex_template_path(BeanReport *report) {
    if (!report || !report->ledger_path || !report->ledger_path[0]) return NULL;
    g_autofree char *dir = g_path_get_dirname(report->ledger_path);
    return g_build_filename(dir, ".graptos-beancount-template.tex", NULL);
}

/**
 * @brief Load the ledger-local LaTeX template if it exists.
 * @param report Parsed report.
 * @return Owned template source, or NULL.
 */
static char *latex_template_load(BeanReport *report) {
    g_autofree char *path = latex_template_path(report);
    if (!path || !g_file_test(path, G_FILE_TEST_IS_REGULAR)) return NULL;
    char *contents = NULL;
    if (!g_file_get_contents(path, &contents, NULL, NULL)) return NULL;
    return contents;
}

/**
 * @brief Build LaTeX source for a balance report.
 * @param report Parsed report.
 * @return Owned LaTeX source.
 */
static char *bean_report_to_latex(BeanReport *report) {
    static const char *default_template =
        "\\documentclass[11pt]{article}\n"
        "\\usepackage[margin=0.8in]{geometry}\n"
        "\\usepackage{longtable}\n"
        "\\usepackage{array}\n"
        "\\begin{document}\n"
        "\\section*{Beancount Balance Sheet}\n"
        "\\textbf{Ledger:} {{ledger_name}}\\\\\n"
        "\\textbf{Generated:} {{generated_at}}\\\\\n"
        "\\textbf{Source:} bean-query {{query}}\n\n"
        "{{sections}}\n"
        "\\end{document}\n";
    g_autofree char *custom_template = latex_template_load(report);
    const char *template_source = custom_template ? custom_template : default_template;
    g_autoptr(GString) sections = g_string_new(NULL);
    latex_append_sections(sections, report);
    return latex_template_render(template_source, report, sections->str);
}

/**
 * @brief Run bean-query and parse a balance report.
 * @param context Command context supplied by Graptoς.
 * @return Parsed report, or NULL on failure.
 */
static BeanReport *build_balance_report(GraptosPluginCommandContext *context) {
    char *path = saved_beancount_path(context, "Balance Sheet");
    if (!path) return NULL;
    if (!g_find_program_in_path("bean-query")) {
        graptos_plugin_context_show_output(context,
                                           "Balance Sheet",
                                           "bean-query Missing",
                                           "Install Beancount to use bean-query.");
        g_free(path);
        return NULL;
    }
    g_autofree char *cwd = g_path_get_dirname(path);
    char *argv[] = { "bean-query", path, BEANCOUNT_BALANCE_QUERY, NULL };
    g_autofree char *stdout_text = NULL;
    g_autofree char *stderr_text = NULL;
    gint status = 0;
    (void)run_tool(cwd, argv, &stdout_text, &stderr_text, &status);
    if (status != 0) {
        g_autofree char *body = format_tool_output("bean-query " BEANCOUNT_BALANCE_QUERY,
                                                   status,
                                                   stdout_text,
                                                   stderr_text);
        graptos_plugin_context_show_output(context,
                                           "Balance Sheet",
                                           "Bean Query Failed",
                                           body);
        g_free(path);
        return NULL;
    }
    BeanReport *report = bean_report_parse(path, stdout_text ? stdout_text : "");
    g_free(path);
    return report;
}

/**
 * @brief Show the Beancount balance sheet in the preview pane.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void balance_sheet_preview(GraptosPluginCommandContext *context,
                                  gpointer user_data) {
    (void)user_data;
    BeanReport *report = build_balance_report(context);
    if (!report) return;
    g_autofree char *body = bean_report_to_text(report);
    graptos_plugin_context_show_preview(context,
                                        "Beancount Balance Sheet",
                                        body);
    bean_report_free(report);
}

/**
 * @brief Export the Beancount balance sheet to PDF.
 * @param context Command context supplied by Graptoς.
 * @param user_data Plugin data supplied during registration.
 */
static void export_balance_sheet_pdf(GraptosPluginCommandContext *context,
                                     gpointer user_data) {
    (void)user_data;
    BeanReport *report = build_balance_report(context);
    if (!report) return;
    g_autofree char *latex = bean_report_to_latex(report);
    g_autofree char *stem = g_strdup_printf("%s-balance-sheet",
                                            report->ledger_name ? report->ledger_name : "ledger");
    (void)graptos_plugin_context_export_latex_pdf(context, stem, latex);
    bean_report_free(report);
}
