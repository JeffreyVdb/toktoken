/*
 * parser_julia.h -- Parser for Julia (.jl) files.
 */

#ifndef TT_PARSER_JULIA_H
#define TT_PARSER_JULIA_H

#include "symbol.h"

int tt_parse_julia(const char *project_root, const char **file_paths, int file_count,
                    tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_JULIA_H */
