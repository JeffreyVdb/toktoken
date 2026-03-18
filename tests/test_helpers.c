/*
 * test_helpers.c -- Implementation of test utility functions.
 */

#include "test_helpers.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>

const char *tt_test_fixtures_dir(void)
{
    static char buf[1024];
    static int resolved = 0;

    if (resolved) return buf[0] ? buf : NULL;
    resolved = 1;

    const char *candidates[] = {
        "tests/fixtures",
        "../tests/fixtures",
        "../../tests/fixtures",
    };

    for (int i = 0; i < 3; i++) {
        struct stat st;
        if (stat(candidates[i], &st) == 0 && S_ISDIR(st.st_mode)) {
            /* Verify directory has actual fixtures (not an empty build artifact) */
            char sentinel[2048];
            snprintf(sentinel, sizeof(sentinel), "%s/sample.blade.php", candidates[i]);
            if (stat(sentinel, &st) != 0) continue;

            char *rp = realpath(candidates[i], NULL);
            if (rp) {
                snprintf(buf, sizeof(buf), "%s", rp);
                free(rp);
                return buf;
            }
        }
    }

    buf[0] = '\0';
    return NULL;
}

const char *tt_test_fixture(const char *relative_path)
{
    static char buf[2048];
    const char *base = tt_test_fixtures_dir();
    if (!base) return NULL;
    snprintf(buf, sizeof(buf), "%s/%s", base, relative_path);
    return buf;
}

char *tt_test_tmpdir(void)
{
    char tpl[256];
    snprintf(tpl, sizeof(tpl), "/tmp/tt_test_XXXXXX");
    char *dir = mkdtemp(tpl);
    if (!dir) return NULL;
    return strdup(dir);
}

int tt_test_rmdir(const char *path)
{
    if (!path) return -1;
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
    return system(cmd);
}

/* Ensure parent directories exist for a file path. */
static int ensure_parent_dir(const char *filepath)
{
    char *tmp = strdup(filepath);
    if (!tmp) return -1;

    char *dir = dirname(tmp);
    int rc = tt_mkdir_p(dir);
    free(tmp);
    return rc;
}

int tt_test_write_file(const char *dir, const char *relative_path,
                        const char *content)
{
    char full[2048];
    snprintf(full, sizeof(full), "%s/%s", dir, relative_path);

    if (ensure_parent_dir(full) < 0) return -1;

    FILE *f = fopen(full, "w");
    if (!f) return -1;

    if (content && content[0]) {
        fputs(content, f);
    }
    fclose(f);
    return 0;
}
