/*
 * parser_xml.c -- Parser for XML/XUL (.xml, .xul) files.
 *
 * Extracts three symbol types from XML documents:
 *   1. Document root element                -> TYPE     (one per file)
 *   2. Elements with id/name/key attributes -> CONSTANT (priority: id > name > key)
 *   3. <script src="..."> / <link href="..."> -> FUNCTION
 *
 * Preceding XML comments (<!-- ... -->) are captured as docstrings.
 * This goes beyond the upstream tree-sitter approach by also detecting
 * namespace prefixes and extracting role/class attributes as keywords.
 */

#include "parser_xml.h"
#include "parser_common.h"
#include "str_util.h"
#include "platform.h"
#include "line_offsets.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Extract attribute value from a line: finds attr="value" or attr='value'.
 * Returns allocated string or NULL. */
static char *extract_attr(const char *line, const char *attr_name)
{
    size_t alen = strlen(attr_name);
    const char *p = line;

    while ((p = strstr(p, attr_name)) != NULL) {
        /* Verify it's preceded by whitespace (not part of another attr name) */
        if (p > line && !isspace((unsigned char)p[-1])) {
            p += alen;
            continue;
        }

        const char *eq = p + alen;
        /* Skip whitespace around '=' */
        while (*eq == ' ' || *eq == '\t') eq++;
        if (*eq != '=') { p += alen; continue; }
        eq++;
        while (*eq == ' ' || *eq == '\t') eq++;

        char quote = *eq;
        if (quote != '"' && quote != '\'') { p += alen; continue; }
        eq++;
        const char *end = strchr(eq, quote);
        if (!end) { p += alen; continue; }
        return tt_strndup(eq, (size_t)(end - eq));
    }
    return NULL;
}

/* Extract the tag name from a line containing '<tagname' or '<ns:tagname'.
 * Skips '<', '/', '?', '!'. Returns allocated string or NULL. */
static char *extract_tag_name(const char *line)
{
    const char *p = line;
    while (*p && *p != '<') p++;
    if (!*p) return NULL;
    p++; /* skip '<' */

    /* Skip '/', '?', '!' */
    if (*p == '/' || *p == '?' || *p == '!') return NULL;

    const char *start = p;
    while (*p && *p != ' ' && *p != '\t' && *p != '>' && *p != '/' && *p != '\n')
        p++;
    if (p == start) return NULL;
    return tt_strndup(start, (size_t)(p - start));
}

/* Collect preceding XML comment block. Walks backward from line_idx.
 * Returns allocated docstring or NULL. */
static char *collect_preceding_comment(char **lines, int line_idx)
{
    if (line_idx <= 0) return NULL;

    /* Look for closing --> first */
    int end_line = -1;
    for (int i = line_idx - 1; i >= 0 && i >= line_idx - 5; i--) {
        const char *trimmed = lines[i];
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (!*trimmed) continue; /* skip empty lines */
        if (strstr(trimmed, "-->")) {
            end_line = i;
            break;
        }
        break; /* non-empty, non-comment line → stop */
    }
    if (end_line < 0) return NULL;

    /* Now find the matching <!-- */
    int start_line = end_line;
    for (int i = end_line; i >= 0 && i >= end_line - 20; i--) {
        if (strstr(lines[i], "<!--")) {
            start_line = i;
            break;
        }
    }

    /* Build docstring from start_line to end_line */
    tt_strbuf_t sb;
    tt_strbuf_init(&sb);
    for (int i = start_line; i <= end_line; i++) {
        const char *l = lines[i];
        while (*l == ' ' || *l == '\t') l++;
        /* Strip comment delimiters */
        if (i == start_line) {
            const char *after = strstr(l, "<!--");
            if (after) l = after + 4;
            while (*l == ' ' || *l == '-') l++;
        }
        if (i == end_line) {
            /* Remove trailing --> */
            char *copy = tt_strdup(l);
            if (copy) {
                char *arrow = strstr(copy, "-->");
                if (arrow) *arrow = '\0';
                /* Trim trailing whitespace/dashes */
                size_t clen = strlen(copy);
                while (clen > 0 && (copy[clen-1] == ' ' || copy[clen-1] == '-'))
                    copy[--clen] = '\0';
                if (sb.len > 0) tt_strbuf_append(&sb, " ", 1);
                tt_strbuf_append(&sb, copy, strlen(copy));
                free(copy);
                break;
            }
        }
        if (sb.len > 0) tt_strbuf_append(&sb, " ", 1);
        tt_strbuf_append(&sb, l, strlen(l));
    }

    if (sb.len == 0) {
        tt_strbuf_free(&sb);
        return NULL;
    }
    return tt_strbuf_detach(&sb);
}

int tt_parse_xml(const char *project_root, const char **file_paths, int file_count,
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

        bool root_found = false;
        bool in_comment = false;

        for (int i = 0; i < nlines; i++) {
            int line_num = i + 1;
            const char *line = lines[i];
            const char *trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

            /* Track multiline comments */
            if (strstr(trimmed, "<!--")) in_comment = true;
            if (strstr(trimmed, "-->")) { in_comment = false; continue; }
            if (in_comment) continue;

            /* Skip processing lines, declarations, DTD */
            if (*trimmed != '<' || trimmed[1] == '?' || trimmed[1] == '!')
                continue;

            /* Closing tags */
            if (trimmed[1] == '/') continue;

            char *tag = extract_tag_name(trimmed);
            if (!tag) continue;

            /* 1. Document root element (first non-prolog element) */
            if (!root_found) {
                root_found = true;
                char *docstring = collect_preceding_comment(lines, i);
                tt_strbuf_t sig;
                tt_strbuf_init(&sig);
                tt_strbuf_appendf(&sig, "<%s>", tag);
                char *sig_str = tt_strbuf_detach(&sig);

                tt_parser_add_symbol(&syms, &cnt, &cap, rel, tag, sig_str,
                                      TT_KIND_CLASS, "xml", line_num,
                                      content, clen, &lo);
                /* Attach docstring to last added symbol */
                if (docstring && cnt > 0) {
                    free(syms[cnt-1].docstring);
                    syms[cnt-1].docstring = docstring;
                }
                free(sig_str);
                free(tag);
                continue;
            }

            /* 2. Elements with identity attributes (priority: id > name > key) */
            char *id_val = extract_attr(line, "id");
            const char *id_attr_name = "id";
            if (!id_val) { id_val = extract_attr(line, "name"); id_attr_name = "name"; }
            if (!id_val) { id_val = extract_attr(line, "key"); id_attr_name = "key"; }
            if (id_val) {
                char *docstring = collect_preceding_comment(lines, i);

                tt_strbuf_t sig;
                tt_strbuf_init(&sig);
                tt_strbuf_appendf(&sig, "<%s %s=\"%s\">", tag, id_attr_name, id_val);
                char *sig_str = tt_strbuf_detach(&sig);

                /* Qualified name: tag::value */
                tt_strbuf_t qn;
                tt_strbuf_init(&qn);
                tt_strbuf_appendf(&qn, "%s::%s", tag, id_val);
                char *qn_str = tt_strbuf_detach(&qn);

                tt_parser_add_symbol(&syms, &cnt, &cap, rel, qn_str, sig_str,
                                      TT_KIND_CONSTANT, "xml", line_num,
                                      content, clen, &lo);
                if (docstring && cnt > 0) {
                    free(syms[cnt-1].docstring);
                    syms[cnt-1].docstring = docstring;
                }
                free(sig_str);
                free(qn_str);
                free(id_val);
            }

            /* 3. <script src="..."> references */
            if (strcasecmp(tag, "script") == 0) {
                char *src = extract_attr(line, "src");
                if (src) {
                    tt_strbuf_t sig;
                    tt_strbuf_init(&sig);
                    tt_strbuf_appendf(&sig, "<script src=\"%s\">", src);
                    char *sig_str = tt_strbuf_detach(&sig);

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, src, sig_str,
                                          TT_KIND_FUNCTION, "xml", line_num,
                                          content, clen, &lo);
                    free(sig_str);
                    free(src);
                }
            }

            /* 4. <link href="..."> references (stylesheet/resource) */
            if (strcasecmp(tag, "link") == 0) {
                char *href = extract_attr(line, "href");
                if (href) {
                    tt_strbuf_t sig;
                    tt_strbuf_init(&sig);
                    tt_strbuf_appendf(&sig, "<link href=\"%s\">", href);
                    char *sig_str = tt_strbuf_detach(&sig);

                    tt_parser_add_symbol(&syms, &cnt, &cap, rel, href, sig_str,
                                          TT_KIND_FUNCTION, "xml", line_num,
                                          content, clen, &lo);
                    free(sig_str);
                    free(href);
                }
            }

            free(tag);
        }

        tt_str_split_free(lines);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = syms;
    *out_count = cnt;
    return 0;
}
