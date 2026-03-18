/*
 * parser_asm.c -- Parser for Assembly (.asm, .s) files.
 *
 * Multi-dialect regex line-scanning. No tree-sitter grammar covers the
 * breadth of assembler dialects, so this uses pattern matching to support
 * WLA-DX, NASM/YASM, GAS (GNU), and CA65 (cc65) in a single pass.
 *
 * Symbol mapping:
 *   Labels (name:)               -> FUNCTION  (local _-prefixed excluded)
 *   Sections (.section, section) -> CLASS
 *   Macros (.macro, %macro)      -> FUNCTION
 *   Constants (.define, equ)     -> CONSTANT
 *   Structs (.struct, struc)     -> TYPE
 *   Procedures (.proc)           -> FUNCTION
 *   Enum members (.enum body)    -> CONSTANT
 *
 * Preceding ; or @ style comments captured as docstrings.
 */

#include "parser_asm.h"
#include "parser_common.h"
#include "str_util.h"
#include "platform.h"
#include "line_offsets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Helper: is c a valid identifier char? */
static bool is_ident(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '.';
}

/* Helper: read an identifier starting at p. Returns length. */
static size_t read_ident(const char *p)
{
    size_t n = 0;
    while (is_ident(p[n]))
        n++;
    return n;
}

/* Helper: skip whitespace at p. */
static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t')
        p++;
    return p;
}

/* Check if line starts with a directive (case-insensitive).
 * dir must include the leading dot, e.g. ".section".
 * Returns pointer past the directive + whitespace, or NULL. */
static const char *match_directive(const char *line, const char *dir)
{
    const char *p = skip_ws(line);
    size_t dlen = strlen(dir);
    if (tt_strncasecmp(p, dir, dlen) != 0)
        return NULL;
    char next = p[dlen];
    if (next && !isspace((unsigned char)next) && next != '"')
        return NULL;
    return skip_ws(p + dlen);
}

/* Check for NASM-style %directive (e.g. %define, %macro). */
static const char *match_percent_directive(const char *line, const char *dir)
{
    const char *p = skip_ws(line);
    if (*p != '%')
        return NULL;
    p++;
    size_t dlen = strlen(dir);
    if (tt_strncasecmp(p, dir, dlen) != 0)
        return NULL;
    char next = p[dlen];
    if (next && !isspace((unsigned char)next))
        return NULL;
    return skip_ws(p + dlen);
}

/* Collect pending comment lines into a docstring. Returns allocated string or NULL. */
static char *flush_comments(char **lines, int *comment_start, int comment_end)
{
    if (*comment_start < 0 || *comment_start > comment_end)
    {
        *comment_start = -1;
        return NULL;
    }

    tt_strbuf_t sb;
    tt_strbuf_init(&sb);
    for (int i = *comment_start; i <= comment_end; i++)
    {
        const char *l = lines[i];
        const char *p = skip_ws(l);
        if (*p == ';' || *p == '@')
        {
            p++;
            if (*p == ' ')
                p++;
        }
        if (sb.len > 0)
            tt_strbuf_append(&sb, " ", 1);
        size_t tlen = strlen(p);
        /* Trim trailing whitespace */
        while (tlen > 0 && (p[tlen - 1] == ' ' || p[tlen - 1] == '\t' ||
                            p[tlen - 1] == '\r' || p[tlen - 1] == '\n'))
            tlen--;
        tt_strbuf_append(&sb, p, tlen);
    }
    *comment_start = -1;
    if (sb.len == 0)
    {
        tt_strbuf_free(&sb);
        return NULL;
    }
    return tt_strbuf_detach(&sb);
}

int tt_parse_asm(const char *project_root, const char **file_paths, int file_count,
                 tt_symbol_t **out, int *out_count)
{
    if (!out || !out_count)
        return -1;
    *out = NULL;
    *out_count = 0;
    if (!project_root || !file_paths || file_count <= 0)
        return 0;

    int cap = 32, cnt = 0;
    tt_symbol_t *syms = malloc((size_t)cap * sizeof(tt_symbol_t));
    if (!syms)
        return -1;

    for (int fi = 0; fi < file_count; fi++)
    {
        const char *rel = file_paths[fi];
        char *full = tt_path_join(project_root, rel);
        if (!full)
            continue;

        size_t clen = 0;
        char *content = tt_read_file(full, &clen);
        free(full);
        if (!content)
            continue;

        tt_line_offsets_t lo;
        tt_line_offsets_build(&lo, content, clen);

        int nlines = 0;
        char **lines = tt_str_split(content, '\n', &nlines);

        /* State */
        bool in_struct = false;
        bool in_enum = false;
        bool in_macro = false;
        bool in_block_comment = false;
        int comment_start = -1;
        int comment_end = -1;
        char current_section[128] = "";

        for (int i = 0; i < nlines; i++)
        {
            int line_num = i + 1;
            const char *line = lines[i];
            const char *trimmed = skip_ws(line);

            /* C-style block comment tracking */
            if (in_block_comment)
            {
                if (strstr(trimmed, "*/"))
                    in_block_comment = false;
                continue;
            }
            if (trimmed[0] == '/' && trimmed[1] == '*')
            {
                if (!strstr(trimmed + 2, "*/"))
                    in_block_comment = true;
                continue;
            }

            /* Blank lines reset comments */
            if (!*trimmed || *trimmed == '\r' || *trimmed == '\n')
            {
                comment_start = -1;
                continue;
            }

            /* Comment lines (;comment or @comment) */
            if (*trimmed == ';' || (*trimmed == '@' && !isalpha((unsigned char)trimmed[1])))
            {
                if (!in_struct && !in_enum)
                {
                    if (comment_start < 0)
                        comment_start = i;
                    comment_end = i;
                }
                continue;
            }

            /* ---- Struct end ---- */
            if (in_struct)
            {
                if (match_directive(line, ".endst") ||
                    match_directive(line, ".ends") ||
                    match_directive(line, "endstruc"))
                {
                    in_struct = false;
                    comment_start = -1;
                }
                continue;
            }

            /* ---- Struct start ---- */
            const char *after;
            after = match_directive(line, ".struct");
            if (!after)
                after = match_directive(line, "struc");
            if (after && !in_macro)
            {
                size_t nlen = read_ident(after);
                if (nlen > 0)
                {
                    char *name = tt_strndup(after, nlen);
                    char *docstring = flush_comments(lines, &comment_start, comment_end);
                    tt_strbuf_t sig;
                    tt_strbuf_init(&sig);
                    tt_strbuf_appendf(&sig, ".struct %s", name);
                    char *sig_str = tt_strbuf_detach(&sig);

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig_str,
                                         TT_KIND_TYPE, "asm", line_num,
                                         content, clen, &lo);
                    if (docstring && cnt > 0)
                    {
                        free(syms[cnt - 1].docstring);
                        syms[cnt - 1].docstring = docstring;
                    }
                    free(sig_str);
                    free(name);
                    in_struct = true;
                    continue;
                }
            }

            /* ---- Enum end ---- */
            if (in_enum)
            {
                if (match_directive(line, ".ende"))
                {
                    in_enum = false;
                    comment_start = -1;
                    continue;
                }
                /* Enum members: NAME db/dw/dl/ds (WLA-DX style) */
                const char *p = skip_ws(line);
                size_t nlen = read_ident(p);
                if (nlen > 0 && isalpha((unsigned char)p[0]))
                {
                    const char *rest = skip_ws(p + nlen);
                    if (tt_strncasecmp(rest, "db", 2) == 0 ||
                        tt_strncasecmp(rest, "dw", 2) == 0 ||
                        tt_strncasecmp(rest, "dl", 2) == 0 ||
                        tt_strncasecmp(rest, "ds", 2) == 0)
                    {
                        char *name = tt_strndup(p, nlen);
                        tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, name,
                                             TT_KIND_CONSTANT, "asm", line_num,
                                             content, clen, &lo);
                        free(name);
                    }
                }
                comment_start = -1;
                continue;
            }

            /* ---- Enum start ---- */
            if (match_directive(line, ".enum") && !in_macro)
            {
                in_enum = true;
                comment_start = -1;
                continue;
            }

            /* ---- Macro end ---- */
            if (in_macro)
            {
                if (match_directive(line, ".endm") ||
                    match_directive(line, ".endmacro") ||
                    match_percent_directive(line, "endmacro"))
                {
                    in_macro = false;
                    comment_start = -1;
                }
                continue;
            }

            /* ---- Macro start ---- */
            after = match_directive(line, ".macro");
            if (!after)
                after = match_percent_directive(line, "macro");
            if (after)
            {
                size_t nlen = read_ident(after);
                if (nlen > 0)
                {
                    char *name = tt_strndup(after, nlen);
                    char *docstring = flush_comments(lines, &comment_start, comment_end);
                    tt_strbuf_t sig;
                    tt_strbuf_init(&sig);
                    tt_strbuf_appendf(&sig, ".macro %s", name);
                    char *sig_str = tt_strbuf_detach(&sig);

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig_str,
                                         TT_KIND_FUNCTION, "asm", line_num,
                                         content, clen, &lo);
                    if (docstring && cnt > 0)
                    {
                        free(syms[cnt - 1].docstring);
                        syms[cnt - 1].docstring = docstring;
                    }
                    free(sig_str);
                    free(name);
                    in_macro = true;
                    continue;
                }
            }

            /* ---- Section / segment ---- */
            /* WLA-DX: .section "name" */
            after = match_directive(line, ".section");
            if (!after)
                after = match_directive(line, ".ramsection");
            if (after && *after == '"')
            {
                const char *end = strchr(after + 1, '"');
                if (end)
                {
                    char *name = tt_strndup(after + 1, (size_t)(end - after - 1));
                    char *docstring = flush_comments(lines, &comment_start, comment_end);
                    snprintf(current_section, sizeof(current_section), "%s", name);

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, lines[i],
                                         TT_KIND_CLASS, "asm", line_num,
                                         content, clen, &lo);
                    if (docstring && cnt > 0)
                    {
                        free(syms[cnt - 1].docstring);
                        syms[cnt - 1].docstring = docstring;
                    }
                    free(name);
                    continue;
                }
            }
            /* NASM: section .text */
            after = match_directive(line, "section");
            if (after && *after == '.')
            {
                size_t nlen = read_ident(after);
                if (nlen > 0)
                {
                    char *name = tt_strndup(after, nlen);
                    char *docstring = flush_comments(lines, &comment_start, comment_end);
                    snprintf(current_section, sizeof(current_section), "%s", name);

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, lines[i],
                                         TT_KIND_CLASS, "asm", line_num,
                                         content, clen, &lo);
                    if (docstring && cnt > 0)
                    {
                        free(syms[cnt - 1].docstring);
                        syms[cnt - 1].docstring = docstring;
                    }
                    free(name);
                    continue;
                }
            }
            /* CA65: .segment "CODE" */
            after = match_directive(line, ".segment");
            if (after && *after == '"')
            {
                const char *end = strchr(after + 1, '"');
                if (end)
                {
                    char *name = tt_strndup(after + 1, (size_t)(end - after - 1));
                    char *docstring = flush_comments(lines, &comment_start, comment_end);
                    snprintf(current_section, sizeof(current_section), "%s", name);

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, lines[i],
                                         TT_KIND_CLASS, "asm", line_num,
                                         content, clen, &lo);
                    if (docstring && cnt > 0)
                    {
                        free(syms[cnt - 1].docstring);
                        syms[cnt - 1].docstring = docstring;
                    }
                    free(name);
                    continue;
                }
            }
            /* GAS: .text, .data, .bss as sections */
            if (match_directive(line, ".text") ||
                match_directive(line, ".data") ||
                match_directive(line, ".bss"))
            {
                /* Extract directive name */
                const char *p = skip_ws(line);
                p++; /* skip '.' */
                size_t nlen = 0;
                while (isalpha((unsigned char)p[nlen]))
                    nlen++;
                char sec_name[16];
                snprintf(sec_name, sizeof(sec_name), ".%.*s", (int)nlen, p);
                snprintf(current_section, sizeof(current_section), "%s", sec_name);
                /* Don't emit a symbol for .text/.data/.bss (too noisy) */
                comment_start = -1;
                continue;
            }

            /* ---- Section end (.ends) ---- */
            if (match_directive(line, ".ends"))
            {
                current_section[0] = '\0';
                comment_start = -1;
                continue;
            }

            /* ---- Procedure (.proc NAME) ---- */
            after = match_directive(line, ".proc");
            if (after)
            {
                size_t nlen = read_ident(after);
                if (nlen > 0)
                {
                    char *name = tt_strndup(after, nlen);
                    char *docstring = flush_comments(lines, &comment_start, comment_end);
                    tt_strbuf_t sig;
                    tt_strbuf_init(&sig);
                    tt_strbuf_appendf(&sig, ".proc %s", name);
                    char *sig_str = tt_strbuf_detach(&sig);

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig_str,
                                         TT_KIND_FUNCTION, "asm", line_num,
                                         content, clen, &lo);
                    if (docstring && cnt > 0)
                    {
                        free(syms[cnt - 1].docstring);
                        syms[cnt - 1].docstring = docstring;
                    }
                    free(sig_str);
                    free(name);
                    continue;
                }
            }

            /* ---- Constants ---- */
            /* WLA-DX: .define NAME value, .def NAME value */
            after = match_directive(line, ".define");
            if (!after)
                after = match_directive(line, ".def");
            /* GAS: .set NAME, value / .equ NAME, value */
            if (!after)
                after = match_directive(line, ".set");
            if (!after)
                after = match_directive(line, ".equ");
            if (after)
            {
                size_t nlen = read_ident(after);
                if (nlen > 0)
                {
                    char *name = tt_strndup(after, nlen);
                    const char *val_start = skip_ws(after + nlen);
                    if (*val_start == ',')
                        val_start = skip_ws(val_start + 1);

                    tt_strbuf_t sig;
                    tt_strbuf_init(&sig);
                    /* Capture value up to comment or end */
                    size_t vlen = 0;
                    while (val_start[vlen] && val_start[vlen] != ';' &&
                           val_start[vlen] != '\n' && val_start[vlen] != '\r')
                        vlen++;
                    while (vlen > 0 && (val_start[vlen - 1] == ' ' || val_start[vlen - 1] == '\t'))
                        vlen--;
                    if (vlen > 0)
                        tt_strbuf_appendf(&sig, "%s = %.*s", name, (int)vlen, val_start);
                    else
                        tt_strbuf_append_str(&sig, name);
                    char *sig_str = tt_strbuf_detach(&sig);

                    char *docstring = flush_comments(lines, &comment_start, comment_end);
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig_str,
                                         TT_KIND_CONSTANT, "asm", line_num,
                                         content, clen, &lo);
                    if (docstring && cnt > 0)
                    {
                        free(syms[cnt - 1].docstring);
                        syms[cnt - 1].docstring = docstring;
                    }
                    free(sig_str);
                    free(name);
                    continue;
                }
            }
            /* NASM: %define NAME value */
            after = match_percent_directive(line, "define");
            if (after)
            {
                size_t nlen = read_ident(after);
                if (nlen > 0)
                {
                    char *name = tt_strndup(after, nlen);
                    char *docstring = flush_comments(lines, &comment_start, comment_end);

                    tt_strbuf_t sig;
                    tt_strbuf_init(&sig);
                    tt_strbuf_appendf(&sig, "%%define %s", name);
                    char *sig_str = tt_strbuf_detach(&sig);

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig_str,
                                         TT_KIND_CONSTANT, "asm", line_num,
                                         content, clen, &lo);
                    if (docstring && cnt > 0)
                    {
                        free(syms[cnt - 1].docstring);
                        syms[cnt - 1].docstring = docstring;
                    }
                    free(sig_str);
                    free(name);
                    continue;
                }
            }
            /* EQU constant: NAME equ VALUE or NAME = VALUE */
            {
                const char *p = skip_ws(line);
                size_t nlen = read_ident(p);
                if (nlen > 0 && isalpha((unsigned char)p[0]))
                {
                    const char *rest = skip_ws(p + nlen);
                    bool is_equ = (tt_strncasecmp(rest, "equ", 3) == 0 &&
                                   (rest[3] == ' ' || rest[3] == '\t'));
                    bool is_eq = (*rest == '=');
                    if (is_equ || is_eq)
                    {
                        const char *val = skip_ws(rest + (is_equ ? 3 : 1));
                        size_t vlen = 0;
                        while (val[vlen] && val[vlen] != ';' &&
                               val[vlen] != '\n' && val[vlen] != '\r')
                            vlen++;
                        while (vlen > 0 && (val[vlen - 1] == ' ' || val[vlen - 1] == '\t'))
                            vlen--;

                        char *name = tt_strndup(p, nlen);
                        char *docstring = flush_comments(lines, &comment_start, comment_end);

                        tt_strbuf_t sig;
                        tt_strbuf_init(&sig);
                        if (vlen > 0)
                            tt_strbuf_appendf(&sig, "%s = %.*s", name, (int)vlen, val);
                        else
                            tt_strbuf_append_str(&sig, name);
                        char *sig_str = tt_strbuf_detach(&sig);

                        tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig_str,
                                             TT_KIND_CONSTANT, "asm", line_num,
                                             content, clen, &lo);
                        if (docstring && cnt > 0)
                        {
                            free(syms[cnt - 1].docstring);
                            syms[cnt - 1].docstring = docstring;
                        }
                        free(sig_str);
                        free(name);
                        continue;
                    }
                }
            }

            /* ---- Labels (NAME: at column 0) ---- */
            if (isalpha((unsigned char)line[0]))
            {
                size_t nlen = read_ident(line);
                if (nlen > 0)
                {
                    const char *after_name = line + nlen;
                    while (*after_name == ' ' || *after_name == '\t')
                        after_name++;
                    if (*after_name == ':')
                    {
                        /* Skip _prefixed local labels */
                        if (line[0] == '_')
                        {
                            comment_start = -1;
                            continue;
                        }
                        char *name = tt_strndup(line, nlen);
                        char *docstring = flush_comments(lines, &comment_start, comment_end);

                        tt_strbuf_t sig;
                        tt_strbuf_init(&sig);
                        tt_strbuf_appendf(&sig, "%s:", name);
                        char *sig_str = tt_strbuf_detach(&sig);

                        tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig_str,
                                             TT_KIND_FUNCTION, "asm", line_num,
                                             content, clen, &lo);
                        if (docstring && cnt > 0)
                        {
                            free(syms[cnt - 1].docstring);
                            syms[cnt - 1].docstring = docstring;
                        }
                        free(sig_str);
                        free(name);
                        continue;
                    }
                }
            }

            /* Non-matching line: reset comments */
            comment_start = -1;
        }

        tt_str_split_free(lines);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = syms;
    *out_count = cnt;
    return 0;
}
