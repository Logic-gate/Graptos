/**
 * @brief Apply task payload.
 * @details The worker thread owns a deep copy of the latest plan so the dialog
 *          can remain responsive without sharing mutable preview state.
 */
typedef struct {
    GraptosProjectPlan *plan; /**< Plan applied by the worker. */
} ProjectInitApply;

/**
 * @brief Free a copied planned directory.
 * @details Worker plans are copied away from the live preview plan, so copied
 *          entries need local cleanup functions.
 * @param data The directory to free.
 */
static void copied_directory_free(gpointer data) {
    GraptosProjectPlannedDirectory *dir = data;
    if (!dir) return;
    g_free(dir->path);
    g_free(dir);
}

/**
 * @brief Free a copied planned file.
 * @details The copied plan refs rendered GBytes instead of rerendering template
 *          content on the worker thread.
 * @param data The file to free.
 */
static void copied_file_free(gpointer data) {
    GraptosProjectPlannedFile *file = data;
    if (!file) return;
    g_free(file->path);
    if (file->content) g_bytes_unref(file->content);
    g_free(file);
}

/**
 * @brief Free a copied planned action.
 * @details Copied actions keep the async apply path independent from UI-owned
 *          preview state.
 * @param data The action to free.
 */
static void copied_action_free(gpointer data) {
    GraptosProjectPlannedAction *action = data;
    if (!action) return;
    g_free(action->type);
    g_free(action->path);
    g_free(action);
}

/**
 * @brief Free apply task payload.
 * @details The task data owns the worker plan and releases it after the async
 *          apply callback has received the result.
 * @param data The ProjectInitApply to free.
 */
static void project_init_apply_free(gpointer data) {
    ProjectInitApply *apply = data;
    if (!apply) return;
    project_init_debug("apply free apply=%p plan=%p", apply, apply->plan);
    graptos_project_plan_free(apply->plan);
    g_free(apply);
}

/**
 * @brief Copy a resolved plan for a worker thread.
 * @details The UI may rebuild or free its preview plan while generation is in
 *          progress. Copying the plan gives the worker immutable data and avoids
 *          sharing GTK-owned state across threads.
 * @param plan The plan to copy.
 * @return A deep enough copy for apply/open handling.
 */
static GraptosProjectPlan *project_init_plan_copy(GraptosProjectPlan *plan) {
    project_init_debug("plan copy start plan=%p destination=%s dirs=%u files=%u actions=%u",
                       plan,
                       plan && plan->destination ? plan->destination : "(null)",
                       plan && plan->directories ? plan->directories->len : 0u,
                       plan && plan->files ? plan->files->len : 0u,
                       plan && plan->actions ? plan->actions->len : 0u);
    GraptosProjectPlan *copy = g_new0(GraptosProjectPlan, 1);
    copy->destination = g_strdup(plan->destination);
    copy->variables = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    copy->directories = g_ptr_array_new_with_free_func(copied_directory_free);
    copy->files = g_ptr_array_new_with_free_func(copied_file_free);
    copy->actions = g_ptr_array_new_with_free_func(copied_action_free);
    copy->open_project = g_strdup(plan->open_project);
    copy->open_file = g_strdup(plan->open_file);

    GHashTableIter iter;
    gpointer key = NULL;
    gpointer value = NULL;
    g_hash_table_iter_init(&iter, plan->variables);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        g_hash_table_replace(copy->variables, g_strdup(key), g_strdup(value));
    }
    for (guint i = 0u; i < plan->directories->len; i++) {
        GraptosProjectPlannedDirectory *src = g_ptr_array_index(plan->directories, i);
        GraptosProjectPlannedDirectory *dst = g_new0(GraptosProjectPlannedDirectory, 1);
        dst->path = g_strdup(src->path);
        g_ptr_array_add(copy->directories, dst);
    }
    for (guint i = 0u; i < plan->files->len; i++) {
        GraptosProjectPlannedFile *src = g_ptr_array_index(plan->files, i);
        GraptosProjectPlannedFile *dst = g_new0(GraptosProjectPlannedFile, 1);
        dst->path = g_strdup(src->path);
        dst->content = g_bytes_ref(src->content);
        dst->executable = src->executable;
        g_ptr_array_add(copy->files, dst);
    }
    for (guint i = 0u; i < plan->actions->len; i++) {
        GraptosProjectPlannedAction *src = g_ptr_array_index(plan->actions, i);
        GraptosProjectPlannedAction *dst = g_new0(GraptosProjectPlannedAction, 1);
        dst->type = g_strdup(src->type);
        dst->path = g_strdup(src->path);
        g_ptr_array_add(copy->actions, dst);
    }
    project_init_debug("plan copy complete copy=%p", copy);
    return copy;
}

/**
 * @brief Worker thread for project generation.
 * @param task The GTask that will receive the result.
 * @param source_object Unused source object.
 * @param task_data The ProjectInitApply payload.
 * @param cancellable Optional cancellation handle.
 */
static void project_init_apply_thread(GTask *task,
                                      gpointer source_object,
                                      gpointer task_data,
                                      GCancellable *cancellable) {
    (void)source_object;
    (void)cancellable;
    ProjectInitApply *apply = task_data;
    project_init_debug("apply thread start task=%p apply=%p plan=%p",
                       task, apply, apply ? apply->plan : NULL);
    g_autoptr(GError) error = NULL;
    if (graptos_project_plan_apply(apply->plan, &error)) {
        project_init_debug("apply thread success task=%p", task);
        g_task_return_boolean(task, TRUE);
    } else {
        project_init_debug("apply thread failed task=%p error=%s",
                           task, error ? error->message : "(none)");
        g_task_return_error(task, g_steal_pointer(&error));
    }
}

/**
 * @brief Finish asynchronous project generation.
 * @param source The GTask source object.
 * @param result The async result.
 * @param user_data The ProjectInitDialog.
 */
static void project_init_apply_done(GObject *source,
                                    GAsyncResult *result,
                                    gpointer user_data) {
    (void)source;
    ProjectInitDialog *dialog = user_data;
    project_init_debug("apply done start source=%p result=%p dialog=%p",
                       source, result, dialog);
    if (!dialog || dialog->closing) return;
    dialog->applying = FALSE;
    g_autoptr(GError) error = NULL;
    gboolean ok = g_task_propagate_boolean(G_TASK(result), &error);
    if (!ok) {
        project_init_debug("apply done failed dialog=%p error=%s",
                           dialog, error ? error->message : "(none)");
        gtk_label_set_text(GTK_LABEL(dialog->status),
                           error ? error->message : "Could not create project.");
        gtk_widget_add_css_class(dialog->status, "graptos-status-error");
        project_init_update_buttons(dialog);
        return;
    }

    g_autofree char *destination = g_strdup(dialog->plan->destination);
    g_autofree char *open_file = dialog->plan->open_file ?
        g_build_filename(destination, dialog->plan->open_file, NULL) : NULL;
    EditorWindow *win = dialog->win;
    project_init_debug("apply done success destination=%s open_file=%s win=%p",
                       destination ? destination : "(null)",
                       open_file ? open_file : "(null)",
                       win);
    dialog->closing = TRUE;
    project_init_disconnect_dialog_signals(dialog);
    graptos_widget_destroy(dialog->window);
    project_tree_load_folder(win, destination);
    if (win->project_revealer) {
        gtk_widget_set_visible(win->project_revealer, TRUE);
        gtk_revealer_set_reveal_child(GTK_REVEALER(win->project_revealer), TRUE);
    }
    if (open_file && !app_window_open_file(win, open_file)) {
        app_window_set_error_status(win, "Project created",
                                    "The project was created, but the initial file could not be opened.");
        return;
    }
    app_window_set_status(win, "Project created.");
}

/**
 * @brief Start asynchronous project generation.
 * @details The live plan is copied before the worker starts so further GTK
 *          events cannot mutate the data being written to disk.
 * @param widget The button that emitted the signal.
 * @param user_data The ProjectInitDialog.
 */
static void project_init_create(GtkWidget *widget, gpointer user_data) {
    ProjectInitDialog *dialog = user_data;
    project_init_debug("create clicked widget=%p dialog=%p plan=%p applying=%d",
                       widget, dialog, dialog ? dialog->plan : NULL,
                       dialog ? dialog->applying : FALSE);
    if (!project_init_dialog_accepts_callbacks(dialog) ||
        !dialog->plan ||
        dialog->applying) {
        return;
    }
    dialog->applying = TRUE;
    gtk_label_set_text(GTK_LABEL(dialog->status), "Creating project...");
    project_init_update_buttons(dialog);

    ProjectInitApply *apply = g_new0(ProjectInitApply, 1);
    apply->plan = project_init_plan_copy(dialog->plan);
    GTask *task = g_task_new(dialog->window, NULL, project_init_apply_done, dialog);
    project_init_debug("create task start task=%p apply=%p source_window=%p",
                       task, apply, dialog->window);
    g_task_set_task_data(task, apply, project_init_apply_free);
    g_task_run_in_thread(task, project_init_apply_thread);
    g_object_unref(task);
}

/**
 * @brief Create a labelled entry row.
 * @details Several dialog pages use the same label/entry layout; sharing the
 *          row keeps spacing and expansion consistent.
 * @param label_text The label text.
 * @param entry_out Output storage for the created entry.
 * @return The row widget.
 */
