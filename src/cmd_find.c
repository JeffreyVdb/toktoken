/*
 * cmd_find.c -- find:importers and find:references commands.
 *
 * find:importers <file>       -> who imports this file?
 * find:references <identifier> -> who imports this symbol name?
 */

#include "cmd_find.h"
#include "json_output.h"
#include "error.h"
#include "platform.h"
#include "database.h"
#include "index_store.h"
#include "import_extractor.h"
#include "str_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *resolve_query(tt_cli_opts_t *opts)
{
    if (opts->search && opts->search[0]) return opts->search;
    if (opts->positional_count > 0) return opts->positional[0];
    return "";
}

static cJSON *make_error(const char *code, const char *message, const char *hint)
{
    cJSON *json = cJSON_CreateObject();
    if (!json) return NULL;
    cJSON_AddStringToObject(json, "error", code);
    cJSON_AddStringToObject(json, "message", message);
    if (hint && hint[0])
        cJSON_AddStringToObject(json, "hint", hint);
    return json;
}

static cJSON *format_imports(const tt_import_t *imps, int count)
{
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "from_file", imps[i].from_file ? imps[i].from_file : "");
        cJSON_AddStringToObject(obj, "to_specifier", imps[i].to_specifier ? imps[i].to_specifier : "");
        if (imps[i].to_file)
            cJSON_AddStringToObject(obj, "to_file", imps[i].to_file);
        if (imps[i].symbol_name)
            cJSON_AddStringToObject(obj, "symbol_name", imps[i].symbol_name);
        cJSON_AddNumberToObject(obj, "line", imps[i].line);
        cJSON_AddStringToObject(obj, "import_type", imps[i].import_type ? imps[i].import_type : "");
        cJSON_AddItemToArray(arr, obj);
    }
    return arr;
}

cJSON *tt_cmd_find_importers_exec(tt_cli_opts_t *opts)
{
    const char *query = resolve_query(opts);
    if (!query || !query[0]) {
        return make_error("missing_argument",
                           "Usage: find:importers <file>",
                           "Specify a file path to find its importers");
    }

    char *project_path = tt_resolve_project_path(opts->path);
    if (!project_path) {
        tt_error_set("Failed to resolve project path");
        return NULL;
    }

    tt_database_t db;
    memset(&db, 0, sizeof(db));
    if (tt_database_open(&db, project_path) < 0) {
        free(project_path);
        return make_error("storage_error", "Failed to open database", NULL);
    }

    tt_index_store_t store;
    if (tt_store_init(&store, &db) < 0) {
        tt_database_close(&db);
        free(project_path);
        return make_error("storage_error", "Failed to prepare statements", NULL);
    }

    tt_import_t *imps = NULL;
    int count = 0;
    tt_store_get_importers(&store, query, &imps, &count);

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "file", query);
    cJSON_AddNumberToObject(result, "count", count);
    cJSON_AddItemToObject(result, "importers", format_imports(imps, count));

    tt_import_array_free(imps, count);
    tt_store_close(&store);
    tt_database_close(&db);
    free(project_path);
    return result;
}

cJSON *tt_cmd_find_references_exec(tt_cli_opts_t *opts)
{
    const char *query = resolve_query(opts);
    if (!query || !query[0]) {
        return make_error("missing_argument",
                           "Usage: find:references <identifier>",
                           "Specify an identifier to find its references");
    }

    char *project_path = tt_resolve_project_path(opts->path);
    if (!project_path) {
        tt_error_set("Failed to resolve project path");
        return NULL;
    }

    tt_database_t db;
    memset(&db, 0, sizeof(db));
    if (tt_database_open(&db, project_path) < 0) {
        free(project_path);
        return make_error("storage_error", "Failed to open database", NULL);
    }

    tt_index_store_t store;
    if (tt_store_init(&store, &db) < 0) {
        tt_database_close(&db);
        free(project_path);
        return make_error("storage_error", "Failed to prepare statements", NULL);
    }

    tt_import_t *imps = NULL;
    int count = 0;
    tt_store_find_references(&store, query, &imps, &count);

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "identifier", query);
    cJSON_AddNumberToObject(result, "count", count);
    cJSON_AddItemToObject(result, "references", format_imports(imps, count));

    tt_import_array_free(imps, count);
    tt_store_close(&store);
    tt_database_close(&db);
    free(project_path);
    return result;
}

int tt_cmd_find_importers(tt_cli_opts_t *opts)
{
    cJSON *result = tt_cmd_find_importers_exec(opts);
    if (!result) {
        const char *err = tt_error_get();
        if (err && err[0])
            return tt_output_error("internal_error", err, NULL);
        return 0;
    }
    if (cJSON_GetObjectItem(result, "error")) {
        tt_json_print(result);
        cJSON_Delete(result);
        return 1;
    }
    tt_json_print(result);
    cJSON_Delete(result);
    return 0;
}

int tt_cmd_find_references(tt_cli_opts_t *opts)
{
    cJSON *result = tt_cmd_find_references_exec(opts);
    if (!result) {
        const char *err = tt_error_get();
        if (err && err[0])
            return tt_output_error("internal_error", err, NULL);
        return 0;
    }
    if (cJSON_GetObjectItem(result, "error")) {
        tt_json_print(result);
        cJSON_Delete(result);
        return 1;
    }
    tt_json_print(result);
    cJSON_Delete(result);
    return 0;
}

cJSON *tt_cmd_find_callers_exec(tt_cli_opts_t *opts)
{
    const char *query = resolve_query(opts);
    if (!query || !query[0]) {
        return make_error("missing_argument",
                           "Usage: find:callers <symbol-id>",
                           "Specify a symbol ID to find its callers");
    }

    char *project_path = tt_resolve_project_path(opts->path);
    if (!project_path) {
        tt_error_set("Failed to resolve project path");
        return NULL;
    }

    tt_database_t db;
    memset(&db, 0, sizeof(db));
    if (tt_database_open(&db, project_path) < 0) {
        free(project_path);
        return make_error("storage_error", "Failed to open database", NULL);
    }

    tt_index_store_t store;
    if (tt_store_init(&store, &db) < 0) {
        tt_database_close(&db);
        free(project_path);
        return make_error("storage_error", "Failed to prepare statements", NULL);
    }

    int limit = opts->limit > 0 ? opts->limit : 50;
    tt_caller_t *callers = NULL;
    int count = 0;
    tt_store_find_callers(&store, query, limit, &callers, &count);

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "symbol", query);
    cJSON_AddNumberToObject(result, "n", count);
    cJSON *arr = cJSON_AddArrayToObject(result, "callers");
    for (int i = 0; i < count; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "id", callers[i].id);
        cJSON_AddStringToObject(obj, "file", callers[i].file);
        cJSON_AddStringToObject(obj, "name", callers[i].name);
        cJSON_AddStringToObject(obj, "kind", callers[i].kind);
        cJSON_AddNumberToObject(obj, "line", callers[i].line);
        if (callers[i].signature && callers[i].signature[0])
            cJSON_AddStringToObject(obj, "sig", callers[i].signature);
        cJSON_AddItemToArray(arr, obj);
    }

    tt_caller_free(callers, count);
    tt_store_close(&store);
    tt_database_close(&db);
    free(project_path);
    return result;
}

int tt_cmd_find_callers(tt_cli_opts_t *opts)
{
    cJSON *result = tt_cmd_find_callers_exec(opts);
    if (!result) {
        const char *err = tt_error_get();
        if (err && err[0])
            return tt_output_error("internal_error", err, NULL);
        return 0;
    }
    if (cJSON_GetObjectItem(result, "error")) {
        tt_json_print(result);
        cJSON_Delete(result);
        return 1;
    }
    tt_json_print(result);
    cJSON_Delete(result);
    return 0;
}
