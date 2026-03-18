/*
 * output_fmt.h -- Output formatting: filter/exclude/limit/table/JSONL.
 */

#ifndef TT_OUTPUT_FMT_H
#define TT_OUTPUT_FMT_H

#include <cJSON.h>
#include <stdbool.h>

/*
 * tt_matches_path_filters -- Check if a file path passes filter/exclude.
 *
 * --filter: pipe-separated patterns. At least one must match (case-insensitive
 *   substring). NULL filter means all paths pass.
 * --exclude: pipe-separated patterns. If any matches, the path is excluded.
 *   NULL exclude means nothing is excluded.
 */
bool tt_matches_path_filters(const char *file_path,
                             const char *filter,
                             const char *exclude);

/*
 * tt_apply_limit -- Cap *count to limit if limit > 0.
 */
void tt_apply_limit(int limit, int *count);

/*
 * tt_output_jsonl -- Print a cJSON array as NDJSON (one object per line).
 *
 * Returns 0.
 */
int tt_output_jsonl(cJSON *array);

/*
 * tt_render_table -- Render an ASCII table to stdout.
 *
 * columns:    array of column header names (printed in UPPERCASE)
 * min_widths: minimum width for each column
 * col_count:  number of columns
 * rows:       array of rows; each row is an array of col_count strings
 * row_count:  number of rows
 * truncate:   max chars per cell (cells exceeding this get "..." suffix)
 */
void tt_render_table(const char **columns, const int *min_widths, int col_count,
                     const char ***rows, int row_count, int truncate);

#endif /* TT_OUTPUT_FMT_H */
