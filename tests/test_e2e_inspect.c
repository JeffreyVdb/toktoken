/*
 * test_e2e_inspect.c -- E2E tests for inspect:outline, inspect:symbol, inspect:file.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "test_e2e_helpers.h"

#include <stdlib.h>
#include <string.h>

static const char *fixture_path;
static int indexed;

static void ensure_index(void)
{
    fixture_path = tt_test_fixture("mini-project");
    if (!fixture_path) { indexed = 0; return; }

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "cache:clear --path %s", fixture_path);
    tt_e2e_run(cmd, NULL);

    snprintf(cmd, sizeof(cmd), "index:create --path %s", fixture_path);
    cJSON *json = NULL;
    int rc = tt_e2e_run(cmd, &json);
    indexed = (rc == 0);
    cJSON_Delete(json);
}

static void cleanup_index(void)
{
    if (fixture_path) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "cache:clear --path %s", fixture_path);
        tt_e2e_run(cmd, NULL);
    }
}

TT_TEST(test_e2e_inspect_outline_returns_symbols)
{
    ensure_index();
    if (!indexed) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "inspect:outline src/App.php --path %s", fixture_path);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);
    TT_ASSERT_NOT_NULL(json);

    cJSON_Delete(json);
    cleanup_index();
}

TT_TEST(test_e2e_inspect_outline_nonexistent_file)
{
    ensure_index();
    if (!indexed) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "inspect:outline nonexistent.php --path %s", fixture_path);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(1, exit_code);

    cJSON_Delete(json);
    cleanup_index();
}

TT_TEST(test_e2e_inspect_file)
{
    ensure_index();
    if (!indexed) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "inspect:file src/App.php --path %s", fixture_path);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);
    TT_ASSERT_NOT_NULL(json);

    cJSON_Delete(json);
    cleanup_index();
}

TT_TEST(test_e2e_inspect_file_with_lines)
{
    ensure_index();
    if (!indexed) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "inspect:file src/App.php --lines 1-10 --path %s", fixture_path);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);
    TT_ASSERT_NOT_NULL(json);

    cJSON_Delete(json);
    cleanup_index();
}

void run_e2e_inspect_tests(void)
{
    TT_RUN(test_e2e_inspect_outline_returns_symbols);
    TT_RUN(test_e2e_inspect_outline_nonexistent_file);
    TT_RUN(test_e2e_inspect_file);
    TT_RUN(test_e2e_inspect_file_with_lines);
}
