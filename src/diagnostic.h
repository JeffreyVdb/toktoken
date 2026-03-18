/*
 * diagnostic.h -- Structured diagnostic output for index pipeline.
 *
 * Emits JSONL events to stderr when enabled via --diagnostic / -X.
 * Each line is a self-contained JSON object with timestamp, phase,
 * event type, and payload. Designed for AI consumption (Claude feedback loop).
 *
 * Thread-safe: each emission is a single fprintf(stderr) call,
 * atomic under POSIX. The enable flag is set before thread creation
 * and read-only thereafter.
 */

#ifndef TT_DIAGNOSTIC_H
#define TT_DIAGNOSTIC_H

#include <stdbool.h>
#include <stdint.h>

/*
 * tt_diag_init -- Initialize diagnostic start timestamp.
 *
 * Must be called before any tt_diag_event() calls.
 */
void tt_diag_init(void);

/*
 * tt_diag_enable -- Enable diagnostic output.
 *
 * Must be called before any threads are spawned.
 */
void tt_diag_enable(void);

/*
 * tt_diag_enabled -- Check if diagnostic mode is active.
 *
 * Returns false by default. Zero-cost branch prediction when disabled.
 */
bool tt_diag_enabled(void);

/*
 * tt_diag_event -- Emit a JSONL diagnostic event to stderr.
 *
 * phase: pipeline phase (e.g., "init", "discovery", "worker", "writer")
 * event: event name (e.g., "start", "progress", "done")
 * fmt:   printf-style format for the JSON payload fields (without braces).
 *        Example: "\"wid\":%d,\"files\":%d"
 *
 * Output format: {"ts":1.234,"ph":"phase","ev":"event",...payload...}\n
 */
void tt_diag_event(const char *phase, const char *event,
                   const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

/*
 * tt_diag_mem_snapshot -- Emit a memory snapshot event.
 *
 * Reads /proc/self/statm on Linux, no-op on other platforms.
 * Tracks peak RSS internally for the final summary.
 */
void tt_diag_mem_snapshot(void);

/*
 * tt_diag_peak_rss_kb -- Return peak RSS observed during this session.
 *
 * Returns 0 if no snapshots were taken or on non-Linux.
 */
uint64_t tt_diag_peak_rss_kb(void);

/* Convenience macros: compile to nothing when diagnostic is disabled */
#define TT_DIAG(ph, ev, ...) \
    do { if (tt_diag_enabled()) tt_diag_event(ph, ev, __VA_ARGS__); } while (0)

#define TT_DIAG_MEM() \
    do { if (tt_diag_enabled()) tt_diag_mem_snapshot(); } while (0)

#endif /* TT_DIAGNOSTIC_H */
