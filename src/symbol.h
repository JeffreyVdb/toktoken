/*
 * symbol.h -- Symbol struct and ID generation.
 *
 * A Symbol represents a code entity (function, class, method, etc.)
 * extracted from source code. All string fields own their memory.
 */

#ifndef TT_SYMBOL_H
#define TT_SYMBOL_H

#include "symbol_kind.h"

typedef struct
{
    char *id;             /* "{file}::{qualified_name}#{kind}[~N]" [owns] */
    char *file;           /* relative path [owns] */
    char *name;           /* bare name [owns] */
    char *qualified_name; /* "scope.name" or just "name" [owns] */
    tt_symbol_kind_e kind;
    char *language;     /* canonical language [owns] */
    char *signature;    /* function prototype [owns] */
    char *docstring;    /* extracted from comments [owns] */
    char *summary;      /* tier 1/3 summary, "" initially [owns] */
    char *decorators;   /* JSON array string "[]" [owns] */
    char *keywords;     /* JSON array string '["word1","word2"]' [owns] */
    char *parent_id;    /* parent symbol ID or NULL [owns] */
    int line;           /* 1-indexed */
    int end_line;       /* 1-indexed */
    int byte_offset;    /* position in file */
    int byte_length;    /* symbol byte range */
    char *content_hash; /* SHA-256 hex string [owns] */
} tt_symbol_t;

/*
 * tt_symbol_make_id -- Generate symbol ID.
 *
 * Format: "{file}::{qualifiedName}#{kind}" when overload==0,
 *         "{file}::{qualifiedName}#{kind}~{N}" when overload>0.
 *
 * Ref: Symbol.php makeId()
 * [caller-frees]
 */
char *tt_symbol_make_id(const char *file, const char *qualified_name,
                        const char *kind_str, int overload);

/*
 * tt_symbol_free -- Free all internal strings of a single symbol.
 *
 * Does NOT free the symbol struct itself.
 */
void tt_symbol_free(tt_symbol_t *sym);

/*
 * tt_symbol_array_free -- Free an array of symbols and their strings.
 *
 * Frees each symbol's strings, then the array itself.
 */
void tt_symbol_array_free(tt_symbol_t *syms, int count);

#endif /* TT_SYMBOL_H */
