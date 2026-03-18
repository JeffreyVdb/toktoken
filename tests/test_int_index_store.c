/*
 * test_int_index_store.c -- Integration tests for index_store module.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "index_store.h"
#include "database.h"
#include "symbol.h"
#include "symbol_kind.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static tt_database_t db;
static tt_index_store_t store;
static char *test_db_path;

static int setup_db(void)
{
    char *tmpdir = tt_test_tmpdir();
    if (!tmpdir) return -1;

    char path[512];
    snprintf(path, sizeof(path), "%s/test.db", tmpdir);
    test_db_path = strdup(path);

    int rc = tt_database_open(&db, path);
    if (rc != 0) { free(tmpdir); return -1; }

    rc = tt_store_init(&store, &db);
    if (rc != 0) { tt_database_close(&db); free(tmpdir); return -1; }

    free(tmpdir);
    return 0;
}

static void teardown_db(void)
{
    tt_store_close(&store);
    tt_database_close(&db);
    if (test_db_path) {
        unlink(test_db_path);
        /* Remove parent tmpdir */
        char *slash = strrchr(test_db_path, '/');
        if (slash) {
            *slash = '\0';
            tt_test_rmdir(test_db_path);
            *slash = '/';
        }
        free(test_db_path);
        test_db_path = NULL;
    }
}

static tt_symbol_t make_test_symbol(const char *id, const char *name,
                                       const char *kind_str, const char *file,
                                       int line)
{
    tt_symbol_t sym;
    memset(&sym, 0, sizeof(sym));
    sym.id = strdup(id);
    sym.name = strdup(name);
    sym.kind = tt_kind_from_ctags(kind_str);
    sym.file = strdup(file);
    sym.language = strdup("php");
    sym.line = line;
    sym.end_line = line + 5;
    sym.byte_offset = 0;
    sym.byte_length = 100;
    sym.qualified_name = strdup(name);
    sym.signature = strdup("");
    sym.docstring = strdup("");
    sym.content_hash = strdup("abc123");
    sym.summary = strdup("");
    sym.keywords = strdup("[]");
    sym.decorators = strdup("[]");
    sym.parent_id = NULL;
    return sym;
}

TT_TEST(test_int_store_insert_and_get_file)
{
    if (setup_db() != 0) return;

    int rc = tt_store_insert_file(&store, "src/App.php", "hash123", "php", 1024, 0, 0, "");
    TT_ASSERT_EQ_INT(0, rc);

    tt_file_record_t rec;
    rc = tt_store_get_file(&store, "src/App.php", &rec);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT_EQ_STR("src/App.php", rec.path);
    TT_ASSERT_EQ_STR("hash123", rec.hash);
    TT_ASSERT_EQ_STR("php", rec.language);

    tt_file_record_free(&rec);
    teardown_db();
}

TT_TEST(test_int_store_insert_and_search_symbol)
{
    if (setup_db() != 0) return;

    tt_store_insert_file(&store, "src/App.php", "hash1", "php", 100, 0, 0, "");

    tt_symbol_t sym = make_test_symbol(
        "src/App.php::App#class", "App", "class", "src/App.php", 10);
    int rc = tt_store_insert_symbol(&store, &sym);
    TT_ASSERT_EQ_INT(0, rc);

    tt_search_results_t results = {0};
    rc = tt_store_search_symbols(&store, "App", NULL, 0, NULL, NULL, 50, false, &results);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT(results.count > 0, "should find App symbol");

    int found = 0;
    for (int i = 0; i < results.count; i++) {
        if (strcmp(results.results[i].sym.name, "App") == 0)
            found = 1;
    }
    TT_ASSERT(found, "results should contain App");

    tt_search_results_free(&results);
    tt_symbol_free(&sym);
    teardown_db();
}

TT_TEST(test_int_store_delete_file_cascades)
{
    if (setup_db() != 0) return;

    tt_store_insert_file(&store, "src/Del.php", "hash2", "php", 200, 0, 0, "");

    tt_symbol_t sym = make_test_symbol(
        "src/Del.php::Foo#class", "Foo", "class", "src/Del.php", 1);
    tt_store_insert_symbol(&store, &sym);

    tt_store_delete_symbols_by_file(&store, "src/Del.php");
    tt_store_delete_file(&store, "src/Del.php");

    tt_file_record_t rec;
    int rc = tt_store_get_file(&store, "src/Del.php", &rec);
    TT_ASSERT(rc != 0, "file should not exist after delete");

    tt_symbol_free(&sym);
    teardown_db();
}

TT_TEST(test_int_store_metadata)
{
    if (setup_db() != 0) return;

    tt_store_set_metadata(&store, "project_root", "/tmp/test");
    char *val = tt_store_get_metadata(&store, "project_root");
    TT_ASSERT_NOT_NULL(val);
    TT_ASSERT_EQ_STR("/tmp/test", val);
    free(val);

    /* Non-existent key returns NULL */
    char *val2 = tt_store_get_metadata(&store, "nonexistent");
    TT_ASSERT_NULL(val2);

    teardown_db();
}

TT_TEST(test_int_store_stats)
{
    if (setup_db() != 0) return;

    tt_store_insert_file(&store, "src/A.php", "h1", "php", 100, 0, 0, "");
    tt_store_insert_file(&store, "src/B.php", "h2", "php", 200, 0, 0, "");

    tt_symbol_t sym1 = make_test_symbol(
        "src/A.php::A#class", "A", "class", "src/A.php", 1);
    tt_symbol_t sym2 = make_test_symbol(
        "src/B.php::B#function", "B", "function", "src/B.php", 1);

    tt_store_insert_symbol(&store, &sym1);
    tt_store_insert_symbol(&store, &sym2);

    tt_stats_t stats;
    int rc = tt_store_get_stats(&store, &stats);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT_EQ_INT(2, stats.files);
    TT_ASSERT_EQ_INT(2, stats.symbols);

    tt_stats_free(&stats);
    tt_symbol_free(&sym1);
    tt_symbol_free(&sym2);
    teardown_db();
}

TT_TEST(test_int_store_get_symbol_by_id)
{
    if (setup_db() != 0) return;

    tt_store_insert_file(&store, "src/C.php", "h3", "php", 300, 0, 0, "");

    tt_symbol_t sym = make_test_symbol(
        "src/C.php::MyClass#class", "MyClass", "class", "src/C.php", 5);
    tt_store_insert_symbol(&store, &sym);

    tt_symbol_result_t result;
    int rc = tt_store_get_symbol(&store, "src/C.php::MyClass#class", &result);
    TT_ASSERT_EQ_INT(0, rc);
    TT_ASSERT_EQ_STR("MyClass", result.sym.name);

    tt_symbol_result_free(&result);
    tt_symbol_free(&sym);
    teardown_db();
}

void run_int_index_store_tests(void)
{
    TT_RUN(test_int_store_insert_and_get_file);
    TT_RUN(test_int_store_insert_and_search_symbol);
    TT_RUN(test_int_store_delete_file_cascades);
    TT_RUN(test_int_store_metadata);
    TT_RUN(test_int_store_stats);
    TT_RUN(test_int_store_get_symbol_by_id);
}
