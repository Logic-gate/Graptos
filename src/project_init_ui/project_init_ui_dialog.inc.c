static GtkWidget *project_init_entry_row(const char *label_text, GtkWidget **entry_out) {
    project_init_debug("entry row create label=%s", label_text ? label_text : "(null)");
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *label = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_box_append(GTK_BOX(row), label);
    gtk_box_append(GTK_BOX(row), entry);
    if (entry_out) *entry_out = entry;
    return row;
}

/**
 * @brief Build the project details page.
 * @details The details page gathers only project-wide choices. Template-specific
 *          variables are rebuilt on the next page from the selected manifest.
 * @param dialog The project-init dialog.
 * @return The page widget.
 */
static GtkWidget *project_init_details_page(ProjectInitDialog *dialog) {
    project_init_debug("details page start dialog=%p templates=%u",
                       dialog,
                       dialog && dialog->templates ? dialog->templates->len : 0u);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    graptos_set_all_margins(box, 12);
    gtk_box_append(GTK_BOX(box), project_init_entry_row("Project name", &dialog->project_entry));
    gtk_editable_set_text(GTK_EDITABLE(dialog->project_entry), "Sample App");
    g_signal_connect(dialog->project_entry, "changed",
                     G_CALLBACK(project_init_option_changed), dialog);

    GtkWidget *dest_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *dest_label = gtk_label_new("Destination parent");
    gtk_label_set_xalign(GTK_LABEL(dest_label), 0.0f);
    gtk_widget_set_hexpand(dest_label, TRUE);
    dialog->destination_entry = gtk_entry_new();
    gtk_widget_set_hexpand(dialog->destination_entry, TRUE);
    gtk_editable_set_text(GTK_EDITABLE(dialog->destination_entry), g_get_home_dir());
    GtkWidget *browse = graptos_flat_button_new("Browse", "Choose destination parent",
                                                G_CALLBACK(project_init_choose_destination),
                                                dialog);
    gtk_box_append(GTK_BOX(dest_row), dest_label);
    gtk_box_append(GTK_BOX(dest_row), dialog->destination_entry);
    gtk_box_append(GTK_BOX(dest_row), browse);
    gtk_box_append(GTK_BOX(box), dest_row);
    g_signal_connect(dialog->destination_entry, "changed",
                     G_CALLBACK(project_init_option_changed), dialog);

    dialog->languages = gtk_string_list_new(NULL);
    gtk_string_list_append(dialog->languages, "All");
    project_init_debug("language model created model=%p", dialog->languages);
    for (guint i = 0u; dialog->templates && i < dialog->templates->len; i++) {
        GraptosProjectTemplate *template_def = g_ptr_array_index(dialog->templates, i);
        project_init_add_language(dialog, template_def->language);
    }
    GtkWidget *lang_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(lang_row), gtk_label_new("Language"));
    dialog->language_drop = gtk_drop_down_new(NULL, NULL);
    project_init_drop_down_take_model(dialog->language_drop, dialog->languages);
    gtk_widget_set_hexpand(dialog->language_drop, TRUE);
    gtk_box_append(GTK_BOX(lang_row), dialog->language_drop);
    gtk_box_append(GTK_BOX(box), lang_row);

    GtkWidget *template_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_append(GTK_BOX(template_row), gtk_label_new("Template"));
    dialog->template_drop = gtk_drop_down_new(NULL, NULL);
    gtk_widget_set_hexpand(dialog->template_drop, TRUE);
    gtk_box_append(GTK_BOX(template_row), dialog->template_drop);
    gtk_box_append(GTK_BOX(box), template_row);

    dialog->template_description = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(dialog->template_description), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(dialog->template_description), TRUE);
    gtk_widget_add_css_class(dialog->template_description, "graptos-dialog-muted");
    gtk_box_append(GTK_BOX(box), dialog->template_description);

    dialog->language_changed_id =
        g_signal_connect(dialog->language_drop, "notify::selected",
                         G_CALLBACK(project_init_language_changed), dialog);
    dialog->template_changed_id =
        g_signal_connect(dialog->template_drop, "notify::selected",
                         G_CALLBACK(project_init_template_changed), dialog);
    project_init_debug("details page complete language_drop=%p template_drop=%p",
                       dialog->language_drop, dialog->template_drop);
    return box;
}

/**
 * @brief Build the options page.
 * @details Controls are generated later from the selected template so the page
 *          can be reused for every language and template.
 * @param dialog The project-init dialog.
 * @return The page widget.
 */
static GtkWidget *project_init_options_page(ProjectInitDialog *dialog) {
    project_init_debug("options page create dialog=%p", dialog);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    graptos_set_all_margins(box, 12);
    dialog->options_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_append(GTK_BOX(box), dialog->options_box);
    return box;
}

/**
 * @brief Build the preview page.
 * @details The preview uses the same text representation as the core plan
 *          helper, which keeps tests, display, and apply data aligned.
 * @param dialog The project-init dialog.
 * @return The page widget.
 */
static GtkWidget *project_init_preview_page(ProjectInitDialog *dialog) {
    project_init_debug("preview page create dialog=%p", dialog);
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    graptos_set_all_margins(box, 12);
    dialog->preview = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(dialog->preview), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(dialog->preview), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(dialog->preview), GTK_WRAP_NONE);
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), dialog->preview);
    gtk_box_append(GTK_BOX(box), scrolled);
    return box;
}

/**
 * @brief Open the Init Project dialog.
 * @details The action is kept in a normal source file rather than app_private
 *          internals so the project-init subsystem remains testable without the
 *          full application composition unit.
 * @param widget The widget that emitted the action.
 * @param user_data The EditorWindow.
 */
void action_init_project(GtkWidget *widget, gpointer user_data) {
    EditorWindow *win = user_data;
    project_init_debug("action start widget=%p win=%p", widget, win);
    if (!win) return;

    ProjectInitDialog *dialog = g_new0(ProjectInitDialog, 1);
    project_init_debug("dialog allocated dialog=%p", dialog);
    dialog->building = TRUE;
    dialog->win = win;
    dialog->controls = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    dialog->templates = graptos_project_init_discover_templates(NULL);
    project_init_debug("templates discovered dialog=%p templates=%p count=%u",
                       dialog,
                       dialog->templates,
                       dialog->templates ? dialog->templates->len : 0u);
    dialog->window = gtk_window_new();
    project_init_debug("window created dialog=%p window=%p", dialog, dialog->window);
    g_object_set_data_full(G_OBJECT(dialog->window), "graptos-project-init-dialog",
                           dialog, project_init_dialog_free);
    gtk_widget_add_css_class(dialog->window, "graptos-window");
    gtk_widget_add_css_class(dialog->window, "graptos-dialog");
    gtk_window_set_title(GTK_WINDOW(dialog->window), "Init Project");
    gtk_window_set_default_size(GTK_WINDOW(dialog->window), 760, 560);
    gtk_window_set_modal(GTK_WINDOW(dialog->window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog->window), app_window_gtk(win));
    dialog->close_request_id =
        g_signal_connect(dialog->window, "close-request",
                         G_CALLBACK(project_init_close_request), dialog);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    project_init_debug("root created root=%p", root);
    gtk_widget_add_css_class(root, "graptos-root");
    gtk_widget_add_css_class(root, "graptos-dialog-root");
    gtk_widget_add_css_class(root, "graptos-flat-dialog");
    graptos_set_all_margins(root, 14);
    gtk_window_set_child(GTK_WINDOW(dialog->window), root);

    GtkWidget *title = gtk_label_new("Init Project");
    gtk_widget_add_css_class(title, "graptos-menu-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_box_append(GTK_BOX(root), title);

    dialog->stack = gtk_stack_new();
    project_init_debug("stack created stack=%p", dialog->stack);
    gtk_widget_set_vexpand(dialog->stack, TRUE);
    gtk_stack_add_named(GTK_STACK(dialog->stack),
                        project_init_details_page(dialog), "details");
    gtk_stack_add_named(GTK_STACK(dialog->stack),
                        project_init_options_page(dialog), "options");
    gtk_stack_add_named(GTK_STACK(dialog->stack),
                        project_init_preview_page(dialog), "preview");
    gtk_box_append(GTK_BOX(root), dialog->stack);

    dialog->status = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(dialog->status), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(dialog->status), TRUE);
    gtk_box_append(GTK_BOX(root), dialog->status);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(buttons, GTK_ALIGN_END);
    GtkWidget *cancel = graptos_flat_button_new("Cancel", NULL,
                                                G_CALLBACK(project_init_cancel),
                                                dialog);
    dialog->back_button = graptos_flat_button_new("Back", NULL,
                                                  G_CALLBACK(project_init_back),
                                                  dialog);
    dialog->next_button = graptos_flat_button_new("Next", NULL,
                                                  G_CALLBACK(project_init_next),
                                                  dialog);
    dialog->create_button = graptos_flat_button_new("Create", NULL,
                                                    G_CALLBACK(project_init_create),
                                                    dialog);
    gtk_box_append(GTK_BOX(buttons), cancel);
    gtk_box_append(GTK_BOX(buttons), dialog->back_button);
    gtk_box_append(GTK_BOX(buttons), dialog->next_button);
    gtk_box_append(GTK_BOX(buttons), dialog->create_button);
    gtk_box_append(GTK_BOX(root), buttons);

    if (!dialog->templates || dialog->templates->len == 0u) {
        gtk_label_set_text(GTK_LABEL(dialog->status), "No project templates were found.");
        gtk_widget_add_css_class(dialog->status, "graptos-status-error");
        project_init_debug("no templates found");
    }
    project_init_debug("initial rebuild start dialog=%p", dialog);
    dialog->building = FALSE;
    project_init_rebuild_template_filter(dialog);
    project_init_set_page(dialog, 0u);
    project_init_debug("present dialog=%p window=%p", dialog, dialog->window);
    gtk_window_present(GTK_WINDOW(dialog->window));
    project_init_debug("action complete dialog=%p window=%p", dialog, dialog->window);
}
