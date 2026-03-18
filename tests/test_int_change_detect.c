/*
 * test_int_change_detect.c -- Integration tests for fast change detection.
 *
 * Tests tt_store_detect_changes_fast() and tt_store_update_file_metadata().
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "database.h"
#include "index_store.h"
#include "platform.h"
#include "str_util.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- Helpers ---- */

static char *s_tmpdir;

static void cd_setup(void)
{
    s_tmpdir = tt_test_tmpdir();
}

static void cd_cleanup(void)
{
    if (s_tmpdir)
    {
        tt_test_rmdir(s_tmpdir);
        free(s_tmpdir);
        s_tmpdir = NULL;
    }
}

/*
 * Create a real file in s_tmpdir and insert it into the store with
 * correct mtime from stat(). Returns 0 on success.
 */
static int insert_real_file(tt_index_store_t *store, const char *relpath,
                            const char *content, const char *hash)
{
    if (tt_test_write_file(s_tmpdir, relpath, content) < 0)
        return -1;

    char *full = tt_path_join(s_tmpdir, relpath);
    if (!full)
        return -1;

    struct stat st;
    if (stat(full, &st) < 0)
    {
        free(full);
        return -1;
    }
    free(full);

    return tt_store_insert_file(store, relpath, hash, "c",
                                (int64_t)st.st_size,
                                TT_ST_MTIME_SEC(st),
                                TT_ST_MTIME_NSEC(st), "");
}

/* ---- Tests ---- */

TT_TEST(test_fast_detect_no_changes)
{
    cd_setup();

    tt_database_t db;
    int rc = tt_database_open(&db, s_tmpdir);
    TT_ASSERT_EQ_INT(rc, 0);

    tt_index_store_t store;
    rc = tt_store_init(&store, &db);
    TT_ASSERT_EQ_INT(rc, 0);

    /* Insert a real file */
    rc = insert_real_file(&store, "main.c", "int main() { return 0; }\n", "h1");
    TT_ASSERT_EQ_INT(rc, 0);

    /* Detect changes immediately -- file is unchanged */
    const char *paths[] = {"main.c"};
    tt_changes_fast_t changes;
    rc = tt_store_detect_changes_fast(&store, s_tmpdir, paths, 1, &changes);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(changes.changed_count, 0);
    TT_ASSERT_EQ_INT(changes.added_count, 0);
    TT_ASSERT_EQ_INT(changes.deleted_count, 0);
    /* metadata_changed may be 0 or 1 depending on timing */

    tt_changes_fast_free(&changes);
    tt_store_close(&store);
    tt_database_close(&db);
    cd_cleanup();
}

TT_TEST(test_fast_detect_added_file)
{
    cd_setup();

    tt_database_t db;
    int rc = tt_database_open(&db, s_tmpdir);
    TT_ASSERT_EQ_INT(rc, 0);

    tt_index_store_t store;
    rc = tt_store_init(&store, &db);
    TT_ASSERT_EQ_INT(rc, 0);

    /* Insert file A into store */
    rc = insert_real_file(&store, "a.c", "void a() {}\n", "ha");
    TT_ASSERT_EQ_INT(rc, 0);

    /* Create file B on disk but don't add to store */
    tt_test_write_file(s_tmpdir, "b.c", "void b() {}\n");

    /* Detect: b.c should show as added */
    const char *paths[] = {"a.c", "b.c"};
    tt_changes_fast_t changes;
    rc = tt_store_detect_changes_fast(&store, s_tmpdir, paths, 2, &changes);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(changes.added_count, 1);
    TT_ASSERT_EQ_STR(changes.added[0], "b.c");

    tt_changes_fast_free(&changes);
    tt_store_close(&store);
    tt_database_close(&db);
    cd_cleanup();
}

TT_TEST(test_fast_detect_deleted_file)
{
    cd_setup();

    tt_database_t db;
    int rc = tt_database_open(&db, s_tmpdir);
    TT_ASSERT_EQ_INT(rc, 0);

    tt_index_store_t store;
    rc = tt_store_init(&store, &db);
    TT_ASSERT_EQ_INT(rc, 0);

    /* Insert two files */
    rc = insert_real_file(&store, "keep.c", "void keep() {}\n", "hk");
    TT_ASSERT_EQ_INT(rc, 0);
    rc = insert_real_file(&store, "gone.c", "void gone() {}\n", "hg");
    TT_ASSERT_EQ_INT(rc, 0);

    /* Only report keep.c in discovered paths -- gone.c is "deleted" */
    const char *paths[] = {"keep.c"};
    tt_changes_fast_t changes;
    rc = tt_store_detect_changes_fast(&store, s_tmpdir, paths, 1, &changes);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(changes.deleted_count, 1);
    TT_ASSERT_EQ_STR(changes.deleted[0], "gone.c");

    tt_changes_fast_free(&changes);
    tt_store_close(&store);
    tt_database_close(&db);
    cd_cleanup();
}

TT_TEST(test_fast_detect_changed_file)
{
    cd_setup();

    tt_database_t db;
    int rc = tt_database_open(&db, s_tmpdir);
    TT_ASSERT_EQ_INT(rc, 0);

    tt_index_store_t store;
    rc = tt_store_init(&store, &db);
    TT_ASSERT_EQ_INT(rc, 0);

    /* Insert file with known content */
    rc = insert_real_file(&store, "mod.c", "/* original */\n", "orig_hash");
    TT_ASSERT_EQ_INT(rc, 0);

    /* Modify the file on disk (different content, different mtime) */
    usleep(50000); /* 50ms to ensure mtime differs */
    tt_test_write_file(s_tmpdir, "mod.c", "/* modified content that is different */\n");

    const char *paths[] = {"mod.c"};
    tt_changes_fast_t changes;
    rc = tt_store_detect_changes_fast(&store, s_tmpdir, paths, 1, &changes);
    TT_ASSERT_EQ_INT(rc, 0);

    /* File should be detected as changed (content hash differs) */
    TT_ASSERT_EQ_INT(changes.changed_count, 1);
    TT_ASSERT_EQ_STR(changes.changed[0], "mod.c");

    tt_changes_fast_free(&changes);
    tt_store_close(&store);
    tt_database_close(&db);
    cd_cleanup();
}

TT_TEST(test_update_file_metadata)
{
    cd_setup();

    tt_database_t db;
    int rc = tt_database_open(&db, s_tmpdir);
    TT_ASSERT_EQ_INT(rc, 0);

    tt_index_store_t store;
    rc = tt_store_init(&store, &db);
    TT_ASSERT_EQ_INT(rc, 0);

    /* Insert a file with mtime 0 */
    rc = tt_store_insert_file(&store, "upd.c", "h1", "c", 100, 0, 0, "");
    TT_ASSERT_EQ_INT(rc, 0);

    /* Update metadata */
    rc = tt_store_update_file_metadata(&store, "upd.c", 1000, 500, 200);
    TT_ASSERT_EQ_INT(rc, 0);

    /* Create a real file so detect_changes_fast can stat it */
    tt_test_write_file(s_tmpdir, "upd.c", "/* content */\n");

    /* Verify via detect_changes_fast that the stored metadata differs
     * from the actual file (since we set artificial values).
     * The file should show as changed or metadata_changed. */
    const char *paths[] = {"upd.c"};
    tt_changes_fast_t changes;
    rc = tt_store_detect_changes_fast(&store, s_tmpdir, paths, 1, &changes);
    TT_ASSERT_EQ_INT(rc, 0);

    /* The file has artificial metadata (1000,500,200) which won't match
     * the real stat values, so it should appear in changed or metadata_changed */
    int total_detected = changes.changed_count + changes.metadata_changed_count;
    TT_ASSERT(total_detected >= 1, "file should be detected as modified");

    tt_changes_fast_free(&changes);
    tt_store_close(&store);
    tt_database_close(&db);
    cd_cleanup();
}

/* ---- Runner ---- */

void run_int_change_detect_tests(void)
{
    TT_RUN(test_fast_detect_no_changes);
    TT_RUN(test_fast_detect_added_file);
    TT_RUN(test_fast_detect_deleted_file);
    TT_RUN(test_fast_detect_changed_file);
    TT_RUN(test_update_file_metadata);
}
