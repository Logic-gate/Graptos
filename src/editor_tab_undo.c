#include "editor_tab_private.h"

void clear_stack(GPtrArray *stack) {
    if (stack) g_ptr_array_set_size(stack, 0);
}


void push_limited(GPtrArray *stack, char *state) {
    if (!stack || !state) return;
    g_ptr_array_add(stack, state);
    while (stack->len > CLEAF_MAX_UNDO_STATES) g_ptr_array_remove_index(stack, 0);
}


char *pop_stack(GPtrArray *stack) {
    if (!stack || stack->len == 0u) return NULL;
    char *state = g_ptr_array_index(stack, stack->len - 1u);
    g_ptr_array_steal_index(stack, stack->len - 1u);
    return state;
}


void reset_undo_state(EditorTab *tab) {
    if (!tab) return;
    clear_stack(tab->undo_stack);
    clear_stack(tab->redo_stack);
    g_free(tab->last_snapshot);
    tab->last_snapshot = NULL;
    if (tab->buffer &&
        (guint)gtk_text_buffer_get_char_count(tab->buffer) <= CLEAF_MAX_UNDO_CAPTURE_BYTES) {
        tab->last_snapshot = buffer_text(tab);
    }
}


void editor_tab_undo(EditorTab *tab) {
    if (!tab) return;
    char *prev = pop_stack(tab->undo_stack);
    if (!prev) return;
    char *current = buffer_text(tab);
    push_limited(tab->redo_stack, current);
    tab->applying_change = TRUE;
    gtk_text_buffer_set_text(tab->buffer, prev, -1);
    tab->applying_change = FALSE;
    g_free(tab->last_snapshot);
    tab->last_snapshot = g_strdup(prev);
    tab->modified = TRUE;
    g_free(prev);
    update_gutter_width(tab);
    editor_tab_schedule_highlight(tab);
    editor_tab_schedule_completion(tab);
    editor_tab_update_title(tab);
    editor_tab_update_status(tab);
}


void editor_tab_redo(EditorTab *tab) {
    if (!tab) return;
    char *next = pop_stack(tab->redo_stack);
    if (!next) return;
    char *current = buffer_text(tab);
    push_limited(tab->undo_stack, current);
    tab->applying_change = TRUE;
    gtk_text_buffer_set_text(tab->buffer, next, -1);
    tab->applying_change = FALSE;
    g_free(tab->last_snapshot);
    tab->last_snapshot = g_strdup(next);
    tab->modified = TRUE;
    g_free(next);
    update_gutter_width(tab);
    editor_tab_schedule_highlight(tab);
    editor_tab_schedule_completion(tab);
    editor_tab_update_title(tab);
    editor_tab_update_status(tab);
}


