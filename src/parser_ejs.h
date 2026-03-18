/*
 * parser_ejs.h -- Parser for EJS template files.
 *
 * Extracts include directives from .ejs files.
 */

#ifndef TT_PARSER_EJS_H
#define TT_PARSER_EJS_H

#include "symbol.h"

/*
 * tt_parse_ejs -- Parse EJS include directives from .ejs files.
 *
 * project_root: absolute path to the project root.
 * file_paths:   array of RELATIVE paths to .ejs files.
 * file_count:   number of file paths.
 * out:          receives allocated array of symbols [caller-frees with tt_symbol_array_free].
 * out_count:    receives number of symbols produced.
 *
 * Returns 0 on success, -1 on error.
 */
int tt_parse_ejs(const char *project_root, const char **file_paths, int file_count,
                 tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_EJS_H */
