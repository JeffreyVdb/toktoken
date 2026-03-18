/*
 * test_e2e_manage.c -- E2E tests for cache:clear, codebase:detect, stats.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "test_e2e_helpers.h"

#include <stdlib.h>
#include <string.h>

TT_TEST(test_e2e_codebase_detect_on_project)
{
    const char *fixture = tt_test_fixture("mini-project");
    if (!fixture) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "codebase:detect --path %s", fixture);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);

    if (json) {
        cJSON *is_cb = cJSON_GetObjectItemCaseSensitive(json, "is_codebase");
        TT_ASSERT(cJSON_IsTrue(is_cb), "should detect as codebase");
        cJSON_Delete(json);
    }
}

TT_TEST(test_e2e_codebase_detect_on_empty_dir)
{
    const char *fixture = tt_test_fixture("empty-dir");
    if (!fixture) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "codebase:detect --path %s", fixture);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(1, exit_code);

    cJSON_Delete(json);
}

TT_TEST(test_e2e_cache_clear_no_index)
{
    const char *fixture = tt_test_fixture("mini-project");
    if (!fixture) return;

    /* Ensure no index */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cache:clear --path %s", fixture);
    tt_e2e_run(cmd, NULL);

    /* Try to clear again */
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(1, exit_code);

    if (json) {
        cJSON *err = cJSON_GetObjectItemCaseSensitive(json, "error");
        TT_ASSERT(cJSON_IsString(err) && strcmp(err->valuestring, "no_index") == 0,
                   "should return no_index error");
        cJSON_Delete(json);
    }
}

TT_TEST(test_e2e_cache_clear_all_requires_force)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cache:clear --all");
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(1, exit_code);

    if (json) {
        cJSON *err = cJSON_GetObjectItemCaseSensitive(json, "error");
        TT_ASSERT(cJSON_IsString(err) &&
                   strcmp(err->valuestring, "confirmation_required") == 0,
                   "should require --force");
        cJSON_Delete(json);
    }
}

TT_TEST(test_e2e_cache_clear_all_force_purge)
{
    const char *fixture = tt_test_fixture("mini-project");
    if (!fixture) return;

    /* Create index first */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "index:create --path %s", fixture);
    cJSON *json = NULL;
    int rc = tt_e2e_run(cmd, &json);
    cJSON_Delete(json);
    if (rc != 0) return;

    /* Purge all */
    snprintf(cmd, sizeof(cmd), "cache:clear --all --force");
    json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);

    if (json) {
        cJSON *purged = cJSON_GetObjectItemCaseSensitive(json, "purged");
        TT_ASSERT(cJSON_IsString(purged), "has purged path");

        cJSON *freed = cJSON_GetObjectItemCaseSensitive(json, "freed_bytes");
        TT_ASSERT(cJSON_IsNumber(freed), "has freed_bytes");

        cJSON_Delete(json);
    }

    /* Verify re-creation works */
    snprintf(cmd, sizeof(cmd), "index:create --path %s", fixture);
    json = NULL;
    rc = tt_e2e_run(cmd, &json);
    TT_ASSERT_EQ_INT(0, rc);
    cJSON_Delete(json);

    /* Cleanup */
    snprintf(cmd, sizeof(cmd), "cache:clear --path %s", fixture);
    tt_e2e_run(cmd, NULL);
}

TT_TEST(test_e2e_stats_after_index)
{
    const char *fixture = tt_test_fixture("mini-project");
    if (!fixture) return;

    /* Create index */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cache:clear --path %s", fixture);
    tt_e2e_run(cmd, NULL);
    snprintf(cmd, sizeof(cmd), "index:create --path %s", fixture);
    cJSON *json = NULL;
    int rc = tt_e2e_run(cmd, &json);
    cJSON_Delete(json);
    if (rc != 0) return;

    /* Run stats */
    snprintf(cmd, sizeof(cmd), "stats --path %s", fixture);
    json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);

    if (json) {
        cJSON *files = cJSON_GetObjectItemCaseSensitive(json, "files");
        TT_ASSERT(cJSON_IsNumber(files) && files->valueint > 0,
                   "stats should show files > 0");
        cJSON_Delete(json);
    }

    /* Cleanup */
    snprintf(cmd, sizeof(cmd), "cache:clear --path %s", fixture);
    tt_e2e_run(cmd, NULL);
}

void run_e2e_manage_tests(void)
{
    TT_RUN(test_e2e_codebase_detect_on_project);
    TT_RUN(test_e2e_codebase_detect_on_empty_dir);
    TT_RUN(test_e2e_cache_clear_no_index);
    TT_RUN(test_e2e_cache_clear_all_requires_force);
    TT_RUN(test_e2e_cache_clear_all_force_purge);
    TT_RUN(test_e2e_stats_after_index);
}
