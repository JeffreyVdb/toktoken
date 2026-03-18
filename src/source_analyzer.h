/*
 * source_analyzer.h -- Source code analysis: end-line estimation, docstring
 *                      extraction, keyword extraction.
 */

#ifndef TT_SOURCE_ANALYZER_H
#define TT_SOURCE_ANALYZER_H

#include "symbol_kind.h"

/*
 * tt_estimate_end_line -- Estimate end line of a symbol via brace counting.
 *
 * Uses a state machine: normal, in_string, in_block_comment.
 * $inString resets per line (PHP behavior: multi-line strings not tracked).
 * $inBlockComment persists across lines.
 * Single-line kinds return start_line immediately.
 * Fallback: min(start_line + 50, line_count).
 *
 * lines:      array of line strings (0-indexed).
 * line_count: total number of lines.
 * start_line: 1-indexed start line of the symbol.
 * kind:       symbol kind (for isSingleLine check).
 */
int tt_estimate_end_line(const char **lines, int line_count,
                         int start_line, tt_symbol_kind_e kind);

/*
 * tt_extract_docstring -- Extract docstring from comments above a symbol.
 *
 * Skips decorators (#[...], @annotations), then checks for docblock
 * or line comments. Returns cleaned text.
 *
 * lines:       array of line strings (0-indexed).
 * line_count:  total number of lines.
 * symbol_line: 1-indexed line of the symbol.
 * [caller-frees] Returns "" (allocated) if no docstring found.
 */
char *tt_extract_docstring(const char **lines, int line_count, int symbol_line);

/*
 * tt_extract_keywords -- Extract searchable keywords from qualified name.
 *
 * Splits on camelCase boundaries, underscores, dots, whitespace.
 * Lowercases, deduplicates, filters single-char words.
 * Returns JSON array string: '["auth","service"]'.
 *
 * [caller-frees]
 */
char *tt_extract_keywords(const char *qualified_name);

#endif /* TT_SOURCE_ANALYZER_H */
