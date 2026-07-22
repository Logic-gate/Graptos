/**
 * @brief Project-init dialog state.
 * @details GTK signal callbacks need a single owner for the selected template,
 *          dynamic variable controls, resolved plan, and async apply state.
 */
typedef struct {
    EditorWindow *win; /**< Application window using the dialog. */
    GtkWidget *window; /**< Dialog window. */
    GtkWidget *stack; /**< Three-page stack. */
    GtkWidget *project_entry; /**< Project name entry. */
    GtkWidget *destination_entry; /**< Destination parent entry. */
    GtkWidget *language_drop; /**< Language drop-down. */
    GtkWidget *template_drop; /**< Template drop-down. */
    GtkWidget *template_description; /**< Selected template description label. */
    GtkWidget *options_box; /**< Generated template options. */
    GtkWidget *preview; /**< Text preview widget. */
    GtkWidget *status; /**< Validation status label. */
    GtkWidget *back_button; /**< Back navigation button. */
    GtkWidget *next_button; /**< Next navigation button. */
    GtkWidget *create_button; /**< Create project button. */
    GPtrArray *templates; /**< Discovered GraptosProjectTemplate values. */
    GtkStringList *languages; /**< Borrowed language model currently owned by the drop-down. */
    GtkStringList *template_names; /**< Borrowed template model currently owned by the drop-down. */
    GPtrArray *filtered_templates; /**< Borrowed templates matching language. */
    GHashTable *controls; /**< GtkWidget controls keyed by variable name. */
    GraptosProjectPlan *plan; /**< Latest resolved plan. */
    guint page; /**< Current page index. */
    gulong language_changed_id; /**< Language notify handler id. */
    gulong template_changed_id; /**< Template notify handler id. */
    gulong close_request_id; /**< Dialog close-request handler id. */
    gboolean building; /**< TRUE while the widget tree is incomplete. */
    gboolean rebuilding; /**< TRUE while dropdown models/options are being replaced. */
    gboolean closing; /**< TRUE once the dialog is being torn down. */
    gboolean applying; /**< TRUE while async generation is running. */
} ProjectInitDialog;
