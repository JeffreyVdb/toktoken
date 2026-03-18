/*
 * test_helpers.h -- Test utility functions: fixture paths, tmpdir, cleanup.
 */

#ifndef TT_TEST_HELPERS_H
#define TT_TEST_HELPERS_H

/*
 * tt_test_fixtures_dir -- Find the fixtures directory.
 *
 * Searches in order:
 *   1. ./tests/fixtures/          (run from project root)
 *   2. ../tests/fixtures/         (run from build dir)
 *   3. ../../tests/fixtures/      (run from build subdir)
 *
 * Returns static buffer (not thread-safe).
 * Returns NULL if not found.
 */
const char *tt_test_fixtures_dir(void);

/*
 * tt_test_fixture -- Build full path to a fixture.
 *
 * Returns static buffer: fixtures_dir + "/" + relative_path.
 * Returns NULL if fixtures dir not found.
 */
const char *tt_test_fixture(const char *relative_path);

/*
 * tt_test_tmpdir -- Create a temporary directory for tests.
 *
 * Returns a heap-allocated string.
 * [caller-frees] Caller should remove it after use with tt_test_rmdir.
 */
char *tt_test_tmpdir(void);

/*
 * tt_test_rmdir -- Recursively remove a directory (rm -rf).
 *
 * Only for test cleanup. Returns 0 on success.
 */
int tt_test_rmdir(const char *path);

/*
 * tt_test_write_file -- Write content to a file inside a directory.
 *
 * Creates parent directories as needed.
 * Returns 0 on success, -1 on error.
 */
int tt_test_write_file(const char *dir, const char *relative_path,
                        const char *content);

#endif /* TT_TEST_HELPERS_H */
