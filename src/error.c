/*
 * error.c -- Thread-local error buffer for structured error reporting.
 */

#include "error.h"

#include <stdarg.h>
#include <stdio.h>

#define TT_ERROR_BUF_SIZE 1024

static _Thread_local char error_buf[TT_ERROR_BUF_SIZE] = "";

void tt_error_set(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error_buf, TT_ERROR_BUF_SIZE, fmt, ap);
    va_end(ap);
}

const char *tt_error_get(void)
{
    return error_buf;
}

void tt_error_clear(void)
{
    error_buf[0] = '\0';
}
