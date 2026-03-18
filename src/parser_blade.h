/*
 * parser_blade.h -- Parser for Laravel Blade templates.
 *
 * Extracts Blade directives (@extends, @section, @include, etc.)
 * as symbols since ctags treats .blade.php as plain PHP.
 */

#ifndef TT_PARSER_BLADE_H
#define TT_PARSER_BLADE_H

#include "symbol.h"

/*
 * tt_parse_blade -- Parse Blade directives from .blade.php files.
 *
 * project_root: absolute path to the project root.
 * file_paths:   array of RELATIVE paths to .blade.php files.
 * file_count:   number of file paths.
 * out:          receives allocated array of symbols [caller-frees with tt_symbol_array_free].
 * out_count:    receives number of symbols produced.
 *
 * Returns 0 on success, -1 on error.
 */
int tt_parse_blade(const char *project_root, const char **file_paths, int file_count,
                   tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_BLADE_H */
