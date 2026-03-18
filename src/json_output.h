/*
 * json_output.h -- JSON output helpers for CLI commands.
 *
 * All JSON goes to stdout. Progress/diagnostics go to stderr.
 * cJSON does not escape '/' or non-ASCII Unicode by default,
 * matching PHP JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE.
 */

#ifndef TT_JSON_OUTPUT_H
#define TT_JSON_OUTPUT_H

#include <cJSON.h>

/*
 * tt_json_print -- Print JSON to stdout with trailing newline.
 */
void tt_json_print(cJSON *json);

/*
 * tt_output_success -- Print data JSON to stdout.
 *
 * No envelope/wrapper: the data is printed directly.
 * Returns exit code 0.
 */
int tt_output_success(cJSON *data);

/*
 * tt_output_error -- Print error JSON to stdout and return exit code 1.
 *
 * Format: {"error":"code","message":"msg","hint":"hint"}
 * The "hint" field is omitted if NULL or empty.
 */
int tt_output_error(const char *error_code, const char *message, const char *hint);

/*
 * tt_timer_start -- Start the elapsed timer.
 */
void tt_timer_start(void);

/*
 * tt_timer_elapsed_sec -- Seconds since tt_timer_start, rounded to 2 decimals.
 */
double tt_timer_elapsed_sec(void);

/*
 * tt_timer_elapsed_ms -- Milliseconds since tt_timer_start, rounded to 1 decimal.
 */
double tt_timer_elapsed_ms(void);

/*
 * tt_progress -- Print progress message to stderr (printf-like).
 */
void tt_progress(const char *fmt, ...);

#endif /* TT_JSON_OUTPUT_H */
