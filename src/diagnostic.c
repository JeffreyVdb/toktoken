/*
 * diagnostic.c -- Structured diagnostic output for index pipeline.
 *
 * Emits JSONL to stderr. Each event is a single fprintf call (POSIX atomic).
 * Memory snapshots read /proc/self/statm on Linux.
 */

#include "diagnostic.h"
#include "platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static bool g_enabled = false;
static uint64_t g_start_ms = 0;
static uint64_t g_peak_rss_kb = 0;

void tt_diag_init(void)
{
    g_start_ms = tt_monotonic_ms();
    g_peak_rss_kb = 0;
}

void tt_diag_enable(void)
{
    g_enabled = true;
}

bool tt_diag_enabled(void)
{
    return g_enabled;
}

void tt_diag_event(const char *phase, const char *event,
                   const char *fmt, ...)
{
    if (!g_enabled)
        return;

    uint64_t now = tt_monotonic_ms();
    double ts = (double)(now - g_start_ms) / 1000.0;

    char payload[1024];
    payload[0] = '\0';

    if (fmt && fmt[0])
    {
        va_list args;
        va_start(args, fmt);
        vsnprintf(payload, sizeof(payload), fmt, args);
        va_end(args);
    }

    if (payload[0])
        fprintf(stderr, "{\"ts\":%.3f,\"ph\":\"%s\",\"ev\":\"%s\",%s}\n",
                ts, phase, event, payload);
    else
        fprintf(stderr, "{\"ts\":%.3f,\"ph\":\"%s\",\"ev\":\"%s\"}\n",
                ts, phase, event);
    fflush(stderr);
}

void tt_diag_mem_snapshot(void)
{
    if (!g_enabled)
        return;

#ifdef TT_PLATFORM_LINUX
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f)
        return;

    unsigned long vm_pages = 0, rss_pages = 0;
    if (fscanf(f, "%lu %lu", &vm_pages, &rss_pages) != 2)
    {
        fclose(f);
        return;
    }
    fclose(f);

    /* Page size is 4KB on x86_64, but use sysconf for correctness */
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0)
        page_size = 4096;

    uint64_t vm_kb = (vm_pages * (uint64_t)page_size) / 1024;
    uint64_t rss_kb = (rss_pages * (uint64_t)page_size) / 1024;

    if (rss_kb > g_peak_rss_kb)
        g_peak_rss_kb = rss_kb;

    tt_diag_event("mem", "snapshot",
                  "\"vm_kb\":%llu,\"rss_kb\":%llu",
                  (unsigned long long)vm_kb,
                  (unsigned long long)rss_kb);
#endif
}

uint64_t tt_diag_peak_rss_kb(void)
{
    return g_peak_rss_kb;
}
