/**
 * @file src/editor_tab_latex.c
 * @brief Cleaf editor tab latex module.
 */

#include "editor_tab_private.h"

#include <ctype.h>
#include <sys/stat.h>

/**
 * @brief Latex build dir macro.
 */
#define LATEX_BUILD_DIR ".cleaf-latex-build"

/**
 * @brief Has space.
 */
static gboolean has_space(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    while (p && *p != '\0') {
        if (g_ascii_isspace(*p)) return TRUE;
        p++;
    }
    return FALSE;
}

/**
 * @brief Find latex command.
 */
static char *find_latex_command(void) {
    const char *env = g_getenv("CLEAF_LATEX_COMMAND");
    if (env && env[0] != '\0' && !has_space(env)) return g_strdup(env);

    static const char *commands[] = {
        "pdflatex",
        "xelatex",
        "lualatex",
        NULL
    };

    for (guint i = 0u; commands[i] != NULL; i++) {
        char *path = g_find_program_in_path(commands[i]);
        if (path) {
            g_free(path);
            return g_strdup(commands[i]);
        }
    }
    return NULL;
}

/**
 * @brief Basename without suffix.
 */
static char *basename_without_suffix(const char *path) {
    g_autofree char *base = g_path_get_basename(path ? path : "document.tex");
    char *dot = base ? strrchr(base, '.') : NULL;
    if (dot && dot != base) *dot = '\0';
    if (!base || base[0] == '\0') {
        return g_strdup("document");
    }
    return g_steal_pointer(&base);
}

/**
 * @brief Ensure saved source.
 */
static gboolean ensure_saved_source(EditorTab *tab, char **source_path_out) {
    if (!tab || !source_path_out) return FALSE;

    if (tab->file_path) {
        if (tab->modified && !editor_tab_save(tab, FALSE)) return FALSE;
        *source_path_out = g_strdup(tab->file_path);
        return *source_path_out != NULL;
    }

    g_autofree char *text = buffer_text(tab);
    if (!text) return FALSE;

    g_autoptr(GError) error = NULL;
    g_autofree char *tmp_dir = g_dir_make_tmp("cleaf-latex-XXXXXX", &error);
    if (!tmp_dir) {
        app_window_set_error_status(tab->win, "Could not create temp dir",
                                    error ? error->message : "Unknown error");
        return FALSE;
    }

    char *source = g_build_filename(tmp_dir, "document.tex", NULL);
    if (!write_text_atomic(source, text, &error)) {
        app_window_set_error_status(tab->win, "Could not write temp TeX file",
                                    error ? error->message : "Unknown error");
        g_free(source);
        return FALSE;
    }

    *source_path_out = source;
    return TRUE;
}

/**
 * @brief Build output dir for source.
 */
static char *build_output_dir_for_source(const char *source_path) {
    g_autofree char *dir = g_path_get_dirname(source_path);
    if (!dir) return NULL;

    char *output = g_build_filename(dir, LATEX_BUILD_DIR, NULL);
    if (g_mkdir_with_parents(output, 0700) != 0) {
        g_free(output);
        return NULL;
    }
    return output;
}

/**
 * @brief Latex log message.
 */
static char *latex_log_message(const char *command,
                               const char *stdout_text,
                               const char *stderr_text) {
    GString *msg = g_string_new(NULL);
    g_string_append_printf(msg, "Command: %s\n\n", command ? command : "latex");
    if (stdout_text && stdout_text[0] != '\0') {
        g_string_append(msg, "Output:\n");
        g_string_append_len(msg, stdout_text, 4000);
        if (strlen(stdout_text) > 4000u) g_string_append(msg, "\n...truncated...");
        g_string_append(msg, "\n\n");
    }
    if (stderr_text && stderr_text[0] != '\0') {
        g_string_append(msg, "Errors:\n");
        g_string_append_len(msg, stderr_text, 4000);
        if (strlen(stderr_text) > 4000u) g_string_append(msg, "\n...truncated...");
    }
    return g_string_free(msg, FALSE);
}

/**
 * @brief Run latex.
 */
static gboolean run_latex(EditorTab *tab,
                          const char *command,
                          const char *source_path,
                          const char *working_dir,
                          const char *output_dir) {
    g_autofree char *stdout_text = NULL;
    g_autofree char *stderr_text = NULL;
    g_autoptr(GError) error = NULL;
    int status = 0;

    char *argv[] = {
        (char *)command,
        "-interaction=nonstopmode",
        "-halt-on-error",
        "-file-line-error",
        "-no-shell-escape",
        "-output-directory",
        (char *)output_dir,
        (char *)source_path,
        NULL
    };

    gboolean ok = g_spawn_sync(working_dir, argv, NULL, G_SPAWN_SEARCH_PATH,
                              NULL, NULL, &stdout_text, &stderr_text,
                              &status, &error);
    if (!ok || status != 0) {
        g_autofree char *log = latex_log_message(command, stdout_text, stderr_text);
        app_window_set_error_status(tab->win, "LaTeX render failed",
                                    error ? error->message : log);
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief Open pdf.
 */
static gboolean open_pdf(EditorTab *tab, const char *pdf_path) {
    g_autoptr(GError) error = NULL;
    g_autofree char *uri = g_filename_to_uri(pdf_path, NULL, &error);
    if (!uri) {
        app_window_set_error_status(tab->win, "Could not open rendered PDF",
                                    error ? error->message : "Invalid PDF path");
        return FALSE;
    }

    if (!g_app_info_launch_default_for_uri(uri, NULL, &error)) {
        app_window_set_error_status(tab->win, "Could not open rendered PDF",
                                    error ? error->message : "No default PDF application");
        return FALSE;
    }
    return TRUE;
}

/**
 * @brief Editor tab render latex.
 */
void editor_tab_render_latex(EditorTab *tab) {
    if (!tab || !tab->win) return;
    if (!editor_tab_is_latex(tab)) {
        app_window_set_error_status(tab->win, "Not a LaTeX document",
                                    "Render is available for .tex/.latex files or LaTeX syntax.");
        return;
    }

    g_autofree char *command = find_latex_command();
    if (!command) {
        app_window_set_error_status(tab->win, "LaTeX command not found",
                                    "Install pdflatex, xelatex, or lualatex, or set "
                                    "CLEAF_LATEX_COMMAND to the command path.");
        return;
    }

    g_autofree char *source_path = NULL;
    if (!ensure_saved_source(tab, &source_path)) {
        return;
    }

    g_autofree char *output_dir = build_output_dir_for_source(source_path);
    if (!output_dir) {
        app_window_set_error_status(tab->win, "Could not create build dir",
                                    "Cleaf could not create .cleaf-latex-build.");
        return;
    }

    g_autofree char *working_dir = g_path_get_dirname(source_path);
    if (!run_latex(tab, command, source_path, working_dir, output_dir)) {
        return;
    }

    g_autofree char *stem = basename_without_suffix(source_path);
    g_autofree char *pdf_name = g_strdup_printf("%s.pdf", stem);
    g_autofree char *pdf_path = g_build_filename(output_dir, pdf_name, NULL);
    if (g_file_test(pdf_path, G_FILE_TEST_IS_REGULAR)) {
        if (open_pdf(tab, pdf_path)) {
            app_window_set_status(tab->win, "LaTeX rendered successfully.");
        }
    } else {
        app_window_set_error_status(tab->win, "Rendered PDF not found",
                                    "LaTeX finished, but Cleaf could not find the PDF output.");
    }
}
