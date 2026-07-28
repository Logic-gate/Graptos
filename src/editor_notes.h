/**
 * @file src/editor_notes.h
 * @brief Text-backed editor note storage.
 * @details Notes are project state, not editor-buffer text. This module keeps
 *          the portable file format and path mapping away from GTK callbacks so
 *          gutter UI and rename handling share one storage contract.
 */

#ifndef GRAPTOS_EDITOR_NOTES_H
#define GRAPTOS_EDITOR_NOTES_H

#include <glib.h>

/**
 * @brief One editor note range.
 */
typedef struct {
    guint id; /**< Stable note id inside one file note database. */
    gint start_line; /**< Zero-based first covered line. */
    gint start_col; /**< Zero-based start column. */
    gint end_line; /**< Zero-based last covered line. */
    gint end_col; /**< Zero-based end column. */
    char *note; /**< User note text. */
} EditorNote;

/**
 * @brief Free one editor note.
 * @param data Note pointer supplied by the owning collection.
 */
void editor_note_free(gpointer data);

/**
 * @brief Create a new editor note.
 * @param id Stable note id.
 * @param start_line Zero-based first covered line.
 * @param start_col Zero-based start column.
 * @param end_line Zero-based last covered line.
 * @param end_col Zero-based end column.
 * @param note User note text.
 * @return Newly allocated note.
 */
EditorNote *editor_note_new(guint id,
                            gint start_line,
                            gint start_col,
                            gint end_line,
                            gint end_col,
                            const char *note);

/**
 * @brief Return the note covering a line.
 * @param notes EditorNote array.
 * @param line Zero-based line.
 * @return Matching note, or NULL.
 */
EditorNote *editor_notes_find_for_line(GPtrArray *notes, gint line);

/**
 * @brief Return the next id for a note array.
 * @param notes EditorNote array.
 * @return Id greater than every existing note id.
 */
guint editor_notes_next_id(GPtrArray *notes);

/**
 * @brief Build the project note storage path for a source file.
 * @details The basename keeps files recognizable while the short hash prevents
 *          collisions between same-named files in different folders.
 * @param project_root Project root used for relative paths.
 * @param file_path Source file path.
 * @return Newly allocated note database path, or NULL.
 */
char *editor_notes_db_path(const char *project_root, const char *file_path);

/**
 * @brief Load notes from a text database.
 * @param db_path Note database path.
 * @return Newly allocated EditorNote array.
 */
GPtrArray *editor_notes_load(const char *db_path);

/**
 * @brief Save notes to a text database.
 * @param db_path Note database path.
 * @param notes EditorNote array.
 * @param error Return location for an optional error.
 * @return TRUE on success.
 */
gboolean editor_notes_save(const char *db_path, GPtrArray *notes, GError **error);

/**
 * @brief Move a note database after a source path change.
 * @param project_root Project root used for relative paths.
 * @param old_path Previous source file path.
 * @param new_path New source file path.
 */
void editor_notes_rename_db(const char *project_root,
                            const char *old_path,
                            const char *new_path);

#endif /* GRAPTOS_EDITOR_NOTES_H */
