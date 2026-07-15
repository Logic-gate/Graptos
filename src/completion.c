/**
 * @file src/completion.c
 * @brief Text completion candidate extraction helpers.
 */

#include "completion.h"

#include <string.h>

/**
 * @brief Ascii word start.
 */
static gboolean ascii_word_start(unsigned char ch) {
    return g_ascii_isalpha((gchar)ch) || ch == '_';
}

/**
 * @brief Ascii word char.
 */
static gboolean ascii_word_char(unsigned char ch) {
    return g_ascii_isalnum((gchar)ch) || ch == '_';
}

/**
 * @brief Completion is word char.
 */
gboolean completion_is_word_char(gunichar ch) {
    return g_unichar_isalnum(ch) || ch == (gunichar)'_';
}

/**
 * @brief Completion prefix at cursor.
 */
char *completion_prefix_at_cursor(GtkTextBuffer *buffer, GtkTextIter *prefix_start, GtkTextIter *cursor) {
    if (!buffer) return NULL;

    GtkTextIter iter;
    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

    GtkTextIter start = iter;
    while (!gtk_text_iter_starts_line(&start)) {
        GtkTextIter prev = start;
        if (!gtk_text_iter_backward_char(&prev)) break;
        gunichar ch = gtk_text_iter_get_char(&prev);
        if (!completion_is_word_char(ch)) break;
        start = prev;
    }

    if (cursor) *cursor = iter;
    if (prefix_start) *prefix_start = start;
    if (gtk_text_iter_equal(&start, &iter)) return g_strdup("");
    return gtk_text_buffer_get_text(buffer, &start, &iter, FALSE);
}

/**
 * @brief Candidate matches.
 */
static gboolean candidate_matches(const char *word, const char *prefix) {
    if (!word || !prefix || prefix[0] == '\0') return FALSE;
    gsize prefix_len = strlen(prefix);
    gsize word_len = strlen(word);
    if (word_len <= prefix_len) return FALSE;
    return strncmp(word, prefix, prefix_len) == 0;
}

/**
 * @brief Candidate exists.
 */
static gboolean candidate_exists(GPtrArray *out, const char *word) {
    if (!out || !word) return FALSE;
    for (guint i = 0; i < out->len; i++) {
        const char *existing = g_ptr_array_index(out, i);
        if (existing && strcmp(existing, word) == 0) return TRUE;
    }
    return FALSE;
}

/**
 * @brief Completion candidates add.
 */
void completion_candidates_add(GPtrArray *out, const char *word, const char *prefix, guint max_results) {
    if (!out || !word || !prefix) return;
    if (out->len >= max_results) return;
    if (!candidate_matches(word, prefix)) return;
    if (candidate_exists(out, word)) return;
    g_ptr_array_add(out, g_strdup(word));
}

/**
 * @brief Collect words from text.
 */
static void collect_words_from_text(GPtrArray *out, const char *text, const char *prefix, guint max_results) {
    if (!out || !text || !prefix) return;

    const char *p = text;
    while (*p != '\0' && out->len < max_results) {
        unsigned char ch = (unsigned char)*p;
        if (!ascii_word_start(ch)) {
            p++;
            continue;
        }

        const char *start = p;
        p++;
        while (*p != '\0' && ascii_word_char((unsigned char)*p)) p++;

        gsize len = (gsize)(p - start);
        if (len >= 2u && len <= 96u) {
            char *word = g_strndup(start, len);
            completion_candidates_add(out, word, prefix, max_results);
            g_free(word);
        }
    }
}

/**
 * @brief Collect syntax completions.
 */
static void collect_syntax_completions(GPtrArray *out, SyntaxDef *syntax, const char *prefix, guint max_results) {
    if (!out || !syntax || !prefix) return;

    if (syntax->completions) {
        for (guint i = 0; i < syntax->completions->len && out->len < max_results; i++) {
            const char *word = g_ptr_array_index(syntax->completions, i);
            completion_candidates_add(out, word, prefix, max_results);
        }
    }

    if (syntax->rules) {
        for (guint i = 0; i < syntax->rules->len && out->len < max_results; i++) {
            SyntaxRule *rule = g_ptr_array_index(syntax->rules, i);
            if (!rule || !rule->pattern) continue;
            collect_words_from_text(out, rule->pattern, prefix, max_results);
        }
    }
}

/**
 * @brief Collect buffer words.
 */
static void collect_buffer_words(GPtrArray *out, GtkTextBuffer *buffer, const char *prefix, guint max_results) {
    if (!out || !buffer || !prefix) return;
    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    end = start;
    if (!gtk_text_iter_forward_chars(&end, CLEAF_COMPLETION_MAX_SCAN_CHARS)) {
        gtk_text_buffer_get_end_iter(buffer, &end);
    }
    char *text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
    collect_words_from_text(out, text, prefix, max_results);
    g_free(text);
}

/**
 * @brief Compare candidate words.
 */
static gint compare_candidate_words(gconstpointer a, gconstpointer b) {
    const char *sa = *(char * const *)a;
    const char *sb = *(char * const *)b;
    return g_ascii_strcasecmp(sa ? sa : "", sb ? sb : "");
}

/**
 * @brief Completion candidates new.
 */
GPtrArray *completion_candidates_new(GtkTextBuffer *buffer, SyntaxDef *syntax, const char *prefix, guint max_results) {
    if (max_results == 0u) max_results = CLEAF_COMPLETION_DEFAULT_MAX_RESULTS;
    GPtrArray *out = g_ptr_array_new_with_free_func(g_free);
    if (!out) return NULL;

    if (!prefix || g_utf8_strlen(prefix, -1) < 2) return out;

    collect_syntax_completions(out, syntax, prefix, max_results);
    collect_buffer_words(out, buffer, prefix, max_results);
    g_ptr_array_sort(out, compare_candidate_words);
    return out;
}
