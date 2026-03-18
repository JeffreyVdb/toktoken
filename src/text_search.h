/*
 * text_search.h -- Full-text search across indexed files.
 *
 * Dual strategy: try ripgrep first (fast), fall back to manual scan.
 */

#ifndef TT_TEXT_SEARCH_H
#define TT_TEXT_SEARCH_H

#include <stdbool.h>

/* Search options. */
typedef struct
{
    bool case_sensitive; /* default: false */
    bool is_regex;       /* default: false -- treat queries as regex patterns */
    int max_results;     /* default: 100 */
    int context_lines;   /* default: 0 (no context) */
} tt_text_search_opts_t;

/* Single search result. */
typedef struct
{
    char *file;    /* relative path [owns] */
    int line;      /* 1-indexed */
    char *text;    /* line text (trimmed, max 200 chars + "...") [owns] */
    char **before; /* context lines before [owns], NULL if no context */
    int before_count;
    char **after; /* context lines after [owns], NULL if no context */
    int after_count;
} tt_text_result_t;

/* Collection of search results. */
typedef struct
{
    tt_text_result_t *results; /* [owns] */
    int count;
} tt_text_results_t;

/*
 * tt_text_search -- Search for text in the specified files.
 *
 * Uses ripgrep if available, falls back to manual line-by-line scan.
 * queries: array of query strings (OR logic)
 * file_paths: array of relative paths to search
 * project_root: root directory (for constructing absolute paths)
 * opts: search options (NULL for defaults)
 *
 * Returns 0 on success, -1 on error.
 */
int tt_text_search(const char *project_root, const char **file_paths, int file_count,
                   const char **queries, int query_count,
                   const tt_text_search_opts_t *opts,
                   tt_text_results_t *out);

/*
 * tt_text_results_free -- Free all results and their strings.
 */
void tt_text_results_free(tt_text_results_t *r);

/*
 * tt_regex_validate -- Validate a regex pattern for safety (ReDoS protection).
 *
 * Rejects patterns that could cause catastrophic backtracking:
 *   - Nested quantifiers: (a+)+, (a*)+, (a+)*, (?:a+){2,}
 *   - Excessive length (> 200 characters)
 *
 * Returns NULL if safe, or a static error message string if rejected.
 */
const char *tt_regex_validate(const char *pattern);

#endif /* TT_TEXT_SEARCH_H */
