/*
 * test_int_platform.c -- Integration tests for the platform module.
 *
 * Tests that require filesystem I/O, process execution, or system queries.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "platform.h"
#include "error.h"
#include "fast_hash.h"

#include <string.h>

/* ---- realpath ---- */

TT_TEST(test_realpath_known)
{
    char *r = tt_realpath("/tmp");
    TT_ASSERT_NOT_NULL(r);
    TT_ASSERT(r[0] == '/', "should be absolute");
    free(r);
}

TT_TEST(test_realpath_nonexistent)
{
    char *r = tt_realpath("/nonexistent_path_xyz_123");
    TT_ASSERT_NULL(r);
    TT_ASSERT(strlen(tt_error_get()) > 0, "should set error");
    tt_error_clear();
}

/* ---- getcwd ---- */

TT_TEST(test_getcwd)
{
    char *cwd = tt_getcwd();
    TT_ASSERT_NOT_NULL(cwd);
    TT_ASSERT(cwd[0] == '/', "should be absolute");
    free(cwd);
}

/* ---- file_exists / is_dir / is_file ---- */

TT_TEST(test_file_ops)
{
    TT_ASSERT_TRUE(tt_file_exists("/tmp"));
    TT_ASSERT_TRUE(tt_is_dir("/tmp"));
    TT_ASSERT_FALSE(tt_is_file("/tmp"));
    TT_ASSERT_FALSE(tt_file_exists("/nonexistent_xyz_123"));
}

/* ---- read/write file ---- */

TT_TEST(test_read_write_file)
{
    const char *path = "/tmp/tt_test_rw.txt";
    const char *content = "Hello TokToken!";

    int rc = tt_write_file(path, content, strlen(content));
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_TRUE(tt_is_file(path));

    size_t len = 0;
    char *data = tt_read_file(path, &len);
    TT_ASSERT_NOT_NULL(data);
    TT_ASSERT_EQ_INT((int)len, (int)strlen(content));
    TT_ASSERT_EQ_STR(data, content);
    free(data);

    tt_remove_file(path);
    TT_ASSERT_FALSE(tt_file_exists(path));
}

/* ---- mkdir_p ---- */

TT_TEST(test_mkdir_p)
{
    const char *dir = "/tmp/tt_test_mkdir/sub/deep";
    int rc = tt_mkdir_p(dir);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_TRUE(tt_is_dir(dir));

    tt_remove_dir("/tmp/tt_test_mkdir/sub/deep");
    tt_remove_dir("/tmp/tt_test_mkdir/sub");
    tt_remove_dir("/tmp/tt_test_mkdir");
}

/* ---- walk_dir ---- */

typedef struct {
    int file_count;
    int dir_count;
} walk_counter_t;

static int walk_count_cb(const char *dir, const char *name,
                          bool is_dir, bool is_symlink, void *userdata)
{
    (void)dir; (void)name; (void)is_symlink;
    walk_counter_t *c = userdata;
    if (is_dir) c->dir_count++;
    else c->file_count++;
    return 0;
}

TT_TEST(test_walk_dir)
{
    tt_mkdir_p("/tmp/tt_walk_test/sub");
    tt_write_file("/tmp/tt_walk_test/a.txt", "a", 1);
    tt_write_file("/tmp/tt_walk_test/b.txt", "b", 1);
    tt_write_file("/tmp/tt_walk_test/sub/c.txt", "c", 1);

    walk_counter_t counter = {0, 0};
    int rc = tt_walk_dir("/tmp/tt_walk_test", walk_count_cb, &counter);
    TT_ASSERT_EQ_INT(rc, 0);
    TT_ASSERT_EQ_INT(counter.dir_count, 1);
    TT_ASSERT_EQ_INT(counter.file_count, 3);

    tt_remove_file("/tmp/tt_walk_test/a.txt");
    tt_remove_file("/tmp/tt_walk_test/b.txt");
    tt_remove_file("/tmp/tt_walk_test/sub/c.txt");
    tt_remove_dir("/tmp/tt_walk_test/sub");
    tt_remove_dir("/tmp/tt_walk_test");
}

/* ---- proc_run ---- */

TT_TEST(test_proc_run_echo)
{
    const char *argv[] = {"echo", "hello world", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 0);
    TT_ASSERT_EQ_INT(r.exit_code, 0);
    TT_ASSERT_NOT_NULL(r.stdout_buf);
    TT_ASSERT_STR_CONTAINS(r.stdout_buf, "hello world");
    tt_proc_result_free(&r);
}

TT_TEST(test_proc_run_exit_code)
{
    const char *argv[] = {"false", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 0);
    TT_ASSERT(r.exit_code != 0, "non-zero exit code");
    tt_proc_result_free(&r);
}

/* ---- home_dir ---- */

TT_TEST(test_home_dir)
{
    const char *home = tt_home_dir();
    TT_ASSERT_NOT_NULL(home);
    TT_ASSERT(home[0] == '/', "absolute path");
}

/* ---- which ---- */

TT_TEST(test_which)
{
    char *path = tt_which("ls");
    TT_ASSERT_NOT_NULL(path);
    TT_ASSERT(path[0] == '/', "absolute path");
    free(path);

    path = tt_which("nonexistent_binary_xyz_123");
    TT_ASSERT_NULL(path);
}

/* ---- fast_hash_file ---- */

TT_TEST(test_fast_hash_file)
{
    const char *path = "/tmp/tt_hash_test.txt";
    tt_write_file(path, "abc", 3);

    char *hex = tt_fast_hash_file(path);
    TT_ASSERT_NOT_NULL(hex);
    /* XXH3_64bits produces a 16-char hex string */
    TT_ASSERT_EQ_INT((int)strlen(hex), 16);
    free(hex);
    tt_remove_file(path);
}

void run_int_platform_tests(void)
{
    TT_RUN(test_realpath_known);
    TT_RUN(test_realpath_nonexistent);
    TT_RUN(test_getcwd);
    TT_RUN(test_file_ops);
    TT_RUN(test_read_write_file);
    TT_RUN(test_mkdir_p);
    TT_RUN(test_walk_dir);
    TT_RUN(test_proc_run_echo);
    TT_RUN(test_proc_run_exit_code);
    TT_RUN(test_home_dir);
    TT_RUN(test_which);
    TT_RUN(test_fast_hash_file);
}
