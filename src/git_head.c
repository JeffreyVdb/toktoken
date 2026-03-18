/*
 * git_head.c -- Read the current git HEAD commit SHA.
 *
 * Reads .git/HEAD directly to avoid forking a subprocess.
 */

#include "git_head.h"
#include "platform.h"
#include "str_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Check if string looks like a 40-char hex SHA */
static bool is_sha(const char *s)
{
    if (!s) return false;
    int i;
    for (i = 0; i < 40; i++) {
        if (!isxdigit((unsigned char)s[i])) return false;
    }
    return s[40] == '\0' || isspace((unsigned char)s[40]);
}

/* Read a file and trim whitespace. Returns NULL on failure. */
static char *read_trimmed(const char *path)
{
    char *data = tt_read_file(path, NULL);
    if (!data) return NULL;
    tt_str_trim(data);
    return data;
}

/* Try to find a SHA in packed-refs for a given ref */
static char *find_in_packed_refs(const char *git_dir, const char *ref)
{
    char *packed_path = tt_path_join(git_dir, "packed-refs");
    if (!packed_path) return NULL;

    char *content = tt_read_file(packed_path, NULL);
    free(packed_path);
    if (!content) return NULL;

    char *result = NULL;
    char *line = content;

    while (line && *line) {
        char *eol = strchr(line, '\n');
        if (eol) *eol = '\0';

        /* Skip comments and peeled refs (^) */
        if (line[0] != '#' && line[0] != '^') {
            /* Format: "<sha> <ref>" */
            char *space = strchr(line, ' ');
            if (space) {
                *space = '\0';
                const char *packed_ref = space + 1;
                if (strcmp(packed_ref, ref) == 0 && is_sha(line)) {
                    result = tt_strndup(line, 40);
                    break;
                }
                *space = ' ';
            }
        }

        line = eol ? eol + 1 : NULL;
    }

    free(content);
    return result;
}

char *tt_git_head(const char *project_root)
{
    if (!project_root || !project_root[0])
        return tt_strdup("");

    /* Build path to .git/HEAD */
    char *git_dir = tt_path_join(project_root, ".git");
    if (!git_dir) return tt_strdup("");

    char *head_path = tt_path_join(git_dir, "HEAD");
    if (!head_path) {
        free(git_dir);
        return tt_strdup("");
    }

    char *head_content = read_trimmed(head_path);
    free(head_path);

    if (!head_content || !head_content[0]) {
        free(head_content);
        free(git_dir);
        return tt_strdup("");
    }

    /* Detached HEAD: content is a raw SHA */
    if (is_sha(head_content)) {
        char *sha = tt_strndup(head_content, 40);
        free(head_content);
        free(git_dir);
        return sha ? sha : tt_strdup("");
    }

    /* Symbolic ref: "ref: refs/heads/main" */
    if (tt_str_starts_with(head_content, "ref: ")) {
        const char *ref = head_content + 5;

        /* Try loose ref file first: .git/refs/heads/main */
        char *ref_path = tt_path_join(git_dir, ref);
        if (ref_path) {
            char *sha_content = read_trimmed(ref_path);
            free(ref_path);

            if (sha_content && is_sha(sha_content)) {
                char *sha = tt_strndup(sha_content, 40);
                free(sha_content);
                free(head_content);
                free(git_dir);
                return sha ? sha : tt_strdup("");
            }
            free(sha_content);
        }

        /* Fallback: check packed-refs */
        char *sha = find_in_packed_refs(git_dir, ref);
        free(head_content);
        free(git_dir);
        return sha ? sha : tt_strdup("");
    }

    /* Unknown format */
    free(head_content);
    free(git_dir);
    return tt_strdup("");
}
