/*
 * parser_graphql.c -- Parser for GraphQL (.graphql, .gql) files.
 *
 * Patterns:
 *   type Name {          -> CLASS
 *   interface Name {     -> INTERFACE
 *   union Name =         -> CLASS
 *   enum Name {          -> ENUM
 *   input Name {         -> CLASS
 *   scalar Name          -> CLASS
 *   query/mutation/subscription Name( -> FUNCTION
 *   fragment Name on     -> FUNCTION
 *   extend type Name {   -> CLASS
 */

#include "parser_graphql.h"
#include "parser_common.h"
#include "str_util.h"
#include "platform.h"
#include "line_offsets.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Check if line starts with keyword followed by a name */
static bool match_keyword_name(const char *line, const char *keyword,
                                char **out_name, char **out_sig)
{
    size_t kw_len = strlen(keyword);
    if (strncmp(line, keyword, kw_len) != 0) return false;
    char c = line[kw_len];
    if (c != ' ' && c != '\t') return false;

    const char *p = line + kw_len;
    while (*p == ' ' || *p == '\t') p++;

    size_t name_len = tt_parser_read_word(p);
    if (name_len == 0) return false;

    *out_name = tt_strndup(p, name_len);
    *out_sig = tt_strdup(line);
    /* Truncate sig to reasonable length */
    if (*out_sig && strlen(*out_sig) > 80) (*out_sig)[80] = '\0';
    return true;
}

int tt_parse_graphql(const char *project_root, const char **file_paths, int file_count,
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
            const char *line = lines[i];

            /* Skip leading whitespace */
            while (*line == ' ' || *line == '\t') line++;
            if (*line == '#' || *line == '\0') continue;

            char *name = NULL, *sig = NULL;

            /* "extend type Name" */
            if (strncmp(line, "extend ", 7) == 0) {
                const char *rest = line + 7;
                while (*rest == ' ' || *rest == '\t') rest++;
                if (match_keyword_name(rest, "type", &name, &sig)) {
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig,
                                          TT_KIND_CLASS, "graphql", line_num,
                                          content, clen, &lo);
                    free(name); free(sig);
                    continue;
                }
            }

            /* type, interface, input */
            if (match_keyword_name(line, "type", &name, &sig) ||
                match_keyword_name(line, "input", &name, &sig)) {
                tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig,
                                      TT_KIND_CLASS, "graphql", line_num,
                                      content, clen, &lo);
                free(name); free(sig); continue;
            }

            if (match_keyword_name(line, "interface", &name, &sig)) {
                tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig,
                                      TT_KIND_INTERFACE, "graphql", line_num,
                                      content, clen, &lo);
                free(name); free(sig); continue;
            }

            if (match_keyword_name(line, "enum", &name, &sig)) {
                tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig,
                                      TT_KIND_ENUM, "graphql", line_num,
                                      content, clen, &lo);
                free(name); free(sig); continue;
            }

            /* union, scalar */
            if (match_keyword_name(line, "union", &name, &sig) ||
                match_keyword_name(line, "scalar", &name, &sig)) {
                tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig,
                                      TT_KIND_CLASS, "graphql", line_num,
                                      content, clen, &lo);
                free(name); free(sig); continue;
            }

            /* query, mutation, subscription */
            if (match_keyword_name(line, "query", &name, &sig) ||
                match_keyword_name(line, "mutation", &name, &sig) ||
                match_keyword_name(line, "subscription", &name, &sig)) {
                tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig,
                                      TT_KIND_FUNCTION, "graphql", line_num,
                                      content, clen, &lo);
                free(name); free(sig); continue;
            }

            /* fragment Name on Type */
            if (match_keyword_name(line, "fragment", &name, &sig)) {
                tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig,
                                      TT_KIND_FUNCTION, "graphql", line_num,
                                      content, clen, &lo);
                free(name); free(sig); continue;
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
