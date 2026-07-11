#include "editor_tab_private.h"

#include <ctype.h>
#include <sys/stat.h>

#define LATEX_BUILD_DIR ".cleaf-latex-build"

static gboolean has_space(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    while (p && *p != '\0') {
        if (g_ascii_isspace(*p)) return TRUE;
        p++;
    }
    return FALSE;
}

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

static char *basename_without_suffix(const char *path) {
    char *base = g_path_get_basename(path ? path : "document.tex");
    char *dot = base ? strrchr(base, '.') : NULL;
    if (dot && dot != base) *dot = '\0';
    if (!base || base[0] == '\0') {
        g_free(base);
        return g_strdup("document");
    }
    return base;
}

static gboolean ensure_saved_source(EditorTab *tab, char **source_path_out) {
    if (!tab || !source_path_out) return FALSE;

    if (tab->file_path) {
        if (tab->modified && !editor_tab_save(tab, FALSE)) return FALSE;
        *source_path_out = g_strdup(tab->file_path);
        return *source_path_out != NULL;
    }

    char *text = buffer_text(tab);
    if (!text) return FALSE;

    GError *error = NULL;
    char *tmp_dir = g_dir_make_tmp("cleaf-latex-XXXXXX", &error);
    if (!tmp_dir) {
        dialog_error(app_window_gtk(tab->win), "Could not create temp dir",
                     error ? error->message : "Unknown error");
        g_clear_error(&error);
        g_free(text);
        return FALSE;
    }

    char *source = g_build_filename(tmp_dir, "document.tex", NULL);
    g_free(tmp_dir);
    if (!write_text_atomic(source, text, &error)) {
        dialog_error(app_window_gtk(tab->win), "Could not write temp TeX file",
                     error ? error->message : "Unknown error");
        g_clear_error(&error);
        g_free(source);
        g_free(text);
        return FALSE;
    }

    g_free(text);
    *source_path_out = source;
    return TRUE;
}

static char *build_output_dir_for_source(const char *source_path) {
    char *dir = g_path_get_dirname(source_path);
    if (!dir) return NULL;

    char *output = g_build_filename(dir, LATEX_BUILD_DIR, NULL);
    g_free(dir);
    if (g_mkdir_with_parents(output, 0700) != 0) {
        g_free(output);
        return NULL;
    }
    return output;
}

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

static gboolean run_latex(EditorTab *tab,
                          const char *command,
                          const char *source_path,
                          const char *working_dir,
                          const char *output_dir) {
    char *stdout_text = NULL;
    char *stderr_text = NULL;
    GError *error = NULL;
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
        char *log = latex_log_message(command, stdout_text, stderr_text);
        dialog_error(app_window_gtk(tab->win), "LaTeX render failed",
                     error ? error->message : log);
        g_free(log);
        g_clear_error(&error);
        g_free(stdout_text);
        g_free(stderr_text);
        return FALSE;
    }

    g_free(stdout_text);
    g_free(stderr_text);
    return TRUE;
}

static void open_pdf(EditorTab *tab, const char *pdf_path) {
    GError *error = NULL;
    char *uri = g_filename_to_uri(pdf_path, NULL, &error);
    if (!uri) {
        dialog_error(app_window_gtk(tab->win), "Could not open rendered PDF",
                     error ? error->message : "Invalid PDF path");
        g_clear_error(&error);
        return;
    }

    gtk_show_uri(app_window_gtk(tab->win), uri, GDK_CURRENT_TIME);
    g_free(uri);
}

void editor_tab_render_latex(EditorTab *tab) {
    if (!tab || !tab->win) return;
    if (!editor_tab_is_latex(tab)) {
        dialog_error(app_window_gtk(tab->win), "Not a LaTeX document",
                     "Render is available for .tex/.latex files or LaTeX syntax.");
        return;
    }

    char *command = find_latex_command();
    if (!command) {
        dialog_error(app_window_gtk(tab->win), "LaTeX command not found",
                     "Install pdflatex, xelatex, or lualatex, or set "
                     "CLEAF_LATEX_COMMAND to the command path.");
        return;
    }

    char *source_path = NULL;
    if (!ensure_saved_source(tab, &source_path)) {
        g_free(command);
        return;
    }

    char *output_dir = build_output_dir_for_source(source_path);
    if (!output_dir) {
        dialog_error(app_window_gtk(tab->win), "Could not create build dir",
                     "Cleaf could not create .cleaf-latex-build.");
        g_free(command);
        g_free(source_path);
        return;
    }

    char *working_dir = g_path_get_dirname(source_path);
    if (!run_latex(tab, command, source_path, working_dir, output_dir)) {
        g_free(command);
        g_free(source_path);
        g_free(working_dir);
        g_free(output_dir);
        return;
    }

    char *stem = basename_without_suffix(source_path);
    char *pdf_name = g_strdup_printf("%s.pdf", stem);
    char *pdf_path = g_build_filename(output_dir, pdf_name, NULL);
    if (g_file_test(pdf_path, G_FILE_TEST_IS_REGULAR)) {
        open_pdf(tab, pdf_path);
        app_window_set_status(tab->win, "LaTeX rendered successfully.");
    } else {
        dialog_error(app_window_gtk(tab->win), "Rendered PDF not found",
                     "LaTeX finished, but Cleaf could not find the PDF output.");
    }

    g_free(pdf_path);
    g_free(pdf_name);
    g_free(stem);
    g_free(command);
    g_free(source_path);
    g_free(working_dir);
    g_free(output_dir);
}
