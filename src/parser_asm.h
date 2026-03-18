/*
 * parser_asm.h -- Parser for Assembly (.asm, .s) files.
 */

#ifndef TT_PARSER_ASM_H
#define TT_PARSER_ASM_H

#include "symbol.h"

int tt_parse_asm(const char *project_root, const char **file_paths, int file_count,
                  tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_ASM_H */
