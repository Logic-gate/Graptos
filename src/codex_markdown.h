/**
 * @file src/codex_markdown.h
 * @brief Codex response markdown renderer.
 */

#ifndef CLEAF_CODEX_MARKDOWN_H
#define CLEAF_CODEX_MARKDOWN_H
#include <gtk/gtk.h>
/**
 * @brief Codex markdown render.
 */
void codex_markdown_render(GtkTextBuffer *buffer, const char *markdown);
#endif
