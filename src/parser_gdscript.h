/*
 * parser_gdscript.h -- Parser for GDScript (.gd) files.
 */

#ifndef TT_PARSER_GDSCRIPT_H
#define TT_PARSER_GDSCRIPT_H

#include "symbol.h"

int tt_parse_gdscript(const char *project_root, const char **file_paths, int file_count,
                      tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_GDSCRIPT_H */
