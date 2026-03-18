/*
 * test_e2e_index.c -- E2E tests for index:create and index:update.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "test_e2e_helpers.h"

#include <stdlib.h>
#include <string.h>

static void clean_index(const char *path)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cache:clear --path %s", path);
    tt_e2e_run(cmd, NULL);
}

TT_TEST(test_e2e_index_create_succeeds)
{
    const char *fixture = tt_test_fixture("mini-project");
    if (!fixture) return;

    clean_index(fixture);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "index:create --path %s", fixture);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);
    TT_ASSERT_NOT_NULL(json);

    if (json) {
        cJSON *files = cJSON_GetObjectItemCaseSensitive(json, "files");
        TT_ASSERT(cJSON_IsNumber(files) && files->valueint > 0,
                   "should have files > 0");
        cJSON_Delete(json);
    }

    clean_index(fixture);
}

TT_TEST(test_e2e_index_create_empty_dir_fails)
{
    const char *fixture = tt_test_fixture("empty-dir");
    if (!fixture) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "index:create --path %s", fixture);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(1, exit_code);

    if (json) {
        cJSON *err = cJSON_GetObjectItemCaseSensitive(json, "error");
        TT_ASSERT(cJSON_IsString(err), "should have error field");
        cJSON_Delete(json);
    }
}

TT_TEST(test_e2e_index_update_no_index_fails)
{
    const char *fixture = tt_test_fixture("empty-dir");
    if (!fixture) return;

    clean_index(fixture);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "index:update --path %s", fixture);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(1, exit_code);

    if (json) {
        cJSON *err = cJSON_GetObjectItemCaseSensitive(json, "error");
        TT_ASSERT(cJSON_IsString(err), "should have error field");
        cJSON_Delete(json);
    }
}

TT_TEST(test_e2e_index_update_no_changes)
{
    const char *fixture = tt_test_fixture("mini-project");
    if (!fixture) return;

    clean_index(fixture);

    /* Create index first */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "index:create --path %s", fixture);
    tt_e2e_run(cmd, NULL);

    /* Update immediately (no changes) */
    snprintf(cmd, sizeof(cmd), "index:update --path %s", fixture);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);

    if (json) {
        cJSON *added = cJSON_GetObjectItemCaseSensitive(json, "added");
        cJSON *changed = cJSON_GetObjectItemCaseSensitive(json, "changed");
        cJSON *deleted = cJSON_GetObjectItemCaseSensitive(json, "deleted");
        int total = 0;
        if (cJSON_IsNumber(added)) total += added->valueint;
        if (cJSON_IsNumber(changed)) total += changed->valueint;
        if (cJSON_IsNumber(deleted)) total += deleted->valueint;
        TT_ASSERT_EQ_INT(0, total);
        cJSON_Delete(json);
    }

    clean_index(fixture);
}

void run_e2e_index_tests(void)
{
    TT_RUN(test_e2e_index_create_succeeds);
    TT_RUN(test_e2e_index_create_empty_dir_fails);
    TT_RUN(test_e2e_index_update_no_index_fails);
    TT_RUN(test_e2e_index_update_no_changes);
}
