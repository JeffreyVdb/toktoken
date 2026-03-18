/*
 * parser_openapi.h -- Parser for OpenAPI/Swagger specification files.
 */

#ifndef TT_PARSER_OPENAPI_H
#define TT_PARSER_OPENAPI_H

#include "symbol.h"

/*
 * tt_parse_openapi -- Parse OpenAPI/Swagger files for API symbols.
 *
 * Detects OpenAPI by well-known basenames or content inspection.
 * Handles JSON (via cJSON) and YAML (lightweight line-based parser).
 *
 * Extracts:
 *   - API info block (title, version) -> CLASS
 *   - Path operations (GET /users)    -> FUNCTION
 *   - Schema definitions              -> CLASS
 */
int tt_parse_openapi(const char *project_root, const char **file_paths, int file_count,
                      tt_symbol_t **out, int *out_count);

/* Check if a file path looks like an OpenAPI/Swagger spec */
int tt_is_openapi_file(const char *path);

#endif /* TT_PARSER_OPENAPI_H */
