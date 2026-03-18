/*
 * ctags_stream_win.c -- Windows stub for ctags streaming.
 *
 * Ctags subprocess streaming is not yet implemented on Windows.
 * All functions return errors gracefully so the rest of the pipeline
 * can detect the failure and report it without crashing.
 *
 * POSIX implementation: ctags_stream.c
 */

#include "platform.h"

#ifdef TT_PLATFORM_WINDOWS

#include "ctags_stream.h"
#include "error.h"

#include <stddef.h>
#include <string.h>

int tt_ctags_stream_start(tt_ctags_stream_t *stream,
                          const char *ctags_path,
                          const char *file_list_path,
                          int timeout_sec)
{
    (void)ctags_path;
    (void)file_list_path;
    (void)timeout_sec;

    if (stream)
    {
        memset(stream, 0, sizeof(*stream));
        stream->finished = true;
    }
    tt_error_set("ctags streaming is not yet supported on Windows");
    return -1;
}

const char *tt_ctags_stream_readline(tt_ctags_stream_t *stream,
                                     size_t *line_len)
{
    (void)stream;
    if (line_len)
        *line_len = 0;
    return NULL;
}

int tt_ctags_stream_finish(tt_ctags_stream_t *stream,
                           char **stderr_out)
{
    (void)stream;
    if (stderr_out)
        *stderr_out = NULL;
    return -1;
}

#endif /* TT_PLATFORM_WINDOWS */
