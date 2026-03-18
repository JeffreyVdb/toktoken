/*
 * test_e2e_search.c -- E2E tests for search:symbols and search:text.
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

    /* Clean + create index */
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

TT_TEST(test_e2e_search_symbols_finds_app)
{
    ensure_index();
    if (!indexed) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "search:symbols App --path %s", fixture_path);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);
    TT_ASSERT_NOT_NULL(json);

    if (json) {
        cJSON *n = cJSON_GetObjectItemCaseSensitive(json, "n");
        TT_ASSERT(cJSON_IsNumber(n) && n->valueint > 0,
                   "should find at least one symbol");
        cJSON_Delete(json);
    }

    cleanup_index();
}

TT_TEST(test_e2e_search_symbols_count)
{
    ensure_index();
    if (!indexed) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "search:symbols App --count --path %s", fixture_path);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);

    if (json) {
        cJSON *count = cJSON_GetObjectItemCaseSensitive(json, "count");
        TT_ASSERT(cJSON_IsNumber(count), "should have count field");
        cJSON_Delete(json);
    }

    cleanup_index();
}

TT_TEST(test_e2e_search_text_finds_content)
{
    ensure_index();
    if (!indexed) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "search:text class --path %s", fixture_path);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);
    TT_ASSERT_NOT_NULL(json);

    if (json) {
        cJSON *n = cJSON_GetObjectItemCaseSensitive(json, "n");
        TT_ASSERT(cJSON_IsNumber(n) && n->valueint > 0,
                   "should find text results");
        cJSON_Delete(json);
    }

    cleanup_index();
}

TT_TEST(test_e2e_search_text_group_by_file)
{
    ensure_index();
    if (!indexed) return;

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "search:text class --group-by file --path %s", fixture_path);
    cJSON *json = NULL;
    int exit_code = tt_e2e_run(cmd, &json);

    TT_ASSERT_EQ_INT(0, exit_code);

    if (json) {
        cJSON *groups = cJSON_GetObjectItemCaseSensitive(json, "groups");
        TT_ASSERT(groups != NULL, "should have groups field");
        cJSON_Delete(json);
    }

    cleanup_index();
}

void run_e2e_search_tests(void)
{
    TT_RUN(test_e2e_search_symbols_finds_app);
    TT_RUN(test_e2e_search_symbols_count);
    TT_RUN(test_e2e_search_text_finds_content);
    TT_RUN(test_e2e_search_text_group_by_file);
}
