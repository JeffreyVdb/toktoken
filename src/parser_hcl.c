/*
 * parser_hcl.c -- Parser for HCL/Terraform (.tf, .hcl, .tfvars) files.
 *
 * Patterns:
 *   resource "type" "name" {  -> CLASS, name="type.name"
 *   variable "name" {         -> VARIABLE
 *   module "name" {           -> CLASS
 *   output "name" {           -> PROPERTY
 *   provider "name" {         -> CLASS
 *   data "type" "name" {      -> PROPERTY, name="type.name"
 *   locals {                  -> VARIABLE, name="locals"
 *   terraform {               -> CLASS, name="terraform"
 */

#include "parser_hcl.h"
#include "parser_common.h"
#include "str_util.h"
#include "platform.h"
#include "line_offsets.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Extract a quoted string at *p (after opening "). Returns alloc'd string, advances *p. */
static char *read_quoted(const char **p)
{
    if (**p != '"') return NULL;
    (*p)++;
    const char *start = *p;
    while (**p && **p != '"') (*p)++;
    if (**p != '"') return NULL;
    char *s = tt_strndup(start, (size_t)(*p - start));
    (*p)++;
    return s;
}

/* Skip whitespace. */
static void skip_ws(const char **p)
{
    while (**p == ' ' || **p == '\t') (*p)++;
}

int tt_parse_hcl(const char *project_root, const char **file_paths, int file_count,
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

            /* Skip leading whitespace (only top-level blocks) */
            int indent = 0;
            while (*p == ' ' || *p == '\t') { indent++; p++; }
            if (indent > 2) continue; /* skip nested blocks */

            /* Read keyword */
            size_t kw_len = tt_parser_read_word(p);
            if (kw_len == 0) continue;

            char *keyword = tt_strndup(p, kw_len);
            if (!keyword) continue;
            p += kw_len;
            skip_ws(&p);

            /* "terraform {" or "locals {" — no quoted args */
            if ((strcmp(keyword, "terraform") == 0 || strcmp(keyword, "locals") == 0) &&
                *p == '{') {
                tt_symbol_kind_e kind = (strcmp(keyword, "terraform") == 0)
                    ? TT_KIND_CLASS : TT_KIND_VARIABLE;
                tt_parser_add_symbol(&syms, &cnt, &cap, rel, keyword, keyword,
                                      kind, "terraform", line_num, content, clen, &lo);
                free(keyword);
                continue;
            }

            /* "resource" and "data" take two quoted args */
            if (strcmp(keyword, "resource") == 0 || strcmp(keyword, "data") == 0) {
                char *type = read_quoted(&p);
                skip_ws(&p);
                char *name = read_quoted(&p);
                if (type && name) {
                    tt_strbuf_t sb;
                    tt_strbuf_init(&sb);
                    tt_strbuf_appendf(&sb, "%s.%s", type, name);
                    char *qname = tt_strbuf_detach(&sb);

                    tt_strbuf_t sig;
                    tt_strbuf_init(&sig);
                    tt_strbuf_appendf(&sig, "%s \"%s\" \"%s\"", keyword, type, name);
                    char *sig_str = tt_strbuf_detach(&sig);

                    tt_symbol_kind_e kind = (strcmp(keyword, "resource") == 0)
                        ? TT_KIND_CLASS : TT_KIND_PROPERTY;
                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, qname, sig_str,
                                          kind, "terraform", line_num, content, clen, &lo);
                    free(qname);
                    free(sig_str);
                }
                free(type);
                free(name);
                free(keyword);
                continue;
            }

            /* Single quoted arg: variable, module, output, provider */
            if (strcmp(keyword, "variable") == 0 || strcmp(keyword, "module") == 0 ||
                strcmp(keyword, "output") == 0 || strcmp(keyword, "provider") == 0) {
                char *name = read_quoted(&p);
                if (name) {
                    tt_strbuf_t sig;
                    tt_strbuf_init(&sig);
                    tt_strbuf_appendf(&sig, "%s \"%s\"", keyword, name);
                    char *sig_str = tt_strbuf_detach(&sig);

                    tt_symbol_kind_e kind;
                    if (strcmp(keyword, "variable") == 0) kind = TT_KIND_VARIABLE;
                    else if (strcmp(keyword, "output") == 0) kind = TT_KIND_PROPERTY;
                    else kind = TT_KIND_CLASS;

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, name, sig_str,
                                          kind, "terraform", line_num, content, clen, &lo);
                    free(sig_str);
                }
                free(name);
            }

            free(keyword);
        }

        tt_str_split_free(lines);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = syms;
    *out_count = cnt;
    return 0;
}
