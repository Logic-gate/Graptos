/**
 * @brief Return the selected template.
 * @details Template drop-down indexes are relative to the current language
 *          filter, not the full discovered template list.
 * @param dialog The project-init dialog.
 * @return The selected template, or NULL when no template is available.
 */
static GraptosProjectTemplate *project_init_selected_template(ProjectInitDialog *dialog) {
    if (!dialog || !dialog->filtered_templates) {
        project_init_debug("selected-template missing dialog=%p filtered=%p",
                           dialog, dialog ? dialog->filtered_templates : NULL);
        return NULL;
    }
    guint index = gtk_drop_down_get_selected(GTK_DROP_DOWN(dialog->template_drop));
    if (index >= dialog->filtered_templates->len) {
        project_init_debug("selected-template out-of-range index=%u filtered=%u drop=%p",
                           index, dialog->filtered_templates->len, dialog->template_drop);
        return NULL;
    }
    GraptosProjectTemplate *template_def = g_ptr_array_index(dialog->filtered_templates, index);
    project_init_debug("selected-template index=%u name=%s ptr=%p",
                       index,
                       template_def && template_def->name ? template_def->name : "(null)",
                       template_def);
    return template_def;
}

/**
 * @brief Get a boolean as a string for the variable map.
 * @details The core renderer works with strings only, so switches are converted
 *          to the manifest's boolean scalar form at the UI boundary.
 * @param active The boolean state.
 * @return The static string value used by the renderer.
 */
static const char *bool_text(gboolean active) {
    return active ? "true" : "false";
}

/**
 * @brief Collect current option widget values.
 * @details The project-init core only sees strings. The dialog converts switch
 *          and drop-down state back to the variable values declared by the
 *          selected template.
 * @param dialog The project-init dialog.
 * @param template_def The selected template.
 * @return A newly allocated string map.
 */
static GHashTable *project_init_collect_variables(ProjectInitDialog *dialog,
                                                  GraptosProjectTemplate *template_def) {
    GHashTable *vars = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    if (!dialog || !template_def || !dialog->controls) {
        project_init_debug("collect variables skipped dialog=%p template=%p controls=%p",
                           dialog, template_def, dialog ? dialog->controls : NULL);
        return vars;
    }
    project_init_debug("collect variables template=%s count=%u controls=%u",
                       template_def->name ? template_def->name : "(null)",
                       template_def->variables ? template_def->variables->len : 0u,
                       g_hash_table_size(dialog->controls));
    for (guint i = 0u; i < template_def->variables->len; i++) {
        GraptosProjectVariable *var = g_ptr_array_index(template_def->variables, i);
        GtkWidget *control = g_hash_table_lookup(dialog->controls, var->name);
        const char *value = var->default_value ? var->default_value : "";
        if (control && GTK_IS_ENTRY(control)) {
            value = gtk_editable_get_text(GTK_EDITABLE(control));
        } else if (control && GTK_IS_SWITCH(control)) {
            value = bool_text(gtk_switch_get_active(GTK_SWITCH(control)));
        } else if (control && GTK_IS_DROP_DOWN(control)) {
            guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(control));
            if (selected < var->values->len) value = g_ptr_array_index(var->values, selected);
        }
        project_init_debug("collect variable name=%s type=%s control=%p value=%s",
                           var->name ? var->name : "(null)",
                           var->type ? var->type : "(null)",
                           control,
                           value ? value : "(null)");
        g_hash_table_replace(vars, g_strdup(var->name), g_strdup(value));
    }
    return vars;
}

/**
 * @brief Update navigation button sensitivity.
 * @details Page state and apply state are separate: while generation is running,
 *          navigation is disabled even if the preview is valid.
 * @param dialog The project-init dialog.
 */
static void project_init_update_buttons(ProjectInitDialog *dialog) {
    if (!dialog) return;
    project_init_debug("update buttons page=%u applying=%d plan=%p",
                       dialog->page, dialog->applying, dialog->plan);
    gtk_widget_set_sensitive(dialog->back_button, dialog->page > 0u && !dialog->applying);
    gtk_widget_set_visible(dialog->next_button, dialog->page < 2u);
    gtk_widget_set_visible(dialog->create_button, dialog->page == 2u);
    gtk_widget_set_sensitive(dialog->next_button, !dialog->applying);
    gtk_widget_set_sensitive(dialog->create_button, dialog->plan && !dialog->applying);
}

/**
 * @brief Set the visible wizard page.
 * @details Page changes also refresh button state so the Create button only
 *          appears on the preview page.
 * @param dialog The project-init dialog.
 * @param page The page index to show.
 */
static void project_init_set_page(ProjectInitDialog *dialog, guint page) {
    if (!dialog) return;
    dialog->page = CLAMP(page, 0u, 2u);
    const char *name = dialog->page == 0u ? "details" :
                       dialog->page == 1u ? "options" : "preview";
    project_init_debug("set page requested=%u actual=%u name=%s stack=%p",
                       page, dialog->page, name, dialog->stack);
    gtk_stack_set_visible_child_name(GTK_STACK(dialog->stack), name);
    project_init_update_buttons(dialog);
}

/**
 * @brief Rebuild the live project preview.
 * @details Validation is intentionally routed through plan creation. The Create
 *          button is enabled only when the same resolved plan the apply engine
 *          will use can be built successfully.
 * @param dialog The project-init dialog.
 */
static void project_init_rebuild_plan(ProjectInitDialog *dialog) {
    if (!project_init_dialog_accepts_callbacks(dialog)) {
        project_init_debug("rebuild plan ignored dialog=%p building=%d rebuilding=%d closing=%d",
                           dialog,
                           dialog ? dialog->building : FALSE,
                           dialog ? dialog->rebuilding : FALSE,
                           dialog ? dialog->closing : FALSE);
        return;
    }
    project_init_debug("rebuild plan start dialog=%p old_plan=%p", dialog, dialog->plan);
    graptos_project_plan_free(dialog->plan);
    dialog->plan = NULL;
    GraptosProjectTemplate *template_def = project_init_selected_template(dialog);
    const char *project = gtk_editable_get_text(GTK_EDITABLE(dialog->project_entry));
    const char *dest = gtk_editable_get_text(GTK_EDITABLE(dialog->destination_entry));
    project_init_debug("rebuild plan input template=%s project=%s dest=%s",
                       template_def && template_def->name ? template_def->name : "(null)",
                       project ? project : "(null)",
                       dest ? dest : "(null)");
    g_autoptr(GHashTable) vars = project_init_collect_variables(dialog, template_def);
    g_autoptr(GError) error = NULL;
    dialog->plan = graptos_project_plan_build(template_def, project, dest, vars, &error);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(dialog->preview));
    if (dialog->plan) {
        g_autofree char *preview = graptos_project_plan_preview_text(dialog->plan);
        gtk_text_buffer_set_text(buffer, preview, -1);
        gtk_label_set_text(GTK_LABEL(dialog->status), "Ready to create.");
        gtk_widget_remove_css_class(dialog->status, "graptos-status-error");
        project_init_debug("rebuild plan success destination=%s dirs=%u files=%u actions=%u",
                           dialog->plan->destination ? dialog->plan->destination : "(null)",
                           dialog->plan->directories ? dialog->plan->directories->len : 0u,
                           dialog->plan->files ? dialog->plan->files->len : 0u,
                           dialog->plan->actions ? dialog->plan->actions->len : 0u);
    } else {
        gtk_text_buffer_set_text(buffer, "", -1);
        gtk_label_set_text(GTK_LABEL(dialog->status),
                           error ? error->message : "Project details are incomplete.");
        gtk_widget_add_css_class(dialog->status, "graptos-status-error");
        project_init_debug("rebuild plan failed error=%s",
                           error ? error->message : "(none)");
    }
    project_init_update_buttons(dialog);
}

/**
 * @brief Handle an option control changing.
 * @details Any input change can affect conditions, file paths, actions, or file
 *          content, so the dialog rebuilds the plan rather than patching it.
 * @param widget The changed widget.
 * @param user_data The ProjectInitDialog.
 */
static void project_init_option_changed(GtkWidget *widget, gpointer user_data) {
    project_init_debug("option changed widget=%p dialog=%p", widget, user_data);
    if (!project_init_dialog_accepts_callbacks(user_data)) return;
    project_init_rebuild_plan(user_data);
}

/**
 * @brief Clear generated option controls.
 * @details Template changes can replace every option type, so old widgets are
 *          removed and the name-to-control map is reset together.
 * @param dialog The project-init dialog.
 */
static void project_init_clear_options(ProjectInitDialog *dialog) {
    if (!dialog) return;
    project_init_disconnect_option_controls(dialog);
    guint removed = 0u;
    while (gtk_widget_get_first_child(dialog->options_box)) {
        gtk_box_remove(GTK_BOX(dialog->options_box),
                       gtk_widget_get_first_child(dialog->options_box));
        removed++;
    }
    if (dialog->controls) g_hash_table_remove_all(dialog->controls);
    project_init_debug("clear options removed_rows=%u controls_cleared=%d",
                       removed, dialog->controls != NULL);
}

/**
 * @brief Refresh the visible selected-template description.
 * @details The parser already owns the manifest description. The dialog keeps
 *          this as a plain label so template authors can explain the generated
 *          shape before the user reaches the options page.
 * @param dialog The project-init dialog.
 * @param template_def The selected template, or NULL when no template is active.
 */
static void project_init_update_template_description(ProjectInitDialog *dialog,
                                                     GraptosProjectTemplate *template_def) {
    if (!dialog || !dialog->template_description) return;
    const char *description = template_def && template_def->description
        ? template_def->description : "";
    gtk_label_set_text(GTK_LABEL(dialog->template_description), description);
    gtk_widget_set_visible(dialog->template_description, description[0] != '\0');
    project_init_debug("template description update template=%s visible=%d",
                       template_def && template_def->name ? template_def->name : "(null)",
                       description[0] != '\0');
}

/**
 * @brief Append a label/control row to the options page.
 * @param dialog The project-init dialog.
 * @param label_text The user-facing option label.
 * @param control The control widget.
 */
static void project_init_append_option(ProjectInitDialog *dialog,
                                       const char *label_text,
                                       GtkWidget *control) {
    project_init_debug("append option label=%s control=%p type=%s",
                       label_text ? label_text : "(null)",
                       control,
                       control ? G_OBJECT_TYPE_NAME(control) : "(null)");
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *label = gtk_label_new(label_text ? label_text : "Option");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), control);
    gtk_box_append(GTK_BOX(dialog->options_box), row);
}

/**
 * @brief Rebuild option controls for the selected template.
 * @details Template variables are dynamic, so controls are recreated whenever
 *          the selected template changes instead of trying to reuse incompatible
 *          widget types.
 * @param dialog The project-init dialog.
 */
static void project_init_rebuild_options(ProjectInitDialog *dialog) {
    project_init_debug("rebuild options start dialog=%p", dialog);
    if (!project_init_dialog_accepts_callbacks(dialog)) {
        project_init_debug("rebuild options ignored building=%d rebuilding=%d closing=%d",
                           dialog ? dialog->building : FALSE,
                           dialog ? dialog->rebuilding : FALSE,
                           dialog ? dialog->closing : FALSE);
        return;
    }
    project_init_clear_options(dialog);
    GraptosProjectTemplate *template_def = project_init_selected_template(dialog);
    if (!template_def) {
        project_init_update_template_description(dialog, NULL);
        project_init_debug("rebuild options stop: no selected template");
        return;
    }
    project_init_update_template_description(dialog, template_def);
    project_init_debug("rebuild options template=%s variables=%u",
                       template_def->name ? template_def->name : "(null)",
                       template_def->variables ? template_def->variables->len : 0u);
    if (template_def->variables->len == 0u) {
        gtk_box_append(GTK_BOX(dialog->options_box),
                       gtk_label_new("This template has no options."));
        project_init_debug("rebuild options no variables label added");
    }
    for (guint i = 0u; i < template_def->variables->len; i++) {
        GraptosProjectVariable *var = g_ptr_array_index(template_def->variables, i);
        GtkWidget *control = NULL;
        project_init_debug("rebuild option index=%u name=%s type=%s default=%s values=%u",
                           i,
                           var && var->name ? var->name : "(null)",
                           var && var->type ? var->type : "(null)",
                           var && var->default_value ? var->default_value : "(null)",
                           var && var->values ? var->values->len : 0u);
        if (g_strcmp0(var->type, "boolean") == 0) {
            control = gtk_switch_new();
            gtk_switch_set_active(GTK_SWITCH(control),
                                  g_ascii_strcasecmp(var->default_value, "true") == 0);
            g_signal_connect(control, "notify::active",
                             G_CALLBACK(project_init_option_changed), dialog);
        } else if (g_strcmp0(var->type, "choice") == 0) {
            GtkStringList *model = gtk_string_list_new(NULL);
            guint selected = 0u;
            for (guint j = 0u; j < var->values->len; j++) {
                const char *value = g_ptr_array_index(var->values, j);
                gtk_string_list_append(model, value);
                if (g_strcmp0(value, var->default_value) == 0) selected = j;
                project_init_debug("choice value var=%s index=%u value=%s selected=%u",
                                   var->name ? var->name : "(null)", j,
                                   value ? value : "(null)", selected);
            }
            control = gtk_drop_down_new(NULL, NULL);
            project_init_drop_down_take_model(control, model);
            gtk_drop_down_set_selected(GTK_DROP_DOWN(control), selected);
            g_signal_connect(control, "notify::selected",
                             G_CALLBACK(project_init_option_changed), dialog);
        } else {
            control = gtk_entry_new();
            gtk_editable_set_text(GTK_EDITABLE(control), var->default_value ? var->default_value : "");
            g_signal_connect(control, "changed",
                             G_CALLBACK(project_init_option_changed), dialog);
        }
        g_hash_table_insert(dialog->controls, g_strdup(var->name), control);
        project_init_append_option(dialog, var->label ? var->label : var->name, control);
    }
    project_init_debug("rebuild options complete controls=%u",
                       dialog->controls ? g_hash_table_size(dialog->controls) : 0u);
    project_init_rebuild_plan(dialog);
}

/**
 * @brief Add a language to the language model if missing.
 * @details Languages are derived from loaded templates at runtime, which lets
 *          user templates appear without hard-coded language lists.
 * @param dialog The project-init dialog.
 * @param language The language to add.
 */
static void project_init_add_language(ProjectInitDialog *dialog, const char *language) {
    if (!dialog || !language) return;
    guint n = g_list_model_get_n_items(G_LIST_MODEL(dialog->languages));
    for (guint i = 0u; i < n; i++) {
        g_autoptr(GtkStringObject) item =
            GTK_STRING_OBJECT(g_list_model_get_item(G_LIST_MODEL(dialog->languages), i));
        if (g_strcmp0(gtk_string_object_get_string(item), language) == 0) return;
    }
    gtk_string_list_append(dialog->languages, language);
    project_init_debug("language added %s count_now=%u", language, n + 1u);
}

/**
 * @brief Rebuild the template list for the selected language.
 * @details The template drop-down uses borrowed template pointers from the full
 *          discovery array, so filtering does not duplicate parsed templates.
 * @param dialog The project-init dialog.
 */
static void project_init_rebuild_template_filter(ProjectInitDialog *dialog) {
    if (!project_init_dialog_accepts_callbacks(dialog)) {
        project_init_debug("template filter ignored dialog=%p building=%d rebuilding=%d closing=%d",
                           dialog,
                           dialog ? dialog->building : FALSE,
                           dialog ? dialog->rebuilding : FALSE,
                           dialog ? dialog->closing : FALSE);
        return;
    }
    project_init_debug("template filter start dialog=%p old_filtered=%u",
                       dialog,
                       dialog->filtered_templates ? dialog->filtered_templates->len : 0u);
    dialog->rebuilding = TRUE;
    if (dialog->filtered_templates) g_ptr_array_free(dialog->filtered_templates, TRUE);
    GtkStringList *model = gtk_string_list_new(NULL);
    dialog->filtered_templates = g_ptr_array_new();
    guint lang_index = gtk_drop_down_get_selected(GTK_DROP_DOWN(dialog->language_drop));
    g_autoptr(GtkStringObject) lang_item =
        GTK_STRING_OBJECT(g_list_model_get_item(G_LIST_MODEL(dialog->languages), lang_index));
    const char *language = lang_item ? gtk_string_object_get_string(lang_item) : NULL;
    project_init_debug("template filter language index=%u value=%s templates_total=%u",
                       lang_index,
                       language ? language : "(null)",
                       dialog->templates ? dialog->templates->len : 0u);
    for (guint i = 0u; dialog->templates && i < dialog->templates->len; i++) {
        GraptosProjectTemplate *template_def = g_ptr_array_index(dialog->templates, i);
        if (language && g_strcmp0(language, "All") != 0 &&
            g_strcmp0(template_def->language, language) != 0) {
            continue;
        }
        gtk_string_list_append(model, template_def->name);
        g_ptr_array_add(dialog->filtered_templates, template_def);
        project_init_debug("template filter add template=%s language=%s ptr=%p",
                           template_def->name ? template_def->name : "(null)",
                           template_def->language ? template_def->language : "(null)",
                           template_def);
    }
    dialog->template_names = model;
    project_init_drop_down_take_model(dialog->template_drop, model);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(dialog->template_drop), 0u);
    project_init_debug("template filter complete filtered=%u",
                       dialog->filtered_templates ? dialog->filtered_templates->len : 0u);
    dialog->rebuilding = FALSE;
    project_init_rebuild_options(dialog);
}

/**
 * @brief Handle language selection changes.
 * @param drop_down The language drop-down.
 * @param pspec The changed property.
 * @param user_data The ProjectInitDialog.
 */
static void project_init_language_changed(GtkDropDown *drop_down,
                                          GParamSpec *pspec,
                                          gpointer user_data) {
    (void)pspec;
    project_init_debug("language changed drop=%p selected=%u dialog=%p",
                       drop_down,
                       drop_down ? gtk_drop_down_get_selected(drop_down) : GTK_INVALID_LIST_POSITION,
                       user_data);
    if (!project_init_dialog_accepts_callbacks(user_data)) return;
    project_init_rebuild_template_filter(user_data);
}

/**
 * @brief Handle template selection changes.
 * @param drop_down The template drop-down.
 * @param pspec The changed property.
 * @param user_data The ProjectInitDialog.
 */
static void project_init_template_changed(GtkDropDown *drop_down,
                                          GParamSpec *pspec,
                                          gpointer user_data) {
    (void)pspec;
    project_init_debug("template changed drop=%p selected=%u dialog=%p",
                       drop_down,
                       drop_down ? gtk_drop_down_get_selected(drop_down) : GTK_INVALID_LIST_POSITION,
                       user_data);
    if (!project_init_dialog_accepts_callbacks(user_data)) return;
    project_init_rebuild_options(user_data);
}
