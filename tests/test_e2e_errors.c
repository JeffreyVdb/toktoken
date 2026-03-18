/*
 * test_e2e_errors.c -- E2E tests for error paths.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "test_e2e_helpers.h"

#include <stdlib.h>
#include <string.h>

TT_TEST(test_e2e_search_without_index)
{
    char *tmpdir = tt_test_tmpdir();
    if (!tmpdir) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "search:symbols test --path %s", tmpdir);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(1, exit_code);

    if (json) {
        cJSON *err = cJSON_GetObjectItemCaseSensitive(json, "error");
        TT_ASSERT(cJSON_IsString(err), "should have error field");
        cJSON_Delete(json);
    }

    tt_test_rmdir(tmpdir);
    free(tmpdir);
}

TT_TEST(test_e2e_inspect_symbol_nonexistent_id)
{
    const char *fixture = tt_test_fixture("mini-project");
    if (!fixture) return;

    /* Create index first */
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cache:clear --path %s", fixture);
    tt_e2e_run(cmd, NULL);
    snprintf(cmd, sizeof(cmd), "index:create --path %s", fixture);
    tt_e2e_run(cmd, NULL);

    /* Try to inspect nonexistent symbol */
    snprintf(cmd, sizeof(cmd),
             "inspect:symbol \"nonexistent::Foo#class\" --path %s", fixture);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    /* Should return error (exit 1) or partial (exit 2) */
    TT_ASSERT(exit_code >= 1, "should fail for nonexistent symbol");

    cJSON_Delete(json);

    snprintf(cmd, sizeof(cmd), "cache:clear --path %s", fixture);
    tt_e2e_run(cmd, NULL);
}

void run_e2e_errors_tests(void)
{
    TT_RUN(test_e2e_search_without_index);
    TT_RUN(test_e2e_inspect_symbol_nonexistent_id);
}
