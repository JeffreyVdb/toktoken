/*
 * test_int_search.c -- Integration tests for Phase 7: Search Layer.
 *
 * Extracted from test_search.c. Tests text_search, FTS5+scorer, sync summaries.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "text_search.h"
#include "symbol_scorer.h"
#include "summarizer.h"
#include "symbol_kind.h"
#include "str_util.h"
#include "platform.h"
#include "database.h"
#include "index_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== Text Search Integration Tests ========== */

TT_TEST(test_int_text_search_found)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"search_test.txt"};
    const char *queries[] = {"caching"};
    tt_text_search_opts_t opts = {false, false, 100, 0};
    tt_text_results_t results;

    int rc = tt_text_search(fixtures, files, 1, queries, 1, &opts, &results);
    TT_ASSERT(rc == 0, "search returned error");
    TT_ASSERT(results.count == 1, "expected 1 result");
    if (results.count < 1) { tt_text_results_free(&results); return; }
    TT_ASSERT(results.results[0].line == 5, "expected line 5");
    TT_ASSERT(results.results[0].text != NULL &&
              strstr(results.results[0].text, "caching") != NULL,
              "text should contain 'caching'");

    tt_text_results_free(&results);
}

TT_TEST(test_int_text_search_not_found)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"search_test.txt"};
    const char *queries[] = {"zzzznotfoundterm"};
    tt_text_search_opts_t opts = {false, false, 100, 0};
    tt_text_results_t results;

    int rc = tt_text_search(fixtures, files, 1, queries, 1, &opts, &results);
    TT_ASSERT(rc == 0, "search returned error");
    TT_ASSERT(results.count == 0, "expected 0 results");

    tt_text_results_free(&results);
}

TT_TEST(test_int_text_search_case_insensitive)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"search_test.txt"};
    const char *queries[] = {"DATABASE"};
    tt_text_search_opts_t opts = {false, false, 100, 0};
    tt_text_results_t results;

    int rc = tt_text_search(fixtures, files, 1, queries, 1, &opts, &results);
    TT_ASSERT(rc == 0, "search returned error");
    /* "DATABASE" appears on line 4, and "database" on line 6 */
    TT_ASSERT(results.count >= 1,
              "expected at least 1 result for case-insensitive 'DATABASE'");

    tt_text_results_free(&results);
}

TT_TEST(test_int_text_search_case_sensitive)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"search_test.txt"};
    const char *queries[] = {"DATABASE"};
    tt_text_search_opts_t opts = {true, false, 100, 0};
    tt_text_results_t results;

    int rc = tt_text_search(fixtures, files, 1, queries, 1, &opts, &results);
    TT_ASSERT(rc == 0, "search returned error");
    /* Only line 4 has "DATABASE" in uppercase */
    TT_ASSERT(results.count == 1,
              "expected exactly 1 result for case-sensitive 'DATABASE'");
    if (results.count < 1) { tt_text_results_free(&results); return; }
    TT_ASSERT(results.results[0].line == 4, "expected line 4");

    tt_text_results_free(&results);
}

TT_TEST(test_int_text_search_context)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"search_test.txt"};
    const char *queries[] = {"caching"};
    tt_text_search_opts_t opts = {false, false, 100, 2};
    tt_text_results_t results;

    int rc = tt_text_search(fixtures, files, 1, queries, 1, &opts, &results);
    TT_ASSERT(rc == 0, "search returned error");
    TT_ASSERT(results.count >= 1, "expected at least 1 result");
    if (results.count < 1) { tt_text_results_free(&results); return; }

    /* If we got context (manual mode), verify it. Ripgrep ignores context,
     * so we just check the main result exists. */
    tt_text_result_t *r = &results.results[0];
    TT_ASSERT(r->line == 5, "expected line 5");

    /* If context was collected (manual mode), check before/after */
    if (r->before_count > 0) {
        TT_ASSERT(r->before_count <= 2, "expected at most 2 before lines");
    }
    if (r->after_count > 0) {
        TT_ASSERT(r->after_count <= 2, "expected at most 2 after lines");
    }

    tt_text_results_free(&results);
}

TT_TEST(test_int_text_search_truncation)
{
    /* Create a temp file with a very long line */
    char *tmpdir = tt_test_tmpdir();
    TT_ASSERT(tmpdir != NULL, "failed to create tmpdir");
    if (!tmpdir) return;

    char long_line[310];
    memset(long_line, 'x', 250);
    memcpy(long_line, "FINDME", 6);
    long_line[250] = '\n';
    long_line[251] = '\0';

    int wrc = tt_test_write_file(tmpdir, "search_long.txt", long_line);
    TT_ASSERT(wrc == 0, "failed to write temp file");

    /* Use tmpdir as the base_dir, file is relative "search_long.txt" */
    const char *files[] = {"search_long.txt"};
    const char *queries[] = {"FINDME"};
    tt_text_search_opts_t opts = {true, false, 100, 0};
    tt_text_results_t results;

    int rc = tt_text_search(tmpdir, files, 1, queries, 1, &opts, &results);
    TT_ASSERT(rc == 0, "search returned error");
    TT_ASSERT(results.count == 1, "expected 1 result");

    if (results.count >= 1) {
        size_t text_len = tt_utf8_strlen(results.results[0].text);
        TT_ASSERT(text_len <= 203,
                  "expected text <= 203 chars (200 + ...)");
        TT_ASSERT(tt_str_ends_with(results.results[0].text, "..."),
                  "expected '...' suffix");
    }

    tt_text_results_free(&results);
    tt_test_rmdir(tmpdir);
    free(tmpdir);
}

TT_TEST(test_int_text_search_multiple_queries)
{
    const char *fixtures = tt_test_fixtures_dir();
    if (!fixtures) return;

    const char *files[] = {"search_test.txt"};
    const char *queries[] = {"caching", "middleware"};
    tt_text_search_opts_t opts = {false, false, 100, 0};
    tt_text_results_t results;

    int rc = tt_text_search(fixtures, files, 1, queries, 2, &opts, &results);
    TT_ASSERT(rc == 0, "search returned error");
    /* "caching" on line 5, "middleware" on line 8 */
    TT_ASSERT(results.count >= 2,
              "expected at least 2 results for OR query");

    tt_text_results_free(&results);
}

/* ========== FTS5 Integration Test ========== */

TT_TEST(test_int_fts5_with_scorer)
{
    char *tmpdir = tt_test_tmpdir();
    TT_ASSERT(tmpdir != NULL, "failed to create tmpdir");
    if (!tmpdir) return;

    tt_database_t db;
    int rc = tt_database_open(&db, tmpdir);
    if (rc < 0) {
        TT_ASSERT(0, "could not open temp database");
        tt_test_rmdir(tmpdir); free(tmpdir); return;
    }

    tt_index_store_t store;
    rc = tt_store_init(&store, &db);
    if (rc < 0) {
        tt_database_close(&db);
        tt_test_rmdir(tmpdir); free(tmpdir);
        TT_ASSERT(0, "could not init store"); return;
    }

    /* Insert test symbols */
    tt_symbol_t sym1 = {0};
    sym1.id = tt_strdup("test.c::AuthService#class");
    sym1.file = tt_strdup("test.c");
    sym1.name = tt_strdup("AuthService");
    sym1.qualified_name = tt_strdup("AuthService");
    sym1.kind = TT_KIND_CLASS;
    sym1.language = tt_strdup("c");
    sym1.signature = tt_strdup("class AuthService");
    sym1.docstring = tt_strdup("");
    sym1.summary = tt_strdup("");
    sym1.decorators = tt_strdup("[]");
    sym1.keywords = tt_strdup("[\"auth\",\"service\"]");
    sym1.content_hash = tt_strdup("abc123");
    sym1.line = 1;
    sym1.end_line = 50;

    tt_symbol_t sym2 = {0};
    sym2.id = tt_strdup("test.c::getUser#function");
    sym2.file = tt_strdup("test.c");
    sym2.name = tt_strdup("getUser");
    sym2.qualified_name = tt_strdup("getUser");
    sym2.kind = TT_KIND_FUNCTION;
    sym2.language = tt_strdup("c");
    sym2.signature = tt_strdup("User* getUser(int id)");
    sym2.docstring = tt_strdup("");
    sym2.summary = tt_strdup("");
    sym2.decorators = tt_strdup("[]");
    sym2.keywords = tt_strdup("[\"get\",\"user\"]");
    sym2.content_hash = tt_strdup("def456");
    sym2.line = 55;
    sym2.end_line = 70;

    tt_store_insert_file(&store, "test.c", "hash1", "c", 1000, 0, 0, "");
    tt_store_insert_symbol(&store, &sym1);
    tt_store_insert_symbol(&store, &sym2);

    /* Search for "auth" */
    tt_search_results_t results;
    rc = tt_store_search_symbols(&store, "auth", NULL, 0, NULL, NULL,
                                 50, false, &results);
    TT_ASSERT(rc == 0, "search failed");
    TT_ASSERT(results.count >= 1,
              "expected at least 1 result for 'auth'");

    /* AuthService should score higher (name contains "auth") */
    bool found_auth = false;
    for (int i = 0; i < results.count; i++) {
        if (strcmp(results.results[i].sym.name, "AuthService") == 0) {
            found_auth = true;
            TT_ASSERT(results.results[i].score >= 10,
                      "expected AuthService score >= 10");
        }
    }
    TT_ASSERT(found_auth, "AuthService not found in results");

    tt_search_results_free(&results);

    /* Cleanup */
    tt_symbol_free(&sym1);
    tt_symbol_free(&sym2);
    tt_store_close(&store);
    tt_database_close(&db);
    tt_test_rmdir(tmpdir);
    free(tmpdir);
}

/* ========== Sync Summaries Test ========== */

TT_TEST(test_int_sync_summaries)
{
    char *tmpdir = tt_test_tmpdir();
    TT_ASSERT(tmpdir != NULL, "failed to create tmpdir");
    if (!tmpdir) return;

    tt_database_t db;
    int rc = tt_database_open(&db, tmpdir);
    if (rc < 0) {
        tt_test_rmdir(tmpdir); free(tmpdir);
        TT_ASSERT(0, "could not open temp database"); return;
    }

    tt_index_store_t store;
    rc = tt_store_init(&store, &db);
    if (rc < 0) {
        tt_database_close(&db);
        tt_test_rmdir(tmpdir); free(tmpdir);
        TT_ASSERT(0, "could not init store"); return;
    }

    /* Insert symbol with docstring (should get tier 1 summary) */
    tt_symbol_t sym1 = {0};
    sym1.id = tt_strdup("test.c::Foo#class");
    sym1.file = tt_strdup("test.c");
    sym1.name = tt_strdup("Foo");
    sym1.qualified_name = tt_strdup("Foo");
    sym1.kind = TT_KIND_CLASS;
    sym1.language = tt_strdup("c");
    sym1.signature = tt_strdup("class Foo");
    sym1.docstring = tt_strdup("Manages user sessions. Provides login and logout.");
    sym1.summary = tt_strdup("");
    sym1.decorators = tt_strdup("[]");
    sym1.keywords = tt_strdup("[]");
    sym1.content_hash = tt_strdup("h1");
    sym1.line = 1;
    sym1.end_line = 10;

    /* Insert symbol without docstring (should get tier 3 summary) */
    tt_symbol_t sym2 = {0};
    sym2.id = tt_strdup("test.c::bar#function");
    sym2.file = tt_strdup("test.c");
    sym2.name = tt_strdup("bar");
    sym2.qualified_name = tt_strdup("bar");
    sym2.kind = TT_KIND_FUNCTION;
    sym2.language = tt_strdup("c");
    sym2.signature = tt_strdup("void bar()");
    sym2.docstring = tt_strdup("");
    sym2.summary = tt_strdup("");
    sym2.decorators = tt_strdup("[]");
    sym2.keywords = tt_strdup("[]");
    sym2.content_hash = tt_strdup("h2");
    sym2.line = 15;
    sym2.end_line = 20;

    tt_store_insert_file(&store, "test.c", "hash1", "c", 500, 0, 0, "");
    tt_store_insert_symbol(&store, &sym1);
    tt_store_insert_symbol(&store, &sym2);
    tt_store_close(&store);

    /* Apply sync summaries */
    rc = tt_apply_sync_summaries(&db);
    TT_ASSERT(rc == 0, "sync summaries failed");

    /* Verify summaries were applied */
    rc = tt_store_init(&store, &db);
    TT_ASSERT(rc == 0, "store re-init failed");
    if (rc != 0) {
        tt_symbol_free(&sym1); tt_symbol_free(&sym2);
        tt_database_close(&db);
        tt_test_rmdir(tmpdir); free(tmpdir); return;
    }

    tt_symbol_result_t r1;
    rc = tt_store_get_symbol(&store, "test.c::Foo#class", &r1);
    TT_ASSERT(rc == 0, "get Foo failed");
    if (rc == 0) {
        TT_ASSERT(r1.sym.summary && r1.sym.summary[0],
                  "Foo should have summary");
        if (r1.sym.summary && r1.sym.summary[0]) {
            TT_ASSERT_EQ_STR(r1.sym.summary, "Manages user sessions.");
        }
        tt_symbol_result_free(&r1);
    }

    tt_symbol_result_t r2;
    rc = tt_store_get_symbol(&store, "test.c::bar#function", &r2);
    TT_ASSERT(rc == 0, "get bar failed");
    if (rc == 0) {
        TT_ASSERT(r2.sym.summary && r2.sym.summary[0],
                  "bar should have summary");
        if (r2.sym.summary && r2.sym.summary[0]) {
            TT_ASSERT_EQ_STR(r2.sym.summary, "Function bar");
        }
        tt_symbol_result_free(&r2);
    }

    /* Cleanup */
    tt_symbol_free(&sym1);
    tt_symbol_free(&sym2);
    tt_store_close(&store);
    tt_database_close(&db);
    tt_test_rmdir(tmpdir);
    free(tmpdir);
}

/* ========== Runner ========== */

void run_int_search_tests(void)
{
    TT_SUITE("Integration: Search Layer");

    TT_RUN(test_int_text_search_found);
    TT_RUN(test_int_text_search_not_found);
    TT_RUN(test_int_text_search_case_insensitive);
    TT_RUN(test_int_text_search_case_sensitive);
    TT_RUN(test_int_text_search_context);
    TT_RUN(test_int_text_search_truncation);
    TT_RUN(test_int_text_search_multiple_queries);
    TT_RUN(test_int_fts5_with_scorer);
    TT_RUN(test_int_sync_summaries);
}
