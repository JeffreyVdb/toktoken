/*
 * parser_common.h -- Shared helpers for custom language parsers.
 *
 * Provides symbol construction, keyword extraction, and array management
 * to avoid boilerplate duplication across parser_hcl, parser_graphql,
 * parser_julia, parser_gdscript, and parser_verse.
 */

#ifndef TT_PARSER_COMMON_H
#define TT_PARSER_COMMON_H

#include "symbol.h"
#include "symbol_kind.h"
#include "line_offsets.h"

#include <stddef.h>

/* Grow symbols array if at capacity. Returns 0 on success, -1 on error. */
int tt_parser_grow(tt_symbol_t **syms, int *count, int *cap);

/* Extract keywords from a name (camelCase + separator splitting).
 * Returns JSON array string. [caller-frees] */
char *tt_parser_extract_keywords(const char *name);

/* Add a symbol to the array with full field initialization.
 * Returns 0 on success, -1 on error. */
int tt_parser_add_symbol(tt_symbol_t **syms, int *count, int *cap,
                          const char *rel_path, const char *name,
                          const char *signature, tt_symbol_kind_e kind,
                          const char *language, int line_num,
                          const char *content, size_t content_len,
                          const tt_line_offsets_t *lo);

/* Read a \w+ word at position p. Returns length, 0 if no match. */
size_t tt_parser_read_word(const char *p);

#endif /* TT_PARSER_COMMON_H */
