/*
 * parser_autohotkey.h -- Parser for AutoHotkey (.ahk) files.
 */

#ifndef TT_PARSER_AUTOHOTKEY_H
#define TT_PARSER_AUTOHOTKEY_H

#include "symbol.h"

int tt_parse_autohotkey(const char *project_root, const char **file_paths, int file_count,
                         tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_AUTOHOTKEY_H */
