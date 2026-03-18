/*
 * parser_nix.h -- Parser for Nix (.nix) files.
 *
 * Extracts top-level attribute bindings and inherit declarations.
 */

#ifndef TT_PARSER_NIX_H
#define TT_PARSER_NIX_H

#include "symbol.h"

/*
 * tt_parse_nix -- Parse Nix files and extract symbols.
 *
 * project_root: absolute path to the project root.
 * file_paths:   array of RELATIVE paths to .nix files.
 * file_count:   number of file paths.
 * out:          receives allocated array of symbols [caller-frees with tt_symbol_array_free].
 * out_count:    receives number of symbols produced.
 *
 * Returns 0 on success, -1 on error.
 */
int tt_parse_nix(const char *project_root, const char **file_paths, int file_count,
                 tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_NIX_H */
