/*
 * parser_verse.h -- Parser for Verse/UEFN (.verse) files.
 */

#ifndef TT_PARSER_VERSE_H
#define TT_PARSER_VERSE_H

#include "symbol.h"

int tt_parse_verse(const char *project_root, const char **file_paths, int file_count,
                   tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_VERSE_H */
