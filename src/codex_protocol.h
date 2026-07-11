#ifndef CLEAF_CODEX_PROTOCOL_H
#define CLEAF_CODEX_PROTOCOL_H

#include <json-glib/json-glib.h>

char *codex_protocol_request(guint64 id,
                             const char *method,
                             JsonNode *params);
char *codex_protocol_notification(const char *method, JsonNode *params);
char *codex_protocol_response(guint64 id, JsonNode *result);
JsonNode *codex_protocol_object_params(void);
gboolean codex_protocol_parse(const char *line,
                              JsonNode **root_out,
                              GError **error);

#endif /* CLEAF_CODEX_PROTOCOL_H */
