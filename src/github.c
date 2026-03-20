/*
 * github.c -- GitHub repository operations via gh CLI.
 *
 * Security: subprocess calls (gh, git) use tt_proc_run() with explicit argv
 * arrays. No user input is ever interpolated into shell command strings.
 * Filesystem operations (rm, ls) use native platform APIs, not subprocesses.
 */

#include "github.h"
#include "storage_paths.h"
#include "platform.h"
#include "error.h"
#include "str_util.h"
#include "json_output.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Clone timeout: 120 seconds */
#define GH_CLONE_TIMEOUT_MS (120 * 1000)

/* Pull timeout: 60 seconds */
#define GH_PULL_TIMEOUT_MS (60 * 1000)

/* ---- Resolved tool paths ---- */

/* Cached resolved paths for gh and git. Resolved once, reused for all
 * subprocess calls. This prevents PATH hijacking between the availability
 * check and actual use, and ensures we always call the same binary. */
static char *gh_path_cache = NULL;
static char *git_path_cache = NULL;

static const char *resolve_gh(void)
{
    if (!gh_path_cache)
        gh_path_cache = tt_which("gh");
    return gh_path_cache;
}

static const char *resolve_git(void)
{
    if (!git_path_cache)
        git_path_cache = tt_which("git");
    return git_path_cache;
}

void tt_gh_reset_path_cache(void)
{
    free(gh_path_cache);
    gh_path_cache = NULL;
    free(git_path_cache);
    git_path_cache = NULL;
}

/* ---- gh CLI checks ---- */

bool tt_gh_available(void)
{
    return resolve_gh() != NULL;
}

bool tt_gh_authenticated(void)
{
    const char *gh = resolve_gh();
    if (!gh) return false;

    const char *argv[] = {gh, "auth", "status", NULL};
    tt_proc_result_t r = tt_proc_run(argv, NULL, 10000);

    bool ok = (r.exit_code == 0);
    if (!ok)
    {
        if (r.stderr_buf && r.stderr_buf[0])
        {
            tt_error_set("gh auth status failed: %s", r.stderr_buf);
        }
        else
        {
            tt_error_set("gh auth status failed with exit code %d", r.exit_code);
        }
    }

    tt_proc_result_free(&r);
    return ok;
}

int tt_gh_check(void)
{
    if (!tt_gh_available())
    {
        tt_error_set("GitHub CLI (gh) non trovato nel PATH. "
                     "Installa gh: https://cli.github.com/ — poi esegui 'gh auth login'");
        return -1;
    }

    if (!tt_gh_authenticated())
    {
        /* tt_error already set by tt_gh_authenticated, but override with
         * a user-friendly message if it's a simple auth failure */
        const char *err = tt_error_get();
        if (strstr(err, "not logged") || strstr(err, "not authenticated") ||
            strstr(err, "no oauth"))
        {
            tt_error_set("GitHub CLI non autenticato. "
                         "Esegui 'gh auth login' per autenticarti con il tuo account GitHub");
        }
        return -1;
    }

    return 0;
}

/* ---- Input validation ---- */

static bool is_valid_owner_char(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '-';
}

static bool is_valid_repo_char(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.';
}

int tt_gh_validate_repo(const char *repo_spec,
                        char *owner_out, size_t owner_size,
                        char *repo_out, size_t repo_size)
{
    if (!repo_spec || repo_spec[0] == '\0')
    {
        tt_error_set("Repository specification is empty");
        return -1;
    }

    /* Check for whitespace */
    for (const char *p = repo_spec; *p; p++)
    {
        if (isspace((unsigned char)*p))
        {
            tt_error_set("Repository specification contains whitespace: '%s'", repo_spec);
            return -1;
        }
    }

    /* Find the slash */
    const char *slash = strchr(repo_spec, '/');
    if (!slash)
    {
        tt_error_set("Invalid format: expected 'owner/repo', got '%s'", repo_spec);
        return -1;
    }

    /* Check for multiple slashes */
    if (strchr(slash + 1, '/') != NULL)
    {
        tt_error_set("Invalid format: too many slashes in '%s'", repo_spec);
        return -1;
    }

    size_t owner_len = (size_t)(slash - repo_spec);
    size_t repo_len = strlen(slash + 1);

    /* Owner validation */
    if (owner_len == 0 || owner_len > 39)
    {
        tt_error_set("Owner name must be 1-39 characters, got %zu in '%s'",
                     owner_len, repo_spec);
        return -1;
    }
    if (repo_spec[0] == '-')
    {
        tt_error_set("Owner name cannot start with hyphen: '%s'", repo_spec);
        return -1;
    }
    for (size_t i = 0; i < owner_len; i++)
    {
        if (!is_valid_owner_char(repo_spec[i]))
        {
            tt_error_set("Invalid character '%c' in owner name: '%s'",
                         repo_spec[i], repo_spec);
            return -1;
        }
    }

    /* Repo validation */
    const char *repo_start = slash + 1;
    if (repo_len == 0 || repo_len > 100)
    {
        tt_error_set("Repository name must be 1-100 characters, got %zu in '%s'",
                     repo_len, repo_spec);
        return -1;
    }
    if (repo_start[0] == '.')
    {
        tt_error_set("Repository name cannot start with dot: '%s'", repo_spec);
        return -1;
    }
    for (size_t i = 0; i < repo_len; i++)
    {
        if (!is_valid_repo_char(repo_start[i]))
        {
            tt_error_set("Invalid character '%c' in repository name: '%s'",
                         repo_start[i], repo_spec);
            return -1;
        }
    }

    /* Path traversal check */
    if (strstr(repo_spec, "..") != NULL)
    {
        tt_error_set("Path traversal detected in repository specification: '%s'",
                     repo_spec);
        return -1;
    }

    /* Copy and normalize to lowercase */
    if (owner_len + 1 > owner_size || repo_len + 1 > repo_size)
    {
        tt_error_set("Output buffer too small for '%s'", repo_spec);
        return -1;
    }

    for (size_t i = 0; i < owner_len; i++)
    {
        owner_out[i] = (char)tolower((unsigned char)repo_spec[i]);
    }
    owner_out[owner_len] = '\0';

    for (size_t i = 0; i < repo_len; i++)
    {
        repo_out[i] = (char)tolower((unsigned char)repo_start[i]);
    }
    repo_out[repo_len] = '\0';

    return 0;
}

/* ---- Repository directory management ---- */

char *tt_gh_repos_base_dir(void)
{
    char *base = tt_storage_base_dir();
    if (!base)
    {
        tt_error_set("github: cannot resolve storage base directory");
        return NULL;
    }

    char *repos = tt_path_join(base, "gh-repos");
    free(base);
    return repos;
}

char *tt_gh_repo_dir(const char *owner, const char *repo)
{
    char *base = tt_gh_repos_base_dir();
    if (!base)
        return NULL;

    char *owner_dir = tt_path_join(base, owner);
    free(base);
    if (!owner_dir)
        return NULL;

    char *repo_dir = tt_path_join(owner_dir, repo);
    free(owner_dir);
    return repo_dir;
}

bool tt_gh_repo_exists(const char *owner, const char *repo)
{
    char *dir = tt_gh_repo_dir(owner, repo);
    if (!dir)
        return false;

    char *git_dir = tt_path_join(dir, ".git");
    free(dir);
    if (!git_dir)
        return false;

    bool exists = tt_is_dir(git_dir);
    free(git_dir);
    return exists;
}

/* ---- Clone and pull ---- */

/*
 * rmrf_safe -- Remove directory tree using native platform APIs.
 *
 * Cross-platform: uses tt_remove_dir_recursive() (implemented natively
 * on both Unix and Windows) instead of shelling out to "rm -rf".
 */
static int rmrf_safe(const char *path)
{
    if (!path)
        return 0;
    if (!tt_file_exists(path))
        return 0;

    return tt_remove_dir_recursive(path);
}

/*
 * classify_clone_error -- Inspect stderr from gh/git to produce a
 * user-friendly error code and message.
 */
static void classify_clone_error(const char *stderr_buf, int exit_code,
                                 const char *owner, const char *repo)
{
    if (!stderr_buf)
        stderr_buf = "";

    if (strstr(stderr_buf, "Could not resolve") ||
        strstr(stderr_buf, "not found") ||
        strstr(stderr_buf, "Not Found") ||
        strstr(stderr_buf, "repository not found"))
    {
        tt_error_set("Repository '%s/%s' not found. "
                     "Verify the name is correct. "
                     "For private repositories, ensure gh has access.",
                     owner, repo);
        return;
    }

    if (strstr(stderr_buf, "permission denied") ||
        strstr(stderr_buf, "Permission denied") ||
        strstr(stderr_buf, "403") ||
        strstr(stderr_buf, "denied"))
    {
        tt_error_set("Permission denied for '%s/%s'. "
                     "Ensure your gh credentials have access to this repository.",
                     owner, repo);
        return;
    }

    if (strstr(stderr_buf, "rate limit") ||
        strstr(stderr_buf, "API rate limit"))
    {
        tt_error_set("GitHub API rate limit exceeded. "
                     "Authenticate with 'gh auth login' for higher limits.");
        return;
    }

    if (exit_code == -1)
    {
        tt_error_set("Clone of '%s/%s' timed out after 120 seconds. "
                     "Verify your network connection and try again.",
                     owner, repo);
        return;
    }

    /* Generic error */
    tt_error_set("Clone of '%s/%s' failed (exit %d): %s",
                 owner, repo, exit_code, stderr_buf);
}

int tt_gh_clone(const char *owner, const char *repo,
                const char *target_dir, int depth, const char *branch)
{
    /* Build repo spec "owner/repo" safely */
    size_t spec_len = strlen(owner) + 1 + strlen(repo) + 1;
    char *repo_spec = malloc(spec_len);
    if (!repo_spec)
    {
        tt_error_set("github: out of memory");
        return -1;
    }
    snprintf(repo_spec, spec_len, "%s/%s", owner, repo);

    /* Create parent directory */
    char *parent = tt_strdup(target_dir);
    if (!parent)
    {
        free(repo_spec);
        tt_error_set("github: out of memory");
        return -1;
    }
    /* Find last '/' to get parent */
    char *last_sep = strrchr(parent, '/');
    if (last_sep)
    {
        *last_sep = '\0';
        int mkdir_rc = tt_mkdir_p(parent);
        if (mkdir_rc < 0)
        {
            tt_error_set("github: cannot create directory '%s'", parent);
            free(parent);
            free(repo_spec);
            return -1;
        }
    }
    free(parent);

    /* Build argument list for gh repo clone */
    const char *gh = resolve_gh();
    if (!gh)
    {
        free(repo_spec);
        tt_error_set("github: gh not found in PATH");
        return -1;
    }
    const char *argv[16];
    int argc = 0;
    argv[argc++] = gh;
    argv[argc++] = "repo";
    argv[argc++] = "clone";
    argv[argc++] = repo_spec;
    argv[argc++] = target_dir;
    argv[argc++] = "--";

    char depth_str[32];
    if (depth > 0)
    {
        snprintf(depth_str, sizeof(depth_str), "%d", depth);
        argv[argc++] = "--depth";
        argv[argc++] = depth_str;
    }

    if (branch && branch[0])
    {
        argv[argc++] = "-b";
        argv[argc++] = branch;
    }

    argv[argc] = NULL;

    tt_progress("Cloning %s/%s...\n", owner, repo);

    /* Retry loop with exponential backoff for rate limiting (403/429). */
    int max_retries = 3;
    for (int attempt = 0; attempt < max_retries; attempt++)
    {
        tt_proc_result_t r = tt_proc_run(argv, NULL, GH_CLONE_TIMEOUT_MS);

        if (r.exit_code == 0)
        {
            tt_proc_result_free(&r);
            free(repo_spec);
            return 0;
        }

        /* Check if this is a rate limit error worth retrying */
        bool is_rate_limit = false;
        if (r.stderr_buf)
        {
            is_rate_limit = (strstr(r.stderr_buf, "rate limit") != NULL ||
                             strstr(r.stderr_buf, "API rate limit") != NULL ||
                             strstr(r.stderr_buf, "secondary rate limit") != NULL ||
                             strstr(r.stderr_buf, "429") != NULL);
        }

        if (is_rate_limit && attempt < max_retries - 1)
        {
            int wait_sec = 1 << attempt; /* 1s, 2s, 4s */
            tt_progress("GitHub rate limit hit, retrying in %ds (attempt %d/%d)...\n",
                        wait_sec, attempt + 1, max_retries);
            tt_proc_result_free(&r);
            /* Cleanup partial clone before retry */
            rmrf_safe(target_dir);
            tt_sleep_ms(wait_sec * 1000);
            continue;
        }

        /* Non-retriable error or final attempt */
        classify_clone_error(r.stderr_buf, r.exit_code, owner, repo);
        tt_proc_result_free(&r);
        free(repo_spec);
        rmrf_safe(target_dir);
        return -1;
    }

    free(repo_spec);
    return -1; /* unreachable */
}

int tt_gh_pull(const char *target_dir, char **out_message)
{
    const char *git = resolve_git();
    if (!git)
    {
        tt_error_set("github: git not found in PATH");
        return -1;
    }
    const char *argv[] = {
        git, "-C", target_dir, "pull", "--ff-only", NULL};

    tt_progress("Updating %s...\n", target_dir);
    tt_proc_result_t r = tt_proc_run(argv, NULL, GH_PULL_TIMEOUT_MS);

    if (r.exit_code != 0)
    {
        if (r.stderr_buf && r.stderr_buf[0])
        {
            tt_error_set("git pull failed: %s", r.stderr_buf);
        }
        else
        {
            tt_error_set("git pull failed with exit code %d", r.exit_code);
        }
        tt_proc_result_free(&r);
        return -1;
    }

    if (out_message && r.stdout_buf)
    {
        *out_message = tt_strdup(r.stdout_buf);
    }
    else if (out_message)
    {
        *out_message = NULL;
    }

    tt_proc_result_free(&r);
    return 0;
}

/* ---- Cleanup ---- */

int tt_gh_remove_repo(const char *owner, const char *repo)
{
    char *dir = tt_gh_repo_dir(owner, repo);
    if (!dir)
        return -1;

    int rc = 0;
    if (tt_file_exists(dir))
    {
        rc = rmrf_safe(dir);
        if (rc < 0)
        {
            tt_error_set("github: failed to remove '%s'", dir);
        }
    }

    free(dir);
    return rc;
}

int tt_gh_remove_all_repos(void)
{
    char *base = tt_gh_repos_base_dir();
    if (!base)
        return -1;

    int rc = 0;
    if (tt_file_exists(base))
    {
        rc = rmrf_safe(base);
        if (rc < 0)
        {
            tt_error_set("github: failed to remove '%s'", base);
        }
    }

    free(base);
    return rc;
}

/*
 * gh_list_scan_ctx_t -- Context for tt_walk_dir callback used by
 * tt_gh_list_repos() to scan the repos/{owner}/{repo}/ structure
 * without shelling out to "ls".
 */
typedef struct
{
    tt_gh_list_entry_t *entries;
    int count;
    int cap;
    bool oom;
} gh_list_scan_ctx_t;

/*
 * repo_scan_cb -- tt_walk_dir callback for the inner (repo-level) scan.
 *
 * Called for each entry inside an owner directory. Checks if the entry
 * is a directory containing a .git subdirectory (i.e., a cloned repo).
 * Returns 1 (skip) for each entry to prevent recursion into subdirs.
 */
static int repo_scan_cb(const char *dir, const char *name,
                         bool is_dir, bool is_symlink, void *userdata)
{
    (void)is_symlink;
    gh_list_scan_ctx_t *ctx = userdata;

    if (ctx->oom)
        return -1; /* stop */

    /* Skip hidden entries and non-directories */
    if (!is_dir || name[0] == '.')
        return 1; /* skip */

    /* Check for .git subdirectory */
    char *repo_path = tt_path_join(dir, name);
    if (!repo_path)
        return 1;

    char *git_dir = tt_path_join(repo_path, ".git");
    bool is_git = git_dir && tt_is_dir(git_dir);
    free(git_dir);

    if (!is_git)
    {
        free(repo_path);
        return 1;
    }

    /* Grow entries array if needed */
    if (ctx->count >= ctx->cap)
    {
        int new_cap = ctx->cap ? ctx->cap * 2 : 16;
        void *tmp = realloc(ctx->entries,
                            (size_t)new_cap * sizeof(ctx->entries[0]));
        if (!tmp)
        {
            free(repo_path);
            ctx->oom = true;
            return -1; /* stop */
        }
        ctx->entries = tmp;
        ctx->cap = new_cap;
    }

    /* Extract owner name from dir path (last component) */
    const char *owner_name = tt_path_basename(dir);

    ctx->entries[ctx->count].owner = tt_strdup(owner_name);
    ctx->entries[ctx->count].repo = tt_strdup(name);
    ctx->entries[ctx->count].local_path = repo_path; /* transfer ownership */
    ctx->count++;

    return 1; /* skip: do not recurse into the repo */
}

/*
 * owner_scan_cb -- tt_walk_dir callback for the outer (owner-level) scan.
 *
 * Called for each entry inside the gh-repos base directory. For each
 * owner directory, runs a nested tt_walk_dir to find repos inside it.
 * Returns 1 (skip) for each entry to prevent tt_walk_dir from recursing
 * on its own -- we handle the one-level-deep recursion ourselves.
 */
static int owner_scan_cb(const char *dir, const char *name,
                          bool is_dir, bool is_symlink, void *userdata)
{
    (void)is_symlink;
    gh_list_scan_ctx_t *ctx = userdata;

    if (ctx->oom)
        return -1;

    if (!is_dir || name[0] == '.')
        return 1; /* skip hidden and non-dirs */

    char *owner_path = tt_path_join(dir, name);
    if (!owner_path)
        return 1;

    /* Scan repos inside this owner directory */
    tt_walk_dir(owner_path, repo_scan_cb, ctx);

    free(owner_path);
    return 1; /* skip: we handled recursion ourselves */
}

/* Scan the repos directory structure: repos/{owner}/{repo}/ */
int tt_gh_list_repos(tt_gh_list_entry_t **out_entries, int *out_count)
{
    *out_entries = NULL;
    *out_count = 0;

    char *base = tt_gh_repos_base_dir();
    if (!base)
        return 0; /* No repos dir = empty list, not an error */

    if (!tt_is_dir(base))
    {
        free(base);
        return 0;
    }

    gh_list_scan_ctx_t ctx = {NULL, 0, 0, false};

    tt_walk_dir(base, owner_scan_cb, &ctx);
    free(base);

    *out_entries = ctx.entries;
    *out_count = ctx.count;
    return 0;
}

void tt_gh_list_entry_free(tt_gh_list_entry_t *entry)
{
    if (!entry)
        return;
    free(entry->owner);
    free(entry->repo);
    free(entry->local_path);
}
