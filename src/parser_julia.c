/*
 * parser_julia.c -- Parser for Julia (.jl) files.
 *
 * Patterns:
 *   function name(args)     -> FUNCTION
 *   macro name(args)        -> FUNCTION
 *   struct Name / mutable struct Name -> CLASS
 *   abstract type Name      -> CLASS
 *   module Name             -> CLASS
 *   const NAME =            -> CONSTANT
 */

#include "parser_julia.h"
#include "parser_common.h"
#include "str_util.h"
#include "platform.h"
#include "line_offsets.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int tt_parse_julia(const char *project_root, const char **file_paths, int file_count,
                    tt_symbol_t **out, int *out_count)
{
    if (!out || !out_count) return -1;
    *out = NULL;
    *out_count = 0;
    if (!project_root || !file_paths || file_count <= 0) return 0;

    int cap = 32, cnt = 0;
    tt_symbol_t *syms = malloc((size_t)cap * sizeof(tt_symbol_t));
    if (!syms) return -1;

    for (int fi = 0; fi < file_count; fi++) {
        const char *rel = file_paths[fi];
        char *full = tt_path_join(project_root, rel);
        if (!full) continue;

        size_t clen = 0;
        char *content = tt_read_file(full, &clen);
        free(full);
        if (!content) continue;

        tt_line_offsets_t lo;
        tt_line_offsets_build(&lo, content, clen);

        int nlines = 0;
        char **lines = tt_str_split(content, '\n', &nlines);

        for (int i = 0; i < nlines; i++) {
            int line_num = i + 1;
            const char *p = lines[i];
            while (*p == ' ' || *p == '\t') p++;

            /* "function name(" or "function name{" */
            if (strncmp(p, "function ", 9) == 0) {
                const char *np = p + 9;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    char *name = tt_strndup(np, nlen);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                          TT_KIND_FUNCTION, "julia", line_num,
                                          content, clen, &lo);
                    free(name);
                }
                continue;
            }

            /* "macro name(" */
            if (strncmp(p, "macro ", 6) == 0) {
                const char *np = p + 6;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    char *name = tt_strndup(np, nlen);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                          TT_KIND_FUNCTION, "julia", line_num,
                                          content, clen, &lo);
                    free(name);
                }
                continue;
            }

            /* "mutable struct Name" or "struct Name" */
            {
                const char *sp = p;
                if (strncmp(sp, "mutable ", 8) == 0) sp += 8;
                if (strncmp(sp, "struct ", 7) == 0) {
                    const char *np = sp + 7;
                    while (*np == ' ' || *np == '\t') np++;
                    size_t nlen = tt_parser_read_word(np);
                    if (nlen > 0) {
                        char *name = tt_strndup(np, nlen);
                        tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                              TT_KIND_CLASS, "julia", line_num,
                                              content, clen, &lo);
                        free(name);
                    }
                    continue;
                }
            }

            /* "abstract type Name" */
            if (strncmp(p, "abstract type ", 14) == 0) {
                const char *np = p + 14;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    char *name = tt_strndup(np, nlen);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                          TT_KIND_CLASS, "julia", line_num,
                                          content, clen, &lo);
                    free(name);
                }
                continue;
            }

            /* "module Name" */
            if (strncmp(p, "module ", 7) == 0) {
                const char *np = p + 7;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    char *name = tt_strndup(np, nlen);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                          TT_KIND_CLASS, "julia", line_num,
                                          content, clen, &lo);
                    free(name);
                }
                continue;
            }

            /* "const NAME =" */
            if (strncmp(p, "const ", 6) == 0) {
                const char *np = p + 6;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    const char *after = np + nlen;
                    while (*after == ' ' || *after == '\t') after++;
                    if (*after == '=') {
                        char *name = tt_strndup(np, nlen);
                        tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                              TT_KIND_CONSTANT, "julia", line_num,
                                              content, clen, &lo);
                        free(name);
                    }
                }
                continue;
            }
        }

        tt_str_split_free(lines);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = syms;
    *out_count = cnt;
    return 0;
}
