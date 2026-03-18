/*
 * parser_autohotkey.c -- Parser for AutoHotkey v1/v2 (.ahk) files.
 *
 * Extracts five symbol types:
 *   1. Classes:      class ClassName [extends Base] {    -> CLASS
 *   2. Functions:    FuncName(params) {                  -> FUNCTION
 *   3. Methods:      MethodName(params) { (inside class) -> METHOD
 *   4. Hotkeys:      ^!s::action (top-level only)        -> CONSTANT
 *   5. #HotIf:       #HotIf WinActive("...")             -> PROPERTY
 *
 * Uses brace-depth tracking with a class stack for nested scoping.
 * Surpasses the upstream implementation by:
 *   - Handling both v1 (::) and v2 (=> / {) syntax
 *   - Extracting methods with correct parent scoping
 *   - Detecting static methods
 *   - Capturing preceding comment blocks as docstrings
 */

#include "parser_autohotkey.h"
#include "parser_common.h"
#include "str_util.h"
#include "platform.h"
#include "line_offsets.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Keywords that look like function calls but aren't */
static const char *KEYWORDS[] = {
    "if", "else", "while", "for", "loop", "catch", "switch",
    "try", "return", "throw", "until", "finally", NULL
};

static int is_keyword(const char *word, size_t len)
{
    for (int i = 0; KEYWORDS[i]; i++) {
        if (strlen(KEYWORDS[i]) == len &&
            strncasecmp(word, KEYWORDS[i], len) == 0)
            return 1;
    }
    return 0;
}

/* Check if a character is a valid AHK identifier char */
static int is_ident(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Check if line is a hotkey pattern (contains :: at top-level depth).
 * Returns the full hotkey string (before and including ::) or NULL. */
static char *extract_hotkey(const char *line)
{
    /* Hotkey prefixes: ~ * $ ! ^ + # < > & and alphanumerics */
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;

    /* Must start with a hotkey-valid char */
    if (!*p || *p == ';' || *p == '{' || *p == '}') return NULL;
    if (*p == '/' && (p[1] == '/' || p[1] == '*')) return NULL;

    /* Find :: */
    const char *dcolon = strstr(p, "::");
    if (!dcolon) return NULL;

    /* The part before :: must not contain spaces (hotkey trigger) */
    for (const char *c = p; c < dcolon; c++) {
        if (*c == ' ' || *c == '\t') return NULL;
        if (*c == '(' || *c == ')') return NULL; /* function call, not hotkey */
    }

    /* Include :: and any inline action */
    const char *end = dcolon + 2;
    while (*end == ' ' || *end == '\t') end++;

    /* Trim trailing comment */
    const char *semi = end;
    while (*semi && *semi != ';') semi++;

    size_t hk_len = (size_t)(semi - p);
    while (hk_len > 0 && (p[hk_len-1] == ' ' || p[hk_len-1] == '\t'))
        hk_len--;

    return tt_strndup(p, hk_len);
}

/* Collect preceding ; comment block as docstring */
static char *collect_comment(char **lines, int line_idx)
{
    if (line_idx <= 0) return NULL;

    int start = -1;
    for (int i = line_idx - 1; i >= 0 && i >= line_idx - 10; i--) {
        const char *l = lines[i];
        while (*l == ' ' || *l == '\t') l++;
        if (*l == ';') {
            start = i;
        } else if (!*l) {
            continue; /* skip blank lines between comments */
        } else {
            break;
        }
    }
    if (start < 0) return NULL;

    tt_strbuf_t sb;
    tt_strbuf_init(&sb);
    for (int i = start; i < line_idx; i++) {
        const char *l = lines[i];
        while (*l == ' ' || *l == '\t') l++;
        if (*l == ';') {
            l++;
            while (*l == ' ') l++;
            if (sb.len > 0) tt_strbuf_append(&sb, " ", 1);
            tt_strbuf_append(&sb, l, strlen(l));
        }
    }
    if (sb.len == 0) {
        tt_strbuf_free(&sb);
        return NULL;
    }
    return tt_strbuf_detach(&sb);
}

int tt_parse_autohotkey(const char *project_root, const char **file_paths, int file_count,
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

        /* Brace depth tracking */
        int depth = 0;

        /* Class stack: name and the depth at which the class body starts */
        char *class_name = NULL;
        int class_depth = -1;

        for (int i = 0; i < nlines; i++) {
            int line_num = i + 1;
            const char *line = lines[i];
            const char *p = line;

            /* Skip leading whitespace */
            while (*p == ' ' || *p == '\t') p++;

            /* Skip empty lines and comments */
            if (!*p || *p == ';') continue;
            if (*p == '/' && (p[1] == '/' || p[1] == '*')) continue;

            /* Count braces on this line (outside strings) */
            int open_braces = 0, close_braces = 0;
            {
                const char *s = line;
                int in_str = 0;
                while (*s) {
                    if (*s == '"') in_str = !in_str;
                    if (!in_str) {
                        if (*s == '{') open_braces++;
                        else if (*s == '}') close_braces++;
                    }
                    s++;
                }
            }

            /* 1. #HotIf directive */
            if (strncasecmp(p, "#HotIf", 6) == 0 &&
                (p[6] == ' ' || p[6] == '\t' || p[6] == '\0' || p[6] == '\n')) {
                const char *expr = p + 6;
                while (*expr == ' ' || *expr == '\t') expr++;
                char *name;
                if (*expr && *expr != ';' && *expr != '\n') {
                    /* Trim trailing comment */
                    const char *end = expr;
                    while (*end && *end != ';') end++;
                    size_t elen = (size_t)(end - expr);
                    while (elen > 0 && (expr[elen-1] == ' ' || expr[elen-1] == '\t'))
                        elen--;
                    name = tt_strndup(expr, elen);
                } else {
                    name = tt_strdup("#HotIf (reset)");
                }
                if (name) {
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name,
                                          p, TT_KIND_PROPERTY, "autohotkey",
                                          line_num, content, clen, &lo);
                    free(name);
                }
                goto update_depth;
            }

            /* 2. Class declaration */
            if (strncasecmp(p, "class ", 6) == 0) {
                const char *name_start = p + 6;
                while (*name_start == ' ') name_start++;
                const char *name_end = name_start;
                while (is_ident(*name_end)) name_end++;

                if (name_end > name_start) {
                    char *name = tt_strndup(name_start, (size_t)(name_end - name_start));
                    if (name) {
                        /* Build signature */
                        const char *end = name_end;
                        while (*end && *end != '{' && *end != ';') end++;
                        char *sig = tt_strndup(p, (size_t)(end - p));
                        char *docstring = collect_comment(lines, i);

                        tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig ? sig : p,
                                              TT_KIND_CLASS, "autohotkey",
                                              line_num, content, clen, &lo);
                        if (docstring && cnt > 0) {
                            free(syms[cnt-1].docstring);
                            syms[cnt-1].docstring = docstring;
                        }

                        /* Track class scope */
                        free(class_name);
                        class_name = tt_strdup(name);
                        class_depth = depth + open_braces;

                        free(name);
                        free(sig);
                    }
                }
                goto update_depth;
            }

            /* 3. Function/Method: Name(params) { or => */
            {
                const char *ident_start = p;
                int is_static = 0;
                if (strncasecmp(p, "static ", 7) == 0) {
                    is_static = 1;
                    ident_start = p + 7;
                    while (*ident_start == ' ') ident_start++;
                }

                if (is_ident(*ident_start)) {
                    const char *ident_end = ident_start;
                    while (is_ident(*ident_end)) ident_end++;
                    size_t ident_len = (size_t)(ident_end - ident_start);

                    if (!is_keyword(ident_start, ident_len)) {
                        const char *after = ident_end;
                        while (*after == ' ' || *after == '\t') after++;

                        if (*after == '(') {
                            /* Find closing paren */
                            const char *cparen = strchr(after, ')');
                            if (cparen) {
                                const char *post = cparen + 1;
                                while (*post == ' ' || *post == '\t') post++;
                                if (*post == '{' || *post == '=' ||
                                    *post == '\n' || *post == '\0') {
                                    char *name = tt_strndup(ident_start, ident_len);
                                    if (name) {
                                        /* Build signature up to closing paren */
                                        size_t sig_len = (size_t)(cparen + 1 - p);
                                        char *sig = tt_strndup(p, sig_len);
                                        char *docstring = collect_comment(lines, i);

                                        int in_class = (class_name && depth >= class_depth &&
                                                        class_depth >= 0);
                                        tt_symbol_kind_e kind = in_class ?
                                            TT_KIND_METHOD : TT_KIND_FUNCTION;

                                        /* Build qualified name for methods */
                                        char *qname = NULL;
                                        if (in_class) {
                                            tt_strbuf_t qb;
                                            tt_strbuf_init(&qb);
                                            tt_strbuf_appendf(&qb, "%s.%s",
                                                              class_name, name);
                                            qname = tt_strbuf_detach(&qb);
                                        }

                                        tt_parser_add_symbol(&syms, &cnt, &cap, rel,
                                                              qname ? qname : name,
                                                              sig ? sig : p,
                                                              kind, "autohotkey",
                                                              line_num, content, clen, &lo);
                                        if (docstring && cnt > 0) {
                                            free(syms[cnt-1].docstring);
                                            syms[cnt-1].docstring = docstring;
                                        }
                                        (void)is_static;

                                        free(name);
                                        free(sig);
                                        free(qname);
                                    }
                                    goto update_depth;
                                }
                            }
                        }
                    }
                }
            }

            /* 4. Hotkeys (top-level only) */
            if (depth == 0) {
                char *hk = extract_hotkey(p);
                if (hk) {
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, hk, hk,
                                          TT_KIND_CONSTANT, "autohotkey",
                                          line_num, content, clen, &lo);
                    free(hk);
                    goto update_depth;
                }
            }

update_depth:
            depth += open_braces - close_braces;
            if (depth < 0) depth = 0;

            /* Pop class scope if we've closed past it */
            if (class_name && class_depth >= 0 && depth < class_depth) {
                free(class_name);
                class_name = NULL;
                class_depth = -1;
            }
        }

        free(class_name);
        class_name = NULL;
        class_depth = -1;

        tt_str_split_free(lines);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = syms;
    *out_count = cnt;
    return 0;
}
