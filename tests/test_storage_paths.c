/*
 * test_storage_paths.c -- Unit tests for storage_paths module.
 */

#include "test_framework.h"
#include "test_helpers.h"
#include "storage_paths.h"
#include "platform.h"
#include "str_util.h"

#include <stdlib.h>
#include <string.h>
#include "test_compat.h"

TT_TEST(test_sp_base_dir_contains_toktoken)
{
    char *dir = tt_storage_base_dir();
    TT_ASSERT_NOT_NULL(dir);
    TT_ASSERT_STR_CONTAINS(dir, "toktoken");
    free(dir);
}

TT_TEST(test_sp_base_dir_is_absolute)
{
    char *dir = tt_storage_base_dir();
    TT_ASSERT_NOT_NULL(dir);
    TT_ASSERT_TRUE(tt_path_is_absolute(dir));
    free(dir);
}

TT_TEST(test_sp_projects_dir_under_base)
{
    char *base = tt_storage_base_dir();
    char *proj = tt_storage_projects_dir();
    TT_ASSERT_NOT_NULL(base);
    TT_ASSERT_NOT_NULL(proj);
    TT_ASSERT(strstr(proj, base) == proj, "projects_dir should start with base_dir");
    TT_ASSERT_STR_ENDS_WITH(proj, "projects");
    free(base);
    free(proj);
}

TT_TEST(test_sp_project_dir_uses_hash)
{
    char *dir = tt_storage_project_dir("/tmp/test-project");
    TT_ASSERT_NOT_NULL(dir);

    /* Get basename (last component) */
    const char *slash = strrchr(dir, '/');
    const char *hash = slash ? slash + 1 : dir;

    /* Hash should be 12 hex chars */
    TT_ASSERT_EQ_INT(12, (int)strlen(hash));
    for (int i = 0; i < 12; i++) {
        char c = hash[i];
        TT_ASSERT((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'),
                   "hash should be hex chars");
    }
    free(dir);
}

TT_TEST(test_sp_project_dir_deterministic)
{
    char *dir1 = tt_storage_project_dir("/tmp/test-project");
    char *dir2 = tt_storage_project_dir("/tmp/test-project");
    TT_ASSERT_NOT_NULL(dir1);
    TT_ASSERT_NOT_NULL(dir2);
    TT_ASSERT_EQ_STR(dir1, dir2);
    free(dir1);
    free(dir2);
}

TT_TEST(test_sp_different_paths_different_hashes)
{
    char *dir1 = tt_storage_project_dir("/tmp/project-a");
    char *dir2 = tt_storage_project_dir("/tmp/project-b");
    TT_ASSERT_NOT_NULL(dir1);
    TT_ASSERT_NOT_NULL(dir2);
    TT_ASSERT(strcmp(dir1, dir2) != 0, "different paths should produce different dirs");
    free(dir1);
    free(dir2);
}

TT_TEST(test_sp_project_dir_long_path)
{
    char long_path[600];
    memset(long_path, 'a', 500);
    memcpy(long_path, "/tmp/", 5);
    long_path[500] = '\0';

    char *dir = tt_storage_project_dir(long_path);
    TT_ASSERT_NOT_NULL(dir);

    const char *slash = strrchr(dir, '/');
    const char *hash = slash ? slash + 1 : dir;
    TT_ASSERT_EQ_INT(12, (int)strlen(hash));
    free(dir);
}

TT_TEST(test_sp_logs_dir_under_base)
{
    char *base = tt_storage_base_dir();
    char *logs = tt_storage_logs_dir();
    TT_ASSERT_NOT_NULL(base);
    TT_ASSERT_NOT_NULL(logs);
    TT_ASSERT(strstr(logs, base) == logs, "logs_dir should start with base_dir");
    TT_ASSERT_STR_ENDS_WITH(logs, "logs");
    free(base);
    free(logs);
}

/* ================================================================
 * Migration tests: old (.toktoken) → new (toktoken)
 * Each test overrides HOME (POSIX) or USERPROFILE (Windows)
 * to an isolated tmpdir.
 * ================================================================ */

/* Helper: override home dir env for test isolation.
 * On Windows, tt_home_dir() reads USERPROFILE, not HOME. */
static void set_home_env(const char *dir)
{
#ifdef TT_PLATFORM_WINDOWS
    setenv("USERPROFILE", dir, 1);
#else
    setenv("HOME", dir, 1);
#endif
}

static void restore_home_env(const char *orig)
{
#ifdef TT_PLATFORM_WINDOWS
    if (orig) setenv("USERPROFILE", orig, 1);
    else      unsetenv("USERPROFILE");
#else
    if (orig) setenv("HOME", orig, 1);
    else      unsetenv("HOME");
#endif
}

static char *save_home_env(void)
{
#ifdef TT_PLATFORM_WINDOWS
    const char *v = getenv("USERPROFILE");
#else
    const char *v = getenv("HOME");
#endif
    return v ? tt_strdup(v) : NULL;
}

TT_TEST(test_sp_migration_old_to_new)
{
    char *tmpdir = tt_test_tmpdir();
    char old_cache[512];
    snprintf(old_cache, sizeof(old_cache), "%s/.cache/.toktoken", tmpdir);
    tt_mkdir_p(old_cache);
    tt_test_write_file(tmpdir, ".cache/.toktoken/marker.txt", "migrated");

    char *orig_home = save_home_env();
    set_home_env(tmpdir);

    char *base = tt_storage_base_dir();
    TT_ASSERT_NOT_NULL(base);
    TT_ASSERT_STR_CONTAINS(base, "toktoken");
    /* Old directory should be gone */
    TT_ASSERT_FALSE(tt_is_dir(old_cache));
    /* Marker file should exist in new location */
    char *marker = tt_path_join(base, "marker.txt");
    TT_ASSERT_TRUE(tt_file_exists(marker));

    restore_home_env(orig_home);
    free(orig_home);
    free(base);
    free(marker);
    tt_test_rmdir(tmpdir);
    free(tmpdir);
}

TT_TEST(test_sp_migration_already_new)
{
    char *tmpdir = tt_test_tmpdir();
    char new_cache[512];
    snprintf(new_cache, sizeof(new_cache), "%s/.cache/toktoken", tmpdir);
    tt_mkdir_p(new_cache);

    char *orig_home = save_home_env();
    set_home_env(tmpdir);

    char *base = tt_storage_base_dir();
    TT_ASSERT_NOT_NULL(base);
    TT_ASSERT_STR_CONTAINS(base, "toktoken");

    restore_home_env(orig_home);
    free(orig_home);
    free(base);
    tt_test_rmdir(tmpdir);
    free(tmpdir);
}

TT_TEST(test_sp_migration_fresh_install)
{
    char *tmpdir = tt_test_tmpdir();
    /* Create .cache/ but neither .toktoken nor toktoken */
    char dot_cache[512];
    snprintf(dot_cache, sizeof(dot_cache), "%s/.cache", tmpdir);
    tt_mkdir_p(dot_cache);

    char *orig_home = save_home_env();
    set_home_env(tmpdir);

    char *base = tt_storage_base_dir();
    TT_ASSERT_NOT_NULL(base);
    TT_ASSERT_STR_CONTAINS(base, "toktoken");

    restore_home_env(orig_home);
    free(orig_home);
    free(base);
    tt_test_rmdir(tmpdir);
    free(tmpdir);
}

TT_TEST(test_sp_migration_both_exist)
{
    char *tmpdir = tt_test_tmpdir();
    char old_cache[512], new_cache[512];
    snprintf(old_cache, sizeof(old_cache), "%s/.cache/.toktoken", tmpdir);
    snprintf(new_cache, sizeof(new_cache), "%s/.cache/toktoken", tmpdir);
    tt_mkdir_p(old_cache);
    tt_mkdir_p(new_cache);

    char *orig_home = save_home_env();
    set_home_env(tmpdir);

    char *base = tt_storage_base_dir();
    TT_ASSERT_NOT_NULL(base);
    TT_ASSERT_STR_CONTAINS(base, "toktoken");

    restore_home_env(orig_home);
    free(orig_home);
    free(base);
    tt_test_rmdir(tmpdir);
    free(tmpdir);
}

void run_storage_paths_tests(void)
{
    TT_RUN(test_sp_base_dir_contains_toktoken);
    TT_RUN(test_sp_base_dir_is_absolute);
    TT_RUN(test_sp_projects_dir_under_base);
    TT_RUN(test_sp_project_dir_uses_hash);
    TT_RUN(test_sp_project_dir_deterministic);
    TT_RUN(test_sp_different_paths_different_hashes);
    TT_RUN(test_sp_project_dir_long_path);
    TT_RUN(test_sp_logs_dir_under_base);

    /* Migration tests */
    TT_RUN(test_sp_migration_old_to_new);
    TT_RUN(test_sp_migration_already_new);
    TT_RUN(test_sp_migration_fresh_install);
    TT_RUN(test_sp_migration_both_exist);
}
