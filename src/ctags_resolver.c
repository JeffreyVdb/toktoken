/*
 * ctags_resolver.c -- Find the Universal Ctags binary.
 */

#include "ctags_resolver.h"
#include "platform.h"
#include "str_util.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TT_PLATFORM_UNIX
#include <unistd.h>
#endif

#ifdef TT_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define CTAGS_EXE "ctags.exe"
#else
#define CTAGS_EXE "ctags"
#endif

/* Determine platform subdirectory for bundled ctags. */
static const char *platform_subdir(void)
{
#ifdef TT_PLATFORM_WINDOWS
#if defined(_M_X64) || defined(__x86_64__)
    return "win-x64";
#else
    return NULL; /* unsupported platform */
#endif
#elif defined(TT_PLATFORM_MACOS)
#if defined(__aarch64__) || defined(__arm64__)
    return "darwin-arm64";
#elif defined(__x86_64__)
    return "darwin-x64";
#else
    return NULL;
#endif
#else /* Linux */
#if defined(__aarch64__)
    return "linux-arm64";
#elif defined(__x86_64__)
    return "linux-x64";
#else
    return NULL;
#endif
#endif
}

/* Get directory of the current executable. */
static char *exe_dir(void)
{
#ifdef TT_PLATFORM_LINUX
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0)
        return NULL;
    buf[len] = '\0';
    /* Strip filename to get directory */
    char *last_sep = strrchr(buf, '/');
    if (last_sep)
        *last_sep = '\0';
    return tt_strdup(buf);
#elif defined(TT_PLATFORM_MACOS)
    /* _NSGetExecutablePath */
    char buf[4096];
    uint32_t size = sizeof(buf);
    extern int _NSGetExecutablePath(char *buf, uint32_t *bufsize);
    if (_NSGetExecutablePath(buf, &size) != 0)
        return NULL;
    char *resolved = tt_realpath(buf);
    if (!resolved)
        return NULL;
    char *last_sep = strrchr(resolved, '/');
    if (last_sep)
        *last_sep = '\0';
    return resolved;
#elif defined(TT_PLATFORM_WINDOWS)
    /* GetModuleFileName */
    char buf[4096];
    DWORD len = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (len == 0)
        return NULL;
    buf[len] = '\0';
    char *last_sep = strrchr(buf, '\\');
    if (!last_sep)
        last_sep = strrchr(buf, '/');
    if (last_sep)
        *last_sep = '\0';
    return tt_strdup(buf);
#else
    return NULL;
#endif
}

/* Check if ctags at path is Universal Ctags. */
static bool is_universal_ctags(const char *path)
{
    char *ver = tt_ctags_version(path);
    if (!ver)
        return false;
    bool ok = (strstr(ver, "Universal Ctags") != NULL);
    free(ver);
    return ok;
}

char *tt_ctags_resolve(void)
{
    /* Strategy 1: bundled binary */
    const char *subdir = platform_subdir();
    if (subdir)
    {
        char *dir = exe_dir();
        if (dir)
        {
            char *bundled = tt_path_join(dir, "resources/ctags");
            if (bundled)
            {
                char *platform_dir = tt_path_join(bundled, subdir);
                free(bundled);
                if (platform_dir)
                {
                    char *bin = tt_path_join(platform_dir, CTAGS_EXE);
                    free(platform_dir);
                    if (bin && tt_file_exists(bin))
                    {
                        free(dir);
                        return bin;
                    }
                    free(bin);
                }
            }
            free(dir);
        }
    }

    /* Strategy 2: universal-ctags in PATH */
    char *path = tt_which("universal-ctags");
    if (path)
        return path;

    /* Strategy 3: ctags in PATH, verify it's Universal */
    path = tt_which("ctags");
    if (path)
    {
        if (is_universal_ctags(path))
            return path;
        free(path);
    }

    tt_error_set("Universal Ctags not found. Install it or place it in PATH.");
    return NULL;
}

char *tt_ctags_version(const char *ctags_path)
{
    if (!ctags_path)
        return NULL;

    const char *argv[] = {ctags_path, "--version", NULL};
    tt_proc_result_t result = tt_proc_run(argv, NULL, 5000);

    if (result.exit_code != 0 || !result.stdout_buf)
    {
        tt_proc_result_free(&result);
        return NULL;
    }

    /* Return first line only. */
    char *nl = strchr(result.stdout_buf, '\n');
    char *version;
    if (nl)
    {
        version = tt_strndup(result.stdout_buf, (size_t)(nl - result.stdout_buf));
    }
    else
    {
        version = tt_strdup(result.stdout_buf);
    }

    tt_proc_result_free(&result);
    return version;
}
