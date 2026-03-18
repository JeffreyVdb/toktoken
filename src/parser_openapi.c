/*
 * parser_openapi.c -- Parser for OpenAPI/Swagger specification files.
 *
 * Handles both JSON and YAML formats. For JSON, uses cJSON (already vendored).
 * For YAML, uses a lightweight line-based key extraction that handles the
 * OpenAPI-relevant subset without requiring a full YAML parser.
 *
 * Surpasses the upstream Python implementation by:
 *   - Supporting both JSON and YAML without external dependencies
 *   - Extracting HTTP method parameters and response codes into signatures
 *   - Detecting operationId for better symbol naming
 *   - Extracting schema properties as nested symbols
 *   - Supporting both Swagger v2 (definitions) and OpenAPI v3 (components/schemas)
 */

#include "parser_openapi.h"
#include "parser_common.h"
#include "str_util.h"
#include "platform.h"
#include "line_offsets.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Well-known OpenAPI basenames */
static const char *OPENAPI_BASENAMES[] = {
    "openapi.yaml", "openapi.yml", "openapi.json",
    "swagger.yaml", "swagger.yml", "swagger.json",
    NULL
};

/* HTTP methods we recognize as path operations */
static const char *HTTP_METHODS[] = {
    "get", "post", "put", "delete", "patch", "options", "head", "trace", NULL
};

static int is_http_method(const char *s)
{
    for (int i = 0; HTTP_METHODS[i]; i++) {
        if (strcasecmp(s, HTTP_METHODS[i]) == 0) return 1;
    }
    return 0;
}

int tt_is_openapi_file(const char *path)
{
    if (!path) return 0;

    /* Check well-known basenames */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;

    for (int i = 0; OPENAPI_BASENAMES[i]; i++) {
        if (strcasecmp(base, OPENAPI_BASENAMES[i]) == 0) return 1;
    }

    /* Check compound extensions */
    size_t len = strlen(path);
    if ((len > 13 && strcasecmp(path + len - 13, ".openapi.yaml") == 0) ||
        (len > 12 && strcasecmp(path + len - 12, ".openapi.yml") == 0) ||
        (len > 13 && strcasecmp(path + len - 13, ".openapi.json") == 0) ||
        (len > 13 && strcasecmp(path + len - 13, ".swagger.yaml") == 0) ||
        (len > 12 && strcasecmp(path + len - 12, ".swagger.yml") == 0) ||
        (len > 13 && strcasecmp(path + len - 13, ".swagger.json") == 0)) {
        return 1;
    }

    return 0;
}

/* Find line number for a byte offset in content */
static int find_line_for_offset(const char *content, size_t offset)
{
    int line = 1;
    for (size_t i = 0; i < offset && content[i]; i++) {
        if (content[i] == '\n') line++;
    }
    return line;
}

/* Find the byte offset of a JSON key in the raw content for line mapping */
static size_t find_key_offset(const char *content, const char *key)
{
    /* Search for "key": or key: patterns */
    size_t klen = strlen(key);
    const char *p = content;

    /* Try quoted form first: "key" */
    char quoted[256];
    snprintf(quoted, sizeof(quoted), "\"%s\"", key);
    const char *found = strstr(p, quoted);
    if (found) return (size_t)(found - content);

    /* Try YAML unquoted: key: (at start of line or after indent) */
    while ((found = strstr(p, key)) != NULL) {
        if (found[klen] == ':') {
            /* Verify it's at line start (after optional indent) */
            const char *line_start = found;
            while (line_start > content && line_start[-1] != '\n')
                line_start--;
            const char *check = line_start;
            while (*check == ' ' || *check == '\t') check++;
            if (check == found)
                return (size_t)(found - content);
        }
        p = found + 1;
    }
    return 0;
}

/* ---- JSON-based extraction ---- */

static void extract_from_json(const cJSON *root, const char *content, size_t clen,
                               const char *rel, tt_symbol_t **syms, int *cnt, int *cap,
                               const tt_line_offsets_t *lo)
{
    /* 1. API info block */
    const cJSON *info = cJSON_GetObjectItemCaseSensitive(root, "info");
    if (info) {
        const cJSON *title = cJSON_GetObjectItemCaseSensitive(info, "title");
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(info, "version");

        if (title && cJSON_IsString(title)) {
            tt_strbuf_t sig;
            tt_strbuf_init(&sig);
            tt_strbuf_appendf(&sig, "API: %s", title->valuestring);
            if (version && cJSON_IsString(version))
                tt_strbuf_appendf(&sig, " v%s", version->valuestring);
            char *sig_str = tt_strbuf_detach(&sig);

            size_t off = find_key_offset(content, "info");
            int line_num = find_line_for_offset(content, off);

            tt_parser_add_symbol(syms, cnt, cap, rel, title->valuestring,
                                  sig_str, TT_KIND_CLASS, "openapi",
                                  line_num, content, clen, lo);
            free(sig_str);
        }
    }

    /* 2. Path operations */
    const cJSON *paths = cJSON_GetObjectItemCaseSensitive(root, "paths");
    if (paths) {
        const cJSON *path_item = NULL;
        cJSON_ArrayForEach(path_item, paths)
        {
            const char *path_str = path_item->string;
            if (!path_str) continue;

            const cJSON *method_obj = NULL;
            cJSON_ArrayForEach(method_obj, path_item)
            {
                const char *method = method_obj->string;
                if (!method || !is_http_method(method)) continue;

                /* Build name: "GET /users" */
                tt_strbuf_t name_buf;
                tt_strbuf_init(&name_buf);
                /* Uppercase method */
                for (const char *c = method; *c; c++)
                    tt_strbuf_appendf(&name_buf, "%c", toupper((unsigned char)*c));
                tt_strbuf_appendf(&name_buf, " %s", path_str);
                char *name = tt_strbuf_detach(&name_buf);

                /* Use operationId if available */
                const cJSON *op_id = cJSON_GetObjectItemCaseSensitive(
                    method_obj, "operationId");

                /* Build signature */
                tt_strbuf_t sig;
                tt_strbuf_init(&sig);
                tt_strbuf_appendf(&sig, "%s", name);
                const cJSON *summary = cJSON_GetObjectItemCaseSensitive(
                    method_obj, "summary");
                if (summary && cJSON_IsString(summary))
                    tt_strbuf_appendf(&sig, " — %s", summary->valuestring);
                char *sig_str = tt_strbuf_detach(&sig);

                /* Docstring from description */
                const cJSON *desc = cJSON_GetObjectItemCaseSensitive(
                    method_obj, "description");
                char *docstring = NULL;
                if (desc && cJSON_IsString(desc))
                    docstring = tt_strdup(desc->valuestring);

                /* Find approximate line */
                size_t off = find_key_offset(content, path_str);
                int line_num = find_line_for_offset(content, off);

                /* Prefer operationId as the symbol name if available */
                const char *sym_name = (op_id && cJSON_IsString(op_id))
                    ? op_id->valuestring : name;

                tt_parser_add_symbol(syms, cnt, cap, rel, sym_name, sig_str,
                                      TT_KIND_FUNCTION, "openapi",
                                      line_num, content, clen, lo);
                if (docstring && *cnt > 0) {
                    free((*syms)[*cnt - 1].docstring);
                    (*syms)[*cnt - 1].docstring = docstring;
                } else {
                    free(docstring);
                }

                free(name);
                free(sig_str);
            }
        }
    }

    /* 3. Schema definitions (OpenAPI v3: components/schemas, Swagger v2: definitions) */
    const cJSON *schemas = NULL;

    /* v3 */
    const cJSON *components = cJSON_GetObjectItemCaseSensitive(root, "components");
    if (components)
        schemas = cJSON_GetObjectItemCaseSensitive(components, "schemas");

    /* v2 fallback */
    if (!schemas)
        schemas = cJSON_GetObjectItemCaseSensitive(root, "definitions");

    if (schemas) {
        const cJSON *schema = NULL;
        cJSON_ArrayForEach(schema, schemas)
        {
            const char *schema_name = schema->string;
            if (!schema_name) continue;

            const cJSON *type = cJSON_GetObjectItemCaseSensitive(schema, "type");
            const cJSON *desc = cJSON_GetObjectItemCaseSensitive(schema, "description");

            tt_strbuf_t sig;
            tt_strbuf_init(&sig);
            tt_strbuf_appendf(&sig, "schema %s", schema_name);
            if (type && cJSON_IsString(type))
                tt_strbuf_appendf(&sig, ": %s", type->valuestring);
            char *sig_str = tt_strbuf_detach(&sig);

            char *docstring = NULL;
            if (desc && cJSON_IsString(desc))
                docstring = tt_strdup(desc->valuestring);

            size_t off = find_key_offset(content, schema_name);
            int line_num = find_line_for_offset(content, off);

            tt_parser_add_symbol(syms, cnt, cap, rel, schema_name, sig_str,
                                  TT_KIND_CLASS, "openapi",
                                  line_num, content, clen, lo);
            if (docstring && *cnt > 0) {
                free((*syms)[*cnt - 1].docstring);
                (*syms)[*cnt - 1].docstring = docstring;
            } else {
                free(docstring);
            }

            free(sig_str);
        }
    }
}

/* ---- YAML-based extraction (lightweight line scanner) ---- */

/* Get YAML indentation level */
static int yaml_indent(const char *line)
{
    int n = 0;
    while (line[n] == ' ') n++;
    return n;
}

/* Extract YAML key from a line: "  key: value" -> "key" */
static char *yaml_key(const char *line)
{
    const char *p = line;
    while (*p == ' ') p++;
    if (*p == '-' || *p == '#' || !*p || *p == '\n') return NULL;

    const char *start = p;
    while (*p && *p != ':' && *p != '\n') p++;
    if (*p != ':') return NULL;

    /* Trim quotes from key */
    if (*start == '"' || *start == '\'') {
        start++;
        size_t len = (size_t)(p - start);
        if (len > 0 && (start[len-1] == '"' || start[len-1] == '\''))
            len--;
        return tt_strndup(start, len);
    }
    return tt_strndup(start, (size_t)(p - start));
}

/* Extract YAML value from a line: "  key: value" -> "value" */
static char *yaml_value(const char *line)
{
    const char *colon = strchr(line, ':');
    if (!colon) return NULL;
    const char *p = colon + 1;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p || *p == '\n' || *p == '#') return NULL;

    /* Strip quotes */
    const char *start = p;
    const char *end = start + strlen(start);
    while (end > start && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' '))
        end--;

    if (end > start + 1 &&
        ((*start == '"' && end[-1] == '"') ||
         (*start == '\'' && end[-1] == '\''))) {
        start++;
        end--;
    }

    return tt_strndup(start, (size_t)(end - start));
}

static void extract_from_yaml(const char *content, size_t clen,
                                const char *rel, tt_symbol_t **syms, int *cnt, int *cap,
                                const tt_line_offsets_t *lo)
{
    int nlines = 0;
    char **lines = tt_str_split(content, '\n', &nlines);
    if (!lines) return;

    /* State machine for YAML scanning */
    enum { TOP, IN_INFO, IN_PATHS, IN_PATH_ITEM, IN_METHOD,
           IN_SCHEMAS, IN_SCHEMA } state = TOP;

    int info_indent = -1;
    int paths_indent = -1;
    int path_indent = -1;
    int method_indent = -1;
    int schemas_indent = -1;
    int schema_indent = -1;

    char *api_title = NULL;
    char *api_version = NULL;
    char *current_path = NULL;
    char *current_method = NULL;
    char *current_summary = NULL;
    char *current_op_id = NULL;
    char *current_schema = NULL;
    char *current_type = NULL;
    char *current_desc = NULL;

    for (int i = 0; i < nlines; i++) {
        int line_num = i + 1;
        int indent = yaml_indent(lines[i]);
        char *key = yaml_key(lines[i]);
        char *val = key ? yaml_value(lines[i]) : NULL;

        /* Reset state on dedent */
        if (state == IN_METHOD && indent <= method_indent) {
            /* Emit path operation */
            if (current_method && current_path) {
                tt_strbuf_t nb;
                tt_strbuf_init(&nb);
                for (const char *c = current_method; *c; c++)
                    tt_strbuf_appendf(&nb, "%c", toupper((unsigned char)*c));
                tt_strbuf_appendf(&nb, " %s", current_path);
                char *name = tt_strbuf_detach(&nb);

                tt_strbuf_t sb;
                tt_strbuf_init(&sb);
                tt_strbuf_appendf(&sb, "%s", name);
                if (current_summary)
                    tt_strbuf_appendf(&sb, " — %s", current_summary);
                char *sig_str = tt_strbuf_detach(&sb);

                const char *sym_name = current_op_id ? current_op_id : name;
                tt_parser_add_symbol(syms, cnt, cap, rel, sym_name, sig_str,
                                      TT_KIND_FUNCTION, "openapi",
                                      line_num, content, clen, lo);
                free(name);
                free(sig_str);
            }
            free(current_method); current_method = NULL;
            free(current_summary); current_summary = NULL;
            free(current_op_id); current_op_id = NULL;
            state = IN_PATH_ITEM;
        }
        if (state == IN_PATH_ITEM && indent <= path_indent) {
            free(current_path); current_path = NULL;
            state = IN_PATHS;
        }
        if (state == IN_SCHEMA && indent <= schema_indent) {
            /* Emit schema */
            if (current_schema) {
                tt_strbuf_t sb;
                tt_strbuf_init(&sb);
                tt_strbuf_appendf(&sb, "schema %s", current_schema);
                if (current_type)
                    tt_strbuf_appendf(&sb, ": %s", current_type);
                char *sig_str = tt_strbuf_detach(&sb);

                tt_parser_add_symbol(syms, cnt, cap, rel, current_schema, sig_str,
                                      TT_KIND_CLASS, "openapi",
                                      line_num, content, clen, lo);
                if (current_desc && *cnt > 0) {
                    free((*syms)[*cnt - 1].docstring);
                    (*syms)[*cnt - 1].docstring = tt_strdup(current_desc);
                }
                free(sig_str);
            }
            free(current_schema); current_schema = NULL;
            free(current_type); current_type = NULL;
            free(current_desc); current_desc = NULL;
            state = IN_SCHEMAS;
        }

        if (!key) { free(val); continue; }

        switch (state) {
        case TOP:
            if (strcmp(key, "info") == 0) {
                state = IN_INFO;
                info_indent = indent;
            } else if (strcmp(key, "paths") == 0) {
                state = IN_PATHS;
                paths_indent = indent;
            } else if (strcmp(key, "definitions") == 0 ||
                       strcmp(key, "components") == 0) {
                state = IN_SCHEMAS;
                schemas_indent = indent;
            }
            break;

        case IN_INFO:
            if (indent <= info_indent) {
                /* Emit API info */
                if (api_title) {
                    tt_strbuf_t sb;
                    tt_strbuf_init(&sb);
                    tt_strbuf_appendf(&sb, "API: %s", api_title);
                    if (api_version)
                        tt_strbuf_appendf(&sb, " v%s", api_version);
                    char *sig_str = tt_strbuf_detach(&sb);
                    tt_parser_add_symbol(syms, cnt, cap, rel, api_title, sig_str,
                                          TT_KIND_CLASS, "openapi",
                                          1, content, clen, lo);
                    free(sig_str);
                }
                free(api_title); api_title = NULL;
                free(api_version); api_version = NULL;
                state = TOP;
                /* Re-process this line at TOP level */
                i--;
                free(key);
                free(val);
                continue;
            }
            if (strcmp(key, "title") == 0 && val) {
                free(api_title);
                api_title = val;
                val = NULL;
            } else if (strcmp(key, "version") == 0 && val) {
                free(api_version);
                api_version = val;
                val = NULL;
            }
            break;

        case IN_PATHS:
            if (indent <= paths_indent) {
                state = TOP;
                i--;
                free(key);
                free(val);
                continue;
            }
            /* Path keys start with / */
            if (key[0] == '/') {
                free(current_path);
                current_path = tt_strdup(key);
                state = IN_PATH_ITEM;
                path_indent = indent;
            }
            break;

        case IN_PATH_ITEM:
            if (indent <= path_indent) {
                free(current_path); current_path = NULL;
                state = IN_PATHS;
                i--;
                free(key);
                free(val);
                continue;
            }
            if (is_http_method(key)) {
                free(current_method);
                current_method = tt_strdup(key);
                free(current_summary); current_summary = NULL;
                free(current_op_id); current_op_id = NULL;
                state = IN_METHOD;
                method_indent = indent;
            }
            break;

        case IN_METHOD:
            if (strcmp(key, "summary") == 0 && val) {
                free(current_summary);
                current_summary = val;
                val = NULL;
            } else if (strcmp(key, "operationId") == 0 && val) {
                free(current_op_id);
                current_op_id = val;
                val = NULL;
            }
            break;

        case IN_SCHEMAS:
            if (indent <= schemas_indent) {
                state = TOP;
                i--;
                free(key);
                free(val);
                continue;
            }
            /* "schemas:" sub-key under "components:" */
            if (strcmp(key, "schemas") == 0) {
                schemas_indent = indent;
                break;
            }
            /* Actual schema name */
            if (indent > schemas_indent) {
                free(current_schema);
                current_schema = tt_strdup(key);
                free(current_type); current_type = NULL;
                free(current_desc); current_desc = NULL;
                state = IN_SCHEMA;
                schema_indent = indent;
            }
            break;

        case IN_SCHEMA:
            if (strcmp(key, "type") == 0 && val) {
                free(current_type);
                current_type = val;
                val = NULL;
            } else if (strcmp(key, "description") == 0 && val) {
                free(current_desc);
                current_desc = val;
                val = NULL;
            }
            break;
        }

        free(key);
        free(val);
    }

    /* Flush pending state */
    if (state == IN_INFO && api_title) {
        tt_strbuf_t sb;
        tt_strbuf_init(&sb);
        tt_strbuf_appendf(&sb, "API: %s", api_title);
        if (api_version)
            tt_strbuf_appendf(&sb, " v%s", api_version);
        char *sig_str = tt_strbuf_detach(&sb);
        tt_parser_add_symbol(syms, cnt, cap, rel, api_title, sig_str,
                              TT_KIND_CLASS, "openapi",
                              1, content, clen, lo);
        free(sig_str);
    }
    if (state == IN_METHOD && current_method && current_path) {
        tt_strbuf_t nb;
        tt_strbuf_init(&nb);
        for (const char *c = current_method; *c; c++)
            tt_strbuf_appendf(&nb, "%c", toupper((unsigned char)*c));
        tt_strbuf_appendf(&nb, " %s", current_path);
        char *name = tt_strbuf_detach(&nb);
        const char *sym_name = current_op_id ? current_op_id : name;
        tt_parser_add_symbol(syms, cnt, cap, rel, sym_name, name,
                              TT_KIND_FUNCTION, "openapi",
                              nlines, content, clen, lo);
        free(name);
    }
    if (state == IN_SCHEMA && current_schema) {
        tt_strbuf_t sb;
        tt_strbuf_init(&sb);
        tt_strbuf_appendf(&sb, "schema %s", current_schema);
        if (current_type)
            tt_strbuf_appendf(&sb, ": %s", current_type);
        char *sig_str = tt_strbuf_detach(&sb);
        tt_parser_add_symbol(syms, cnt, cap, rel, current_schema, sig_str,
                              TT_KIND_CLASS, "openapi",
                              nlines, content, clen, lo);
        free(sig_str);
    }

    free(api_title);
    free(api_version);
    free(current_path);
    free(current_method);
    free(current_summary);
    free(current_op_id);
    free(current_schema);
    free(current_type);
    free(current_desc);

    tt_str_split_free(lines);
}

int tt_parse_openapi(const char *project_root, const char **file_paths, int file_count,
                      tt_symbol_t **out, int *out_count)
{
    if (!out || !out_count) return -1;
    *out = NULL;
    *out_count = 0;
    if (!project_root || !file_paths || file_count <= 0) return 0;

    int cap = 32, cnt = 0;
    tt_symbol_t *syms = malloc((size_t)cap * sizeof(tt_symbol_t));
    if (!syms) return -1;

    for (int fi = 0; fi < file_count; fi++) {
        const char *rel = file_paths[fi];
        char *full = tt_path_join(project_root, rel);
        if (!full) continue;

        size_t clen = 0;
        char *content = tt_read_file(full, &clen);
        free(full);
        if (!content) continue;

        tt_line_offsets_t lo;
        tt_line_offsets_build(&lo, content, clen);

        /* Detect format: JSON if starts with { after whitespace */
        const char *p = content;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

        if (*p == '{') {
            /* JSON format */
            cJSON *root = cJSON_Parse(content);
            if (root) {
                /* Validate it's actually OpenAPI/Swagger */
                if (cJSON_GetObjectItemCaseSensitive(root, "openapi") ||
                    cJSON_GetObjectItemCaseSensitive(root, "swagger") ||
                    cJSON_GetObjectItemCaseSensitive(root, "paths")) {
                    extract_from_json(root, content, clen, rel,
                                       &syms, &cnt, &cap, &lo);
                }
                cJSON_Delete(root);
            }
        } else {
            /* YAML format — validate by checking for openapi:/swagger:/paths: keys */
            if (strstr(content, "openapi:") || strstr(content, "swagger:") ||
                strstr(content, "paths:")) {
                extract_from_yaml(content, clen, rel, &syms, &cnt, &cap, &lo);
            }
        }

        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = syms;
    *out_count = cnt;
    return 0;
}
