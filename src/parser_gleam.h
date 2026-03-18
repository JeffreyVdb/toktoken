/*
 * parser_gleam.h -- Parser for Gleam (.gleam) files.
 *
 * Extracts: pub/fn, pub/type, pub/const, and import declarations.
 */

#ifndef TT_PARSER_GLEAM_H
#define TT_PARSER_GLEAM_H

#include "symbol.h"

/*
 * tt_parse_gleam -- Parse Gleam files and extract symbols.
 *
 * project_root: absolute path to the project root.
 * file_paths:   array of RELATIVE paths to .gleam files.
 * file_count:   number of file paths.
 * out:          receives allocated array of symbols [caller-frees with tt_symbol_array_free].
 * out_count:    receives number of symbols produced.
 *
 * Returns 0 on success, -1 on error.
 */
int tt_parse_gleam(const char *project_root, const char **file_paths, int file_count,
                   tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_GLEAM_H */
