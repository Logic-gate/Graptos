/**
 * @file src/codex_markdown.h
 * @brief Codex response markdown renderer.
 * @details AI touches the parts of the app where mistakes are expensive: files,
 *          permissions, markdown, and long-running processes. We keep protocol, client,
 *          panel, and review logic separated so approval and cleanup paths are easy to
 *          audit.
 */

#ifndef GRAPTOS_CODEX_MARKDOWN_H
#define GRAPTOS_CODEX_MARKDOWN_H
#include <gtk/gtk.h>
/**
 * @brief Codex markdown render.
 * @details Replaces the target buffer with styled text for the supported subset.
 * @param buffer The text buffer used for the operation.
 * @param markdown The markdown supplied by the caller.
 */
void codex_markdown_render(GtkTextBuffer *buffer, const char *markdown);
#endif
