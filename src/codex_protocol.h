/**
 * @file src/codex_protocol.h
 * @brief Codex JSON line protocol helpers.
 */

#ifndef CLEAF_CODEX_PROTOCOL_H
#define CLEAF_CODEX_PROTOCOL_H

#include <json-glib/json-glib.h>

/**
 * @brief Codex protocol request.
 */
char *codex_protocol_request(guint64 id,
                             const char *method,
                             JsonNode *params);
/**
 * @brief Codex protocol notification.
 */
char *codex_protocol_notification(const char *method, JsonNode *params);
/**
 * @brief Codex protocol response.
 */
char *codex_protocol_response(guint64 id, JsonNode *result);
/**
 * @brief Codex protocol object params.
 */
JsonNode *codex_protocol_object_params(void);
/**
 * @brief Codex protocol parse.
 */
gboolean codex_protocol_parse(const char *line,
                              JsonNode **root_out,
                              GError **error);

#endif /* CLEAF_CODEX_PROTOCOL_H */
