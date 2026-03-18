/*
 * parser_gdscript.c -- Parser for GDScript (.gd) files.
 *
 * Patterns:
 *   func name(args):          -> FUNCTION
 *   class_name ClassName      -> CLASS
 *   class ClassName:          -> CLASS
 *   signal name(args)         -> PROPERTY
 *   enum Name {               -> ENUM
 *   const NAME =              -> CONSTANT
 *   @export var name          -> VARIABLE (only exported vars)
 */

#include "parser_gdscript.h"
#include "parser_common.h"
#include "str_util.h"
#include "platform.h"
#include "line_offsets.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int tt_parse_gdscript(const char *project_root, const char **file_paths, int file_count,
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

            /* "func name(" */
            if (strncmp(p, "func ", 5) == 0) {
                const char *np = p + 5;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    char *name = tt_strndup(np, nlen);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                          TT_KIND_FUNCTION, "gdscript", line_num,
                                          content, clen, &lo);
                    free(name);
                }
                continue;
            }

            /* "class_name ClassName" */
            if (strncmp(p, "class_name ", 11) == 0) {
                const char *np = p + 11;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    char *name = tt_strndup(np, nlen);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                          TT_KIND_CLASS, "gdscript", line_num,
                                          content, clen, &lo);
                    free(name);
                }
                continue;
            }

            /* "class ClassName:" (inner class) */
            if (strncmp(p, "class ", 6) == 0) {
                const char *np = p + 6;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    char *name = tt_strndup(np, nlen);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                          TT_KIND_CLASS, "gdscript", line_num,
                                          content, clen, &lo);
                    free(name);
                }
                continue;
            }

            /* "signal name(" */
            if (strncmp(p, "signal ", 7) == 0) {
                const char *np = p + 7;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    char *name = tt_strndup(np, nlen);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                          TT_KIND_PROPERTY, "gdscript", line_num,
                                          content, clen, &lo);
                    free(name);
                }
                continue;
            }

            /* "enum Name {" */
            if (strncmp(p, "enum ", 5) == 0) {
                const char *np = p + 5;
                while (*np == ' ' || *np == '\t') np++;
                size_t nlen = tt_parser_read_word(np);
                if (nlen > 0) {
                    char *name = tt_strndup(np, nlen);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                          TT_KIND_ENUM, "gdscript", line_num,
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
                    /* Accept both "const NAME =" and "const NAME:" (typed) */
                    if (*after == '=' || *after == ':') {
                        char *name = tt_strndup(np, nlen);
                        tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                              TT_KIND_CONSTANT, "gdscript", line_num,
                                              content, clen, &lo);
                        free(name);
                    }
                }
                continue;
            }

            /* "@export var name" */
            if (strncmp(p, "@export ", 8) == 0) {
                const char *vp = p + 8;
                while (*vp == ' ' || *vp == '\t') vp++;
                /* Skip optional @export sub-annotations like @export_range */
                if (strncmp(vp, "var ", 4) == 0) {
                    const char *np = vp + 4;
                    while (*np == ' ' || *np == '\t') np++;
                    size_t nlen = tt_parser_read_word(np);
                    if (nlen > 0) {
                        char *name = tt_strndup(np, nlen);
                        tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, p,
                                              TT_KIND_VARIABLE, "gdscript", line_num,
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
