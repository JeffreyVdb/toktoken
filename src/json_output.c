/*
 * json_output.c -- JSON output helpers for CLI commands.
 */

#include "json_output.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>

/* Timer state */
static uint64_t timer_start_ms;

void tt_json_print(cJSON *json)
{
    if (!json)
        return;
    char *str = cJSON_PrintUnformatted(json);
    if (str)
    {
        fputs(str, stdout);
        fputc('\n', stdout);
        fflush(stdout);
        free(str);
    }
}

int tt_output_success(cJSON *data)
{
    tt_json_print(data);
    cJSON_Delete(data);
    return 0;
}

int tt_output_error(const char *error_code, const char *message, const char *hint)
{
    cJSON *json = cJSON_CreateObject();
    if (!json)
        return 1;

    cJSON_AddStringToObject(json, "error", error_code ? error_code : "unknown");
    cJSON_AddStringToObject(json, "message", message ? message : "");
    if (hint && hint[0])
    {
        cJSON_AddStringToObject(json, "hint", hint);
    }

    tt_json_print(json);
    cJSON_Delete(json);
    return 1;
}

void tt_timer_start(void)
{
    timer_start_ms = tt_monotonic_ms();
}

double tt_timer_elapsed_sec(void)
{
    uint64_t now = tt_monotonic_ms();
    double sec = (double)(now - timer_start_ms) / 1000.0;
    return round(sec * 100.0) / 100.0;
}

double tt_timer_elapsed_ms(void)
{
    uint64_t now = tt_monotonic_ms();
    double ms = (double)(now - timer_start_ms);
    return round(ms * 10.0) / 10.0;
}

void tt_progress(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fflush(stderr);
}
