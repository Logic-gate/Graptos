/**
 * @file src/editor_notes.c
 * @brief Text-backed editor note storage.
 * @details Notes need to travel with a project without making source buffers
 *          dirty. The small text format here keeps that state reviewable and
 *          mergeable while the editor owns the transient UI around it.
 */

#include "editor_notes.h"

#include <glib/gstdio.h>
#include <errno.h>

/**
 * @brief Escape one note field for TSV storage.
 * @param text User note text.
 * @return Escaped text.
 */
static char *notes_escape_text(const char *text) {
    GString *out = g_string_new(NULL);
    if (!out) return NULL;
    for (const char *p = text ? text : ""; *p; p++) {
        if (*p == '\\') g_string_append(out, "\\\\");
        else if (*p == '\n') g_string_append(out, "\\n");
        else if (*p == '\t') g_string_append(out, "\\t");
        else g_string_append_c(out, *p);
    }
    return g_string_free(out, FALSE);
}

/**
 * @brief Unescape one note field from TSV storage.
 * @param text Escaped field text.
 * @return Unescaped text.
 */
static char *notes_unescape_text(const char *text) {
    GString *out = g_string_new(NULL);
    if (!out) return NULL;
    for (const char *p = text ? text : ""; *p; p++) {
        if (*p == '\\' && p[1] != '\0') {
            p++;
            if (*p == 'n') g_string_append_c(out, '\n');
            else if (*p == 't') g_string_append_c(out, '\t');
            else g_string_append_c(out, *p);
        } else {
            g_string_append_c(out, *p);
        }
    }
    return g_string_free(out, FALSE);
}

void editor_note_free(gpointer data) {
    EditorNote *note = data;
    if (!note) return;
    g_free(note->note);
    g_free(note);
}

EditorNote *editor_note_new(guint id,
                            gint start_line,
                            gint start_col,
                            gint end_line,
                            gint end_col,
                            const char *note) {
    EditorNote *item = g_new0(EditorNote, 1);
    if (!item) return NULL;
    item->id = id;
    item->start_line = MAX(0, start_line);
    item->start_col = MAX(0, start_col);
    item->end_line = MAX(item->start_line, end_line);
    item->end_col = MAX(0, end_col);
    item->note = g_strdup(note ? note : "");
    return item;
}

EditorNote *editor_notes_find_for_line(GPtrArray *notes, gint line) {
    if (!notes || line < 0) return NULL;
    for (guint i = 0u; i < notes->len; i++) {
        EditorNote *note = g_ptr_array_index(notes, i);
        if (note && line >= note->start_line && line <= note->end_line) {
            return note;
        }
    }
    return NULL;
}

guint editor_notes_next_id(GPtrArray *notes) {
    guint next = 1u;
    if (!notes) return next;
    for (guint i = 0u; i < notes->len; i++) {
        EditorNote *note = g_ptr_array_index(notes, i);
        if (note && note->id >= next) next = note->id + 1u;
    }
    return next;
}

/**
 * @brief Return a path relative to the project root when possible.
 * @param project_root Project root.
 * @param file_path Source file path.
 * @return Newly allocated relative or absolute path.
 */
static char *notes_relative_path(const char *project_root, const char *file_path) {
    if (!file_path || file_path[0] == '\0') return NULL;
    if (project_root && project_root[0] != '\0') {
        g_autofree char *root = g_canonicalize_filename(project_root, NULL);
        g_autofree char *path = g_canonicalize_filename(file_path, NULL);
        if (root && path && g_str_has_prefix(path, root)) {
            gsize len = strlen(root);
            if (path[len] == G_DIR_SEPARATOR) return g_strdup(path + len + 1u);
            if (path[len] == '\0') return g_path_get_basename(path);
        }
    }
    return g_strdup(file_path);
}

char *editor_notes_db_path(const char *project_root, const char *file_path) {
    if (!project_root || project_root[0] == '\0' || !file_path || file_path[0] == '\0') {
        return NULL;
    }
    g_autofree char *relative = notes_relative_path(project_root, file_path);
    if (!relative) return NULL;
    g_autofree char *basename = g_path_get_basename(file_path);
    if (!basename || basename[0] == '\0') return NULL;
    guint hash = g_str_hash(relative);
    g_autofree char *filename =
        g_strdup_printf("%s.%08x.notes.txt", basename, hash);
    return g_build_filename(project_root, ".graptos", "notes", filename, NULL);
}

/**
 * @brief Parse one note database line.
 * @param line TSV record.
 * @return Parsed note, or NULL.
 */
static EditorNote *notes_parse_line(const char *line) {
    if (!line || line[0] == '\0') return NULL;
    g_auto(GStrv) parts = g_strsplit(line, "\t", 6);
    if (!parts || !parts[0] || !parts[1] || !parts[2] ||
        !parts[3] || !parts[4] || !parts[5]) {
        return NULL;
    }
    char *end = NULL;
    guint64 id = g_ascii_strtoull(parts[0], &end, 10);
    if (end == parts[0] || *end != '\0' || id == 0u || id > G_MAXUINT) return NULL;
    gint64 start_line = g_ascii_strtoll(parts[1], &end, 10);
    if (end == parts[1] || *end != '\0' || start_line < 0 || start_line > G_MAXINT) return NULL;
    gint64 start_col = g_ascii_strtoll(parts[2], &end, 10);
    if (end == parts[2] || *end != '\0' || start_col < 0 || start_col > G_MAXINT) return NULL;
    gint64 end_line = g_ascii_strtoll(parts[3], &end, 10);
    if (end == parts[3] || *end != '\0' || end_line < start_line || end_line > G_MAXINT) return NULL;
    gint64 end_col = g_ascii_strtoll(parts[4], &end, 10);
    if (end == parts[4] || *end != '\0' || end_col < 0 || end_col > G_MAXINT) return NULL;
    g_autofree char *text = notes_unescape_text(parts[5]);
    return editor_note_new((guint)id, (gint)start_line, (gint)start_col,
                           (gint)end_line, (gint)end_col, text);
}

GPtrArray *editor_notes_load(const char *db_path) {
    GPtrArray *notes = g_ptr_array_new_with_free_func(editor_note_free);
    if (!notes || !db_path || db_path[0] == '\0') return notes;
    g_autofree char *contents = NULL;
    gsize len = 0u;
    GError *error = NULL;
    if (!g_file_get_contents(db_path, &contents, &len, &error)) {
        if (error) g_error_free(error);
        return notes;
    }
    g_auto(GStrv) lines = g_strsplit(contents, "\n", -1);
    for (guint i = 0u; lines && lines[i]; i++) {
        if (lines[i][0] == '\0') continue;
        EditorNote *note = notes_parse_line(lines[i]);
        if (note) g_ptr_array_add(notes, note);
    }
    return notes;
}

gboolean editor_notes_save(const char *db_path, GPtrArray *notes, GError **error) {
    if (!db_path || db_path[0] == '\0') return TRUE;
    g_autofree char *dir = g_path_get_dirname(db_path);
    if (dir && g_mkdir_with_parents(dir, 0755) != 0) {
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
                    "Could not create note directory %s", dir);
        return FALSE;
    }
    GString *out = g_string_new(NULL);
    if (!out) return FALSE;
    for (guint i = 0u; notes && i < notes->len; i++) {
        EditorNote *note = g_ptr_array_index(notes, i);
        if (!note) continue;
        g_autofree char *escaped = notes_escape_text(note->note);
        g_string_append_printf(out, "%u\t%d\t%d\t%d\t%d\t%s\n",
                               note->id,
                               note->start_line,
                               note->start_col,
                               note->end_line,
                               note->end_col,
                               escaped ? escaped : "");
    }
    g_autofree char *contents = g_string_free(out, FALSE);
    return g_file_set_contents(db_path, contents ? contents : "", -1, error);
}

void editor_notes_rename_db(const char *project_root,
                            const char *old_path,
                            const char *new_path) {
    g_autofree char *old_db = editor_notes_db_path(project_root, old_path);
    g_autofree char *new_db = editor_notes_db_path(project_root, new_path);
    if (!old_db || !new_db || g_strcmp0(old_db, new_db) == 0) return;
    if (!g_file_test(old_db, G_FILE_TEST_EXISTS)) return;
    g_autofree char *dir = g_path_get_dirname(new_db);
    if (dir) (void)g_mkdir_with_parents(dir, 0755);
    if (g_rename(old_db, new_db) != 0 && g_file_test(new_db, G_FILE_TEST_EXISTS)) {
        (void)g_unlink(old_db);
    }
}
