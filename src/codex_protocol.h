/**
 * @file src/codex_protocol.h
 * @brief Codex JSON line protocol helpers.
 * @details AI touches the parts of the app where mistakes are expensive: files,
 *          permissions, markdown, and long-running processes. We keep protocol, client,
 *          panel, and review logic separated so approval and cleanup paths are easy to
 *          audit.
 */

#ifndef GRAPTOS_CODEX_PROTOCOL_H
#define GRAPTOS_CODEX_PROTOCOL_H

#include <json-glib/json-glib.h>

/**
 * @brief Codex protocol request.
 * @details The returned line is newline-terminated and owned by the caller.
 * @param id The id supplied by the caller.
 * @param method The method supplied by the caller.
 * @param params The params supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *codex_protocol_request(guint64 id,
                             const char *method,
                             JsonNode *params);
/**
 * @brief Codex protocol notification.
 * @details The returned line is suitable for direct writing to Codex stdin.
 * @param method The method supplied by the caller.
 * @param params The params supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *codex_protocol_notification(const char *method, JsonNode *params);
/**
 * @brief Codex protocol response.
 * @details Used for approval replies where Codex already supplied the id.
 * @param id The id supplied by the caller.
 * @param result The result supplied by the caller.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
char *codex_protocol_response(guint64 id, JsonNode *result);
/**
 * @brief Codex protocol object params.
 * @details The returned JsonNode owns an empty JsonObject.
 * @return The resolved value for the caller, or NULL when no suitable value is available.
 */
JsonNode *codex_protocol_object_params(void);
/**
 * @brief Codex protocol parse.
 * @details On success, `root_out` receives an owned object node.
 * @param line The zero-based or display line handled by the caller, matching the surrounding API.
 * @param root_out Output storage filled when the operation can provide a value.
 * @param error Return location for an optional error; left untouched on success unless GTK sets it.
 * @return TRUE when the condition is satisfied; otherwise FALSE.
 */
gboolean codex_protocol_parse(const char *line,
                              JsonNode **root_out,
                              GError **error);

#endif /* GRAPTOS_CODEX_PROTOCOL_H */
