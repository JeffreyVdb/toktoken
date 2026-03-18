/*
 * parser_ejs.c -- Parser for EJS template files.
 *
 * Scans .ejs files for include directives.
 * Pattern: <%[-=]? include('path') %>
 * Manual pattern matching (no PCRE2 required).
 */

#include "parser_ejs.h"
#include "symbol.h"
#include "symbol_kind.h"
#include "line_offsets.h"
#include "fast_hash.h"
#include "str_util.h"
#include "platform.h"
#include "hashmap.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Get byte offset and length of a 1-indexed line. */
static void get_line_bytes(const tt_line_offsets_t *lo, int line_num,
                           size_t content_len,
                           int *out_offset, int *out_length)
{
    *out_offset = tt_line_offsets_offset_at(lo, line_num);
    int next;
    if (line_num + 1 <= lo->count)
    {
        next = tt_line_offsets_offset_at(lo, line_num + 1);
    }
    else
    {
        next = (int)content_len;
    }
    *out_length = next - *out_offset;
}

/* Grow symbols array if at capacity. */
static int grow_symbols(tt_symbol_t **syms, int *count, int *cap)
{
    if (*count < *cap)
        return 0;
    int new_cap = (*cap) * 2;
    if (new_cap < 16)
        new_cap = 16;
    tt_symbol_t *tmp = realloc(*syms, (size_t)new_cap * sizeof(tt_symbol_t));
    if (!tmp)
        return -1;
    *syms = tmp;
    *cap = new_cap;
    return 0;
}

/*
 * Build keywords for an EJS include path.
 * Split on /[\/.\-_]+/, lowercase, unique, filter >1 char.
 * Returns JSON array string. [caller-frees]
 */
static char *ejs_extract_keywords(const char *path)
{
    char *words[64];
    int word_count = 0;

    const char *start = path;
    while (*start)
    {
        /* Skip separator characters: / . - _ */
        while (*start && (*start == '/' || *start == '.' || *start == '-' || *start == '_'))
            start++;
        if (!*start)
            break;

        /* Find end of word */
        const char *end = start;
        while (*end && *end != '/' && *end != '.' && *end != '-' && *end != '_')
            end++;

        size_t wlen = (size_t)(end - start);
        if (wlen > 1 && word_count < 63)
        {
            char *tmp = tt_strndup(start, wlen);
            if (!tmp)
            {
                start = end;
                continue;
            }
            words[word_count] = tt_str_tolower(tmp);
            free(tmp);
            if (words[word_count])
                word_count++;
        }
        start = end;
    }

    /* Build JSON array with dedup */
    tt_strbuf_t sb;
    tt_strbuf_init(&sb);
    tt_strbuf_append_char(&sb, '[');
    int written = 0;
    for (int i = 0; i < word_count; i++)
    {
        bool dup = false;
        for (int j = 0; j < i; j++)
        {
            if (strcmp(words[i], words[j]) == 0)
            {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;
        if (strlen(words[i]) <= 1)
            continue;
        if (written > 0)
            tt_strbuf_append_char(&sb, ',');
        tt_strbuf_appendf(&sb, "\"%s\"", words[i]);
        written++;
    }
    tt_strbuf_append_char(&sb, ']');

    for (int i = 0; i < word_count; i++)
        free(words[i]);

    return tt_strbuf_detach(&sb);
}

int tt_parse_ejs(const char *project_root, const char **file_paths, int file_count,
                 tt_symbol_t **out, int *out_count)
{
    if (!out || !out_count)
        return -1;
    *out = NULL;
    *out_count = 0;

    if (!project_root || !file_paths || file_count <= 0)
        return 0;

    int sym_cap = 32;
    int sym_count = 0;
    tt_symbol_t *symbols = malloc((size_t)sym_cap * sizeof(tt_symbol_t));
    if (!symbols)
        return -1;

    for (int fi = 0; fi < file_count; fi++)
    {
        const char *rel_path = file_paths[fi];
        char *full_path = tt_path_join(project_root, rel_path);
        if (!full_path)
            continue;

        size_t content_len = 0;
        char *content = tt_read_file(full_path, &content_len);
        free(full_path);
        if (!content)
            continue;

        tt_line_offsets_t lo;
        tt_line_offsets_build(&lo, content, content_len);

        tt_hashmap_t *seen = tt_hashmap_new(32);

        /*
         * Scan for pattern: <%[-=]?\s*include\s*\(\s*['"]([^'"]+)['"]\s*(?:,\s*[^)]+)?\s*\)
         * Manual matching: find "<%", then optional - or =, whitespace, "include",
         * whitespace, "(", whitespace, quote, path, quote.
         */
        size_t pos = 0;
        while (pos + 1 < content_len)
        {
            /* Find "<%" */
            const char *tag = NULL;
            for (size_t i = pos; i + 1 < content_len; i++)
            {
                if (content[i] == '<' && content[i + 1] == '%')
                {
                    tag = content + i;
                    break;
                }
            }
            if (!tag)
                break;
            size_t tag_pos = (size_t)(tag - content);
            pos = tag_pos + 2;

            const char *p = tag + 2;
            const char *end = content + content_len;

            /* Optional '-' or '=' */
            if (p < end && (*p == '-' || *p == '='))
                p++;

            /* Skip whitespace */
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
                p++;

            /* Expect "include" */
            if (p + 7 > end || strncmp(p, "include", 7) != 0)
                continue;
            p += 7;

            /* After "include": expect \s*\( */
            while (p < end && (*p == ' ' || *p == '\t'))
                p++;
            if (p >= end || *p != '(')
                continue;
            p++;

            /* Skip whitespace */
            while (p < end && (*p == ' ' || *p == '\t'))
                p++;

            /* Expect quote */
            if (p >= end || (*p != '\'' && *p != '"'))
                continue;
            char quote = *p;
            p++;

            /* Read include path until matching quote */
            const char *path_start = p;
            while (p < end && *p != quote)
                p++;
            if (p >= end)
                continue;

            size_t path_len = (size_t)(p - path_start);
            char *include_path = tt_strndup(path_start, path_len);
            if (!include_path)
                continue;

            /* Line number from byte offset of "<%" */
            int line_num = tt_line_offsets_line_at(&lo, (int)tag_pos);

            /* Dedup: key = includePath */
            intptr_t occurrence = 0;
            if (tt_hashmap_has(seen, include_path))
            {
                occurrence = (intptr_t)tt_hashmap_get(seen, include_path);
            }
            occurrence++;
            tt_hashmap_set(seen, include_path, (void *)occurrence);

            /* Build qualified name */
            tt_strbuf_t qn_sb;
            tt_strbuf_init(&qn_sb);
            tt_strbuf_appendf(&qn_sb, "include.%s", include_path);
            if (occurrence > 1)
            {
                tt_strbuf_appendf(&qn_sb, "~%d", (int)occurrence);
            }
            char *qualified_name = tt_strbuf_detach(&qn_sb);

            /* Signature */
            tt_strbuf_t sig_sb;
            tt_strbuf_init(&sig_sb);
            tt_strbuf_appendf(&sig_sb, "include('%s')", include_path);
            char *signature = tt_strbuf_detach(&sig_sb);

            /* Line bytes */
            int line_byte_offset, line_byte_length;
            get_line_bytes(&lo, line_num, content_len,
                           &line_byte_offset, &line_byte_length);

            /* Content hash */
            char *content_hash = NULL;
            if (line_byte_length > 0 &&
                line_byte_offset + line_byte_length <= (int)content_len)
            {
                content_hash = tt_fast_hash_hex(content + line_byte_offset,
                                             (size_t)line_byte_length);
            }
            if (!content_hash)
                content_hash = tt_strdup("");

            char *id = tt_symbol_make_id(rel_path, qualified_name, "directive", 0);
            char *keywords = ejs_extract_keywords(include_path);

            if (grow_symbols(&symbols, &sym_count, &sym_cap) < 0)
            {
                free(id);
                free(qualified_name);
                free(signature);
                free(keywords);
                free(content_hash);
                free(include_path);
                continue;
            }

            tt_symbol_t *sym = &symbols[sym_count];
            sym->id = id;
            sym->file = tt_strdup(rel_path);
            sym->name = tt_strdup(include_path);
            sym->qualified_name = qualified_name;
            sym->kind = TT_KIND_DIRECTIVE;
            sym->language = tt_strdup("ejs");
            sym->signature = signature;
            sym->docstring = tt_strdup("");
            sym->summary = tt_strdup("");
            sym->decorators = tt_strdup("[]");
            sym->keywords = keywords;
            sym->parent_id = NULL;
            sym->line = line_num;
            sym->end_line = line_num;
            sym->byte_offset = line_byte_offset;
            sym->byte_length = line_byte_length;
            sym->content_hash = content_hash;
            sym_count++;

            free(include_path);
        }

        tt_hashmap_free(seen);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = symbols;
    *out_count = sym_count;
    return 0;
}
