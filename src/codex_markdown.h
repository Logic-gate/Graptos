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
 * @param heading_color The primary heading/link color.
 * @param success_color The secondary heading color.
 * @param warning_color The tertiary heading color.
 * @param code_fg_color The code foreground color.
 * @param code_bg_color The code background color.
 * @param quote_color The quoted text color.
 */
void codex_markdown_render(GtkTextBuffer *buffer,
                           const char *markdown,
                           const char *heading_color,
                           const char *success_color,
                           const char *warning_color,
                           const char *code_fg_color,
                           const char *code_bg_color,
                           const char *quote_color);
#endif
