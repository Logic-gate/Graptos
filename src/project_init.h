/**
 * @file src/project_init.h
 * @brief Declarative project initialization API.
 * @details Project creation is split into template, plan, and apply phases so
 *          Graptoς can preview every filesystem operation before anything is
 *          written. The core layer stays language-agnostic; C, GTK, Sailfish,
 *          and other layouts are data in templates, not branches in app code.
 */

#ifndef GRAPTOS_PROJECT_INIT_H
#define GRAPTOS_PROJECT_INIT_H

#include <gio/gio.h>
#include <glib.h>

/**
 * @brief Project-init error domain macro.
 */
#define GRAPTOS_PROJECT_INIT_ERROR (graptos_project_init_error_quark())

/**
 * @brief Project initialization failure codes.
 * @details The codes keep template validation, rendering, destination checks,
 *          and built-in actions distinguishable while still using normal GError
 *          plumbing everywhere the subsystem crosses into the UI.
 */
typedef enum {
    GRAPTOS_PROJECT_INIT_ERROR_INVALID_SCHEMA,
    GRAPTOS_PROJECT_INIT_ERROR_INVALID_PATH,
    GRAPTOS_PROJECT_INIT_ERROR_UNKNOWN_VARIABLE,
    GRAPTOS_PROJECT_INIT_ERROR_UNKNOWN_ACTION,
    GRAPTOS_PROJECT_INIT_ERROR_DUPLICATE_PATH,
    GRAPTOS_PROJECT_INIT_ERROR_RENDER,
    GRAPTOS_PROJECT_INIT_ERROR_IO,
    GRAPTOS_PROJECT_INIT_ERROR_DESTINATION_EXISTS,
    GRAPTOS_PROJECT_INIT_ERROR_ACTION_FAILED
} GraptosProjectInitError;

/**
 * @brief Template source type.
 * @details Discovery uses source precedence rather than merging templates. A
 *          user template with the same id replaces a built-in template cleanly,
 *          which keeps editing local copies predictable.
 */
typedef enum {
    GRAPTOS_PROJECT_TEMPLATE_BUILTIN,
    GRAPTOS_PROJECT_TEMPLATE_USER,
    GRAPTOS_PROJECT_TEMPLATE_PLUGIN
} GraptosProjectTemplateSource;

/**
 * @brief Template variable declaration.
 * @details Variables are intentionally simple in v1. The renderer only needs
 *          final string values, while the UI still needs type and choice data to
 *          build safe controls.
 */
typedef struct {
    char *name;
    char *label;
    char *type;
    char *default_value;
    GPtrArray *values;
} GraptosProjectVariable;

/**
 * @brief Template directory declaration.
 */
typedef struct {
    char *path;
    char *condition;
} GraptosProjectDirectory;

/**
 * @brief Template file declaration.
 */
typedef struct {
    char *path;
    char *template_path;
    char *source_path;
    char *condition;
    gboolean render;
} GraptosProjectFile;

/**
 * @brief Template action declaration.
 */
typedef struct {
    char *type;
    char *path;
    char *condition;
} GraptosProjectAction;

/**
 * @brief Parsed project template.
 * @details The template holds raw paths, placeholders, and conditions. Callers
 *          must resolve it into a plan before previewing or applying it.
 */
typedef struct {
    guint version;
    GraptosProjectTemplateSource source;
    char *id;
    char *name;
    char *description;
    char *language;
    char *base_path;
    GPtrArray *variables;
    GPtrArray *directories;
    GPtrArray *files;
    GPtrArray *actions;
    char *open_project;
    char *open_file;
} GraptosProjectTemplate;

/**
 * @brief Planned directory creation.
 */
typedef struct {
    char *path;
} GraptosProjectPlannedDirectory;

/**
 * @brief Planned file creation.
 */
typedef struct {
    char *path;
    GBytes *content;
    gboolean executable;
} GraptosProjectPlannedFile;

/**
 * @brief Planned built-in action.
 */
typedef struct {
    char *type;
    char *path;
} GraptosProjectPlannedAction;

/**
 * @brief Resolved project generation plan.
 * @details The plan contains only final paths, rendered bytes, and approved
 *          actions. The apply step never reparses a template, which keeps the
 *          preview and filesystem write behavior identical.
 */
typedef struct {
    char *destination;
    GHashTable *variables;
    GPtrArray *directories;
    GPtrArray *files;
    GPtrArray *actions;
    char *open_project;
    char *open_file;
} GraptosProjectPlan;

/**
 * @brief Return the project-init error quark.
 * @details Callers use the domain to separate template/render/apply failures
 *          from generic GLib IO errors in dialogs and tests.
 * @return The project-init error quark.
 */
GQuark graptos_project_init_error_quark(void);
/**
 * @brief Discover available project templates.
 * @details Built-in templates are loaded before user templates so local copies
 *          can override shipped templates by id without merging fields.
 * @param error Reserved for fatal discovery errors.
 * @return An owned array of GraptosProjectTemplate values.
 */
GPtrArray *graptos_project_init_discover_templates(GError **error);
/**
 * @brief Load one template directory.
 * @details Loading keeps raw placeholders and conditions intact. The caller
 *          still needs to build a resolved plan before preview or apply.
 * @param template_dir The directory containing template.yaml.
 * @param source The source type assigned by discovery.
 * @param error Return location for parser errors.
 * @return A parsed template, or NULL on failure.
 */
GraptosProjectTemplate *graptos_project_template_load(const char *template_dir,
                                                      GraptosProjectTemplateSource source,
                                                      GError **error);
/**
 * @brief Free a parsed project template.
 * @details Template arrays own all nested declarations, so one call releases
 *          parser state after discovery or planning.
 * @param template_def The template to free.
 */
void graptos_project_template_free(GraptosProjectTemplate *template_def);
/**
 * @brief Free a resolved project plan.
 * @details Plans own rendered file bytes and final operations, which may be
 *          rebuilt often while the preview is live.
 * @param plan The plan to free.
 */
void graptos_project_plan_free(GraptosProjectPlan *plan);
/**
 * @brief Evaluate a v1 template condition.
 * @details The accepted grammar is intentionally small and rejects unsupported
 *          expression syntax before it can influence filesystem output.
 * @param condition The condition text, or NULL for unconditional.
 * @param variables The resolved variable map.
 * @param error Return location for condition errors.
 * @return TRUE when the conditioned entry should be included.
 */
gboolean graptos_project_condition_eval(const char *condition,
                                        GHashTable *variables,
                                        GError **error);
/**
 * @brief Render placeholders in a string.
 * @details Unknown placeholders fail hard so a typo in a manifest never creates
 *          a silently broken source file.
 * @param text The template text.
 * @param variables The resolved variable map.
 * @param error Return location for render errors.
 * @return An owned rendered string, or NULL on failure.
 */
char *graptos_project_render_string(const char *text,
                                    GHashTable *variables,
                                    GError **error);
/**
 * @brief Validate and normalize a relative output path.
 * @details Template paths are checked before planning and applying so every
 *          generated path stays below the project root.
 * @param path The path to validate.
 * @param normalized_out Optional output storage for a normalized path.
 * @param error Return location for validation errors.
 * @return TRUE when the path is safe.
 */
gboolean graptos_project_path_is_safe(const char *path,
                                      char **normalized_out,
                                      GError **error);
/**
 * @brief Resolve a template into a project plan.
 * @details The plan is the single source of truth for preview and apply, so the
 *          filesystem writer does not reinterpret template YAML.
 * @param template_def The parsed template.
 * @param project_name The project display name.
 * @param destination_parent The selected destination parent directory.
 * @param user_variables Optional variable overrides from the dialog.
 * @param error Return location for planning errors.
 * @return A resolved plan, or NULL on failure.
 */
GraptosProjectPlan *graptos_project_plan_build(GraptosProjectTemplate *template_def,
                                               const char *project_name,
                                               const char *destination_parent,
                                               GHashTable *user_variables,
                                               GError **error);
/**
 * @brief Render a text preview of a plan.
 * @details The preview text is shared by tests and the GTK dialog so displayed
 *          operations match the data the apply engine receives.
 * @param plan The resolved plan.
 * @return An owned preview string.
 */
char *graptos_project_plan_preview_text(GraptosProjectPlan *plan);
/**
 * @brief Apply a resolved project plan.
 * @details Generation writes into a sibling temporary directory first and only
 *          renames it to the final destination after every operation succeeds.
 * @param plan The resolved plan to apply.
 * @param error Return location for apply errors.
 * @return TRUE when the project was created.
 */
gboolean graptos_project_plan_apply(GraptosProjectPlan *plan, GError **error);

#endif /* GRAPTOS_PROJECT_INIT_H */
