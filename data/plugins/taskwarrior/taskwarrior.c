/**
 * @file taskwarrior.c
 * @brief Native Taskwarrior hub plugin for Graptoς.
 * @details The plugin assumes users may or may not have taskwarrior installed.
 *          Missing tools and missing project config render as hub states rather
 *          than plugin load failures.
 */

#include "plugin_api.h"

#include <glib/gstdio.h>
#include <string.h>

#define TASK_CONFIG_FILE ".graptos-task.ini"

typedef struct {
    char *root; /**< Project root used for config and cwd. */
    char *filter; /**< Taskwarrior filter from config. */
    char *project; /**< Taskwarrior project for new tasks. */
} TaskProject;

gboolean graptos_plugin_register(GraptosPluginHost *host);

static void task_project_free(TaskProject *project) {
    if (!project) return;
    g_free(project->root);
    g_free(project->filter);
    g_free(project->project);
    g_free(project);
}

static char *context_root(GraptosPluginCommandContext *context) {
    char *root = graptos_plugin_context_project_root(context);
    if (root && root[0]) return root;
    g_free(root);
    char *path = graptos_plugin_context_file_path(context);
    if (!path || !path[0]) {
        g_free(path);
        return NULL;
    }
    char *dir = g_path_get_dirname(path);
    g_free(path);
    return dir;
}

static char *default_project_for_root(const char *root) {
    if (!root || !root[0]) return g_strdup("default");
    char *base = g_path_get_basename(root);
    if (!base || !base[0] || g_strcmp0(base, ".") == 0 ||
        g_strcmp0(base, G_DIR_SEPARATOR_S) == 0) {
        g_free(base);
        return g_strdup("default");
    }
    return base;
}

static TaskProject *task_project_load(GraptosPluginCommandContext *context,
                                      char **message_out) {
    if (message_out) *message_out = NULL;
    char *root = context_root(context);
    if (!root || !root[0]) {
        if (message_out) {
            *message_out = g_strdup("Open a project folder or save the active file first.");
        }
        g_free(root);
        return NULL;
    }

    TaskProject *project = g_new0(TaskProject, 1);
    project->root = root;
    project->project = default_project_for_root(root);

    g_autofree char *path = g_build_filename(root, TASK_CONFIG_FILE, NULL);
    if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
        g_autoptr(GKeyFile) key_file = g_key_file_new();
        if (!g_key_file_load_from_file(key_file, path, G_KEY_FILE_NONE, NULL)) {
            if (message_out) *message_out = g_strdup("Could not read " TASK_CONFIG_FILE ".");
            task_project_free(project);
            return NULL;
        }

        g_autofree char *configured_project =
            g_key_file_get_string(key_file, "Taskwarrior", "project", NULL);
        if (configured_project && configured_project[0]) {
            g_free(project->project);
            project->project = g_steal_pointer(&configured_project);
        }
        project->filter = g_key_file_get_string(key_file, "Taskwarrior", "filter", NULL);
    }

    if (!project->filter || !project->filter[0]) {
        g_free(project->filter);
        project->filter = g_strdup_printf("project:%s", project->project);
    }
    return project;
}

static gboolean task_available(void) {
    g_autofree char *path = g_find_program_in_path("task");
    return path != NULL;
}

static GPtrArray *argv_new(void) {
    return g_ptr_array_new_with_free_func(g_free);
}

static void argv_add(GPtrArray *argv, const char *text) {
    if (argv && text && text[0]) g_ptr_array_add(argv, g_strdup(text));
}

static void argv_add_shell_words(GPtrArray *argv, const char *text) {
    if (!argv || !text || !text[0]) return;
    gint argc = 0;
    g_auto(GStrv) words = NULL;
    if (!g_shell_parse_argv(text, &argc, &words, NULL)) {
        argv_add(argv, text);
        return;
    }
    for (gint i = 0; i < argc; i++) argv_add(argv, words[i]);
}

static char **argv_finish(GPtrArray *argv) {
    if (!argv) return NULL;
    g_ptr_array_add(argv, NULL);
    return (char **)g_ptr_array_free(argv, FALSE);
}

static gboolean run_task(TaskProject *project,
                         GPtrArray *argv,
                         char **stdout_text,
                         char **stderr_text,
                         gint *status) {
    g_autoptr(GError) error = NULL;
    char **args = argv_finish(argv);
    gboolean t_done = g_spawn_sync(project && project->root ? project->root : NULL,
                               args,
                               NULL,
                               G_SPAWN_SEARCH_PATH,
                               NULL,
                               NULL,
                               stdout_text,
                               stderr_text,
                               status,
                               &error);
    g_strfreev(args);
    if (!t_done && error) {
        g_free(*stderr_text);
        *stderr_text = g_strdup(error->message);
    }
    return t_done;
}

static char *format_task_result(const char *title,
                                gint status,
                                const char *stdout_text,
                                const char *stderr_text) {
    return g_strdup_printf("%s\nExit status: %d\n\n%s%s%s%s",
                           title ? title : "Taskwarrior",
                           status,
                           stdout_text && stdout_text[0] ? stdout_text : "",
                           stdout_text && stdout_text[0] ? "\n" : "",
                           stderr_text && stderr_text[0] ? stderr_text : "",
                           stderr_text && stderr_text[0] ? "\n" : "");
}

static char *sanitize_task_text(const char *text) {
    if (!text) return g_strdup("");
    GString *out = g_string_new(NULL);
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == 0x1b) {
            while (*p && ((*p < 'A' || *p > 'Z') && (*p < 'a' || *p > 'z'))) {
                p++;
            }
            if (!*p) break;
            continue;
        }
        if (*p == '\n' || *p == '\t') {
            g_string_append_c(out, (char)*p);
        } else if (*p == '\r' || *p == '\b' || *p < 0x20) {
            continue;
        } else {
            g_string_append_c(out, (char)*p);
        }
    }
    return g_string_free(out, FALSE);
}

static char *task_hub_render(GraptosPluginCommandContext *context,
                             gpointer user_data) {
    (void)user_data;
    if (!task_available()) {
        return g_strdup("Taskwarrior is not installed.\n\nInstall the `task` command to use this hub.");
    }
    g_autofree char *message = NULL;
    TaskProject *project = task_project_load(context, &message);
    if (!project) return g_steal_pointer(&message);

    GPtrArray *argv = argv_new();
    argv_add(argv, "task");
    argv_add(argv, "rc.verbose=nothing");
    argv_add(argv, "rc.color=off");
    argv_add_shell_words(argv, project->filter);
    argv_add(argv, "list");

    g_autofree char *stdout_text = NULL;
    g_autofree char *stderr_text = NULL;
    gint status = 0;
    (void)run_task(project, argv, &stdout_text, &stderr_text, &status);
    g_autofree char *clean_stdout = sanitize_task_text(stdout_text);
    g_autofree char *clean_stderr = sanitize_task_text(stderr_text);
    GString *out = g_string_new(NULL);
    g_string_append_printf(out,
                           "Project root: %s\nFilter: %s\nProject: %s\n\n",
                           project->root,
                           project->filter,
                           project->project && project->project[0]
                               ? project->project
                               : "(not configured)");
    if (status == 0) {
        g_string_append(out,
                        clean_stdout && clean_stdout[0]
                            ? clean_stdout
                            : "No matching tasks.");
    } else {
        g_autofree char *body = format_task_result("Task list failed",
                                                   status,
                                                   clean_stdout,
                                                   clean_stderr);
        g_string_append(out, body);
    }
    task_project_free(project);
    return g_string_free(out, FALSE);
}

static gboolean input_first_word(const char *input,
                                 char **first_out,
                                 char **rest_out) {
    if (first_out) *first_out = NULL;
    if (rest_out) *rest_out = NULL;
    g_auto(GStrv) words = NULL;
    gint argc = 0;
    if (!g_shell_parse_argv(input ? input : "", &argc, &words, NULL) || argc == 0) {
        return FALSE;
    }
    if (first_out) *first_out = g_strdup(words[0]);
    if (rest_out) {
        GString *rest = g_string_new(NULL);
        for (gint i = 1; i < argc; i++) {
            if (rest->len > 0u) g_string_append_c(rest, ' ');
            g_string_append(rest, words[i]);
        }
        *rest_out = g_string_free(rest, FALSE);
    }
    return TRUE;
}

static void show_action_result(GraptosPluginCommandContext *context,
                               const char *heading,
                               gint status,
                               const char *stdout_text,
                               const char *stderr_text) {
    if (status == 0) {
        graptos_plugin_context_set_status(context, "Taskwarrior updated.");
        return;
    }
    g_autofree char *body = format_task_result(heading, status, stdout_text, stderr_text);
    graptos_plugin_context_show_output(context, "Taskwarrior", heading, body);
}

static void task_hub_action(GraptosPluginCommandContext *context,
                            const char *action_id,
                            const char *input,
                            gpointer user_data) {
    (void)user_data;
    if (g_strcmp0(action_id, "refresh") == 0) return;
    if (!task_available()) {
        graptos_plugin_context_set_status(context, "Taskwarrior is not installed.");
        return;
    }
    g_autofree char *message = NULL;
    TaskProject *project = task_project_load(context, &message);
    if (!project) {
        graptos_plugin_context_show_output(context,
                                           "Taskwarrior",
                                           "Project Task Config Missing",
                                           message ? message : "Missing config.");
        return;
    }

    GPtrArray *argv = argv_new();
    argv_add(argv, "task");
    argv_add(argv, "rc.confirmation=no");
    argv_add(argv, "rc.color=off");
    const char *heading = "Taskwarrior";
    if (g_strcmp0(action_id, "add") == 0) {
        if (!project->project || !project->project[0]) {
            graptos_plugin_context_show_output(context,
                                               "Taskwarrior",
                                               "Project Missing",
                                               TASK_CONFIG_FILE " needs [Taskwarrior] project=... before adding tasks.");
            g_ptr_array_free(argv, TRUE);
            task_project_free(project);
            return;
        }
        argv_add(argv, "add");
        g_autofree char *project_arg = g_strdup_printf("project:%s", project->project);
        argv_add(argv, project_arg);
        argv_add(argv, input);
        heading = "Task Add";
    } else if (g_strcmp0(action_id, "modify") == 0) {
        g_autofree char *id = NULL;
        g_autofree char *rest = NULL;
        if (!input_first_word(input, &id, &rest) || !rest || !rest[0]) {
            graptos_plugin_context_set_status(context, "Use: <id> <modifications>");
            g_ptr_array_free(argv, TRUE);
            task_project_free(project);
            return;
        }
        argv_add(argv, id);
        argv_add(argv, "modify");
        argv_add_shell_words(argv, rest);
        heading = "Task Modify";
    } else if (g_strcmp0(action_id, "priority") == 0) {
        g_autofree char *id = NULL;
        g_autofree char *priority = NULL;
        if (!input_first_word(input, &id, &priority)) {
            graptos_plugin_context_set_status(context, "Use: <id> [H|M|L]");
            g_ptr_array_free(argv, TRUE);
            task_project_free(project);
            return;
        }
        argv_add(argv, id);
        argv_add(argv, "modify");
        g_autofree char *priority_arg = priority && priority[0]
            ? g_strdup_printf("priority:%s", priority)
            : g_strdup("priority:");
        argv_add(argv, priority_arg);
        heading = "Task Priority";
    } else if (g_strcmp0(action_id, "done") == 0) {
        argv_add(argv, input);
        argv_add(argv, "done");
        heading = "Task Done";
    } else if (g_strcmp0(action_id, "delete") == 0) {
        argv_add(argv, input);
        argv_add(argv, "delete");
        heading = "Task Delete";
    } else {
        g_ptr_array_free(argv, TRUE);
        task_project_free(project);
        return;
    }

    g_autofree char *stdout_text = NULL;
    g_autofree char *stderr_text = NULL;
    gint status = 0;
    (void)run_task(project, argv, &stdout_text, &stderr_text, &status);
    g_autofree char *clean_stdout = sanitize_task_text(stdout_text);
    g_autofree char *clean_stderr = sanitize_task_text(stderr_text);
    show_action_result(context, heading, status, clean_stdout, clean_stderr);
    task_project_free(project);
}

gboolean graptos_plugin_register(GraptosPluginHost *host) {
    if (graptos_plugin_host_api_version(host) != GRAPTOS_PLUGIN_API_VERSION) {
        return FALSE;
    }
    return graptos_plugin_host_register_hub(host,
                                           "taskwarrior-hub",
                                           "Taskwarrior",
                                           task_hub_render,
                                           task_hub_action,
                                           NULL,
                                           NULL);
}
