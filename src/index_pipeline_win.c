/*
 * index_pipeline_win.c -- Windows stub for parallel indexing pipeline.
 *
 * The pipeline relies on pthreads, fork/exec, and POSIX I/O.
 * Full Windows implementation pending (CreateThread + CreateProcess).
 * All functions return errors gracefully.
 *
 * POSIX implementation: index_pipeline.c
 */

#include "platform.h"

#ifdef TT_PLATFORM_WINDOWS

#include "index_pipeline.h"
#include "schema.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>

struct tt_pipeline_handle
{
    _Atomic(int) finished;
};

int tt_pipeline_start(const tt_pipeline_config_t *config,
                      tt_progress_t *progress,
                      tt_pipeline_handle_t **handle)
{
    (void)config;
    (void)progress;
    (void)handle;
    tt_error_set("indexing pipeline is not yet supported on Windows");
    return -1;
}

bool tt_pipeline_finished(tt_pipeline_handle_t *handle)
{
    (void)handle;
    return true;
}

int tt_pipeline_join(tt_pipeline_handle_t *handle,
                     tt_pipeline_result_t *result)
{
    (void)handle;
    if (result)
        memset(result, 0, sizeof(*result));
    return -1;
}

void tt_pipeline_finalize_schema(tt_database_t *db)
{
    if (!db)
        return;
    tt_schema_create_secondary_indexes(db->db);
    tt_schema_rebuild_fts(db->db);
    tt_schema_create_fts_triggers(db->db);
}

int tt_pipeline_run(const tt_pipeline_config_t *config,
                    tt_pipeline_result_t *result)
{
    (void)config;
    if (result)
        memset(result, 0, sizeof(*result));
    tt_error_set("indexing pipeline is not yet supported on Windows");
    return -1;
}

void tt_pipeline_result_free(tt_pipeline_result_t *result)
{
    if (!result)
        return;
    for (int i = 0; i < result->errors_count; i++)
        free(result->errors[i]);
    free(result->errors);
    result->errors = NULL;
    result->errors_count = 0;
}

#endif /* TT_PLATFORM_WINDOWS */
