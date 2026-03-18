/*
 * parser_verse.c -- Parser for Verse/UEFN (.verse) files.
 *
 * Patterns:
 *   name := class{...}          -> CLASS
 *   name()<decides><transacts>: type = -> FUNCTION (parenthesized)
 *   name() : type =             -> FUNCTION
 *   name : type = ...           -> VARIABLE (top-level, no parens)
 */

#include "parser_verse.h"
#include "parser_common.h"
#include "str_util.h"
#include "platform.h"
#include "line_offsets.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Check if a line defines a class: "name := class{" or "name := class:" */
static int try_class(const char *p, const char *rel, const char *content,
                     size_t clen, const tt_line_offsets_t *lo,
                     tt_symbol_t **syms, int *cnt, int *cap, int line_num)
{
    const char *start = p;
    size_t nlen = tt_parser_read_word(start);
    if (nlen == 0) return 0;

    const char *after = start + nlen;
    while (*after == ' ' || *after == '\t') after++;

    /* ":=" assignment */
    if (after[0] != ':' || after[1] != '=') return 0;
    after += 2;
    while (*after == ' ' || *after == '\t') after++;

    /* Check if RHS starts with "class" keyword pattern */
    if (strncmp(after, "class", 5) == 0) {
        char ch = after[5];
        if (ch == '{' || ch == '(' || ch == '<' || ch == ':' ||
            ch == ' ' || ch == '\t' || ch == '\0' || ch == '\n') {
            char *name = tt_strndup(start, nlen);
            tt_parser_add_symbol(syms, cnt, cap, rel, name, p,
                                  TT_KIND_CLASS, "verse", line_num,
                                  content, clen, lo);
            free(name);
            return 1;
        }
    }
    return 0;
}

/* Check if a line defines a function: "name(...) : type =" */
static int try_function(const char *p, const char *rel, const char *content,
                        size_t clen, const tt_line_offsets_t *lo,
                        tt_symbol_t **syms, int *cnt, int *cap, int line_num)
{
    const char *start = p;
    size_t nlen = tt_parser_read_word(start);
    if (nlen == 0) return 0;

    const char *after = start + nlen;

    /* Skip optional pre-paren specifiers like <override> */
    while (*after == '<') {
        while (*after && *after != '>') after++;
        if (*after == '>') after++;
    }

    /* Must have parentheses */
    if (*after != '(') return 0;

    /* Find closing paren (possibly after effect specifiers) */
    const char *scan = after;
    int depth = 0;
    while (*scan) {
        if (*scan == '(') depth++;
        else if (*scan == ')') { depth--; if (depth == 0) { scan++; break; } }
        scan++;
    }
    if (depth != 0) return 0;

    /* Skip optional effect specifiers like <decides><transacts> */
    while (*scan == '<') {
        while (*scan && *scan != '>') scan++;
        if (*scan == '>') scan++;
    }
    while (*scan == ' ' || *scan == '\t') scan++;

    /* Expect ": type =" or just ":" for void-like */
    if (*scan == ':') {
        char *name = tt_strndup(start, nlen);
        tt_parser_add_symbol(syms, cnt, cap, rel, name, p,
                              TT_KIND_FUNCTION, "verse", line_num,
                              content, clen, lo);
        free(name);
        return 1;
    }
    return 0;
}

/* Check if a top-level line defines a variable: "name : type = ..." */
static int try_variable(const char *line_start, const char *p, const char *rel,
                        const char *content, size_t clen,
                        const tt_line_offsets_t *lo,
                        tt_symbol_t **syms, int *cnt, int *cap, int line_num)
{
    /* Only top-level (no indentation) */
    if (p != line_start) return 0;

    const char *start = p;
    size_t nlen = tt_parser_read_word(start);
    if (nlen == 0) return 0;

    const char *after = start + nlen;

    /* Must NOT have parens (that's a function) */
    if (*after == '(') return 0;

    while (*after == ' ' || *after == '\t') after++;

    /* ":" followed by type, then "=" */
    if (*after != ':') return 0;
    after++;
    while (*after == ' ' || *after == '\t') after++;

    /* Skip the type name(s) */
    while (*after && *after != '=' && *after != '\n') after++;
    if (*after == '=') {
        char *name = tt_strndup(start, nlen);
        tt_parser_add_symbol(syms, cnt, cap, rel, name, p,
                              TT_KIND_VARIABLE, "verse", line_num,
                              content, clen, lo);
        free(name);
        return 1;
    }
    return 0;
}

int tt_parse_verse(const char *project_root, const char **file_paths, int file_count,
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
            const char *raw = lines[i];
            const char *p = raw;
            while (*p == ' ' || *p == '\t') p++;

            /* Skip empty lines and comments */
            if (*p == '\0' || *p == '#') continue;

            /* Try class first (name := class...) */
            if (try_class(p, rel, content, clen, &lo, &syms, &cnt, &cap, line_num))
                continue;

            /* Try function (name(...) : type =) */
            if (try_function(p, rel, content, clen, &lo, &syms, &cnt, &cap, line_num))
                continue;

            /* Try top-level variable (name : type = ...) */
            try_variable(raw, p, rel, content, clen, &lo, &syms, &cnt, &cap, line_num);
        }

        tt_str_split_free(lines);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = syms;
    *out_count = cnt;
    return 0;
}
