/*
 * parser_gleam.c -- Parser for Gleam (.gleam) files.
 *
 * Line-by-line parser: extracts functions, types, constants, and imports.
 * No PCRE2 required (simple patterns).
 */

#include "parser_gleam.h"
#include "symbol.h"
#include "symbol_kind.h"
#include "line_offsets.h"
#include "fast_hash.h"
#include "str_util.h"
#include "platform.h"

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
 * Build keywords for Gleam: split on camelCase + [_/.\s]+, lowercase, unique, filter >1.
 * Returns JSON array string. [caller-frees]
 */
static char *gleam_extract_keywords(const char *name)
{
    char *words[64];
    int word_count = 0;

    tt_strbuf_t word;
    tt_strbuf_init(&word);

    for (const char *p = name; *p; p++)
    {
        bool is_sep = (*p == '_' || *p == '/' || *p == '.' ||
                       *p == ' ' || *p == '\t' || *p == '\n');
        bool is_upper = isupper((unsigned char)*p);

        if (is_sep)
        {
            if (word.len > 0 && word_count < 63)
            {
                char *w = tt_str_tolower(word.data);
                if (w && strlen(w) > 1)
                {
                    words[word_count++] = w;
                }
                else
                {
                    free(w);
                }
                tt_strbuf_reset(&word);
            }
            continue;
        }

        if (is_upper && word.len > 0)
        {
            if (word_count < 63)
            {
                char *w = tt_str_tolower(word.data);
                if (w && strlen(w) > 1)
                {
                    words[word_count++] = w;
                }
                else
                {
                    free(w);
                }
            }
            tt_strbuf_reset(&word);
        }

        tt_strbuf_append_char(&word, *p);
    }
    if (word.len > 0 && word_count < 63)
    {
        char *w = tt_str_tolower(word.data);
        if (w && strlen(w) > 1)
        {
            words[word_count++] = w;
        }
        else
        {
            free(w);
        }
    }
    tt_strbuf_free(&word);

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

/* Add a symbol to the array. */
static int add_gleam_symbol(tt_symbol_t **syms, int *count, int *cap,
                            const char *rel_path, const char *name,
                            const char *signature, tt_symbol_kind_e kind,
                            int line_num, const char *content,
                            size_t content_len, const tt_line_offsets_t *lo)
{
    if (grow_symbols(syms, count, cap) < 0)
        return -1;

    int line_byte_offset, line_byte_length;
    get_line_bytes(lo, line_num, content_len, &line_byte_offset, &line_byte_length);

    char *content_hash = NULL;
    if (line_byte_length > 0 &&
        line_byte_offset + line_byte_length <= (int)content_len)
    {
        content_hash = tt_fast_hash_hex(content + line_byte_offset,
                                     (size_t)line_byte_length);
    }
    if (!content_hash)
        content_hash = tt_strdup("");

    const char *kind_str = tt_kind_to_str(kind);
    char *id = tt_symbol_make_id(rel_path, name, kind_str, 0);
    char *keywords = gleam_extract_keywords(name);

    tt_symbol_t *sym = &(*syms)[*count];
    sym->id = id;
    sym->file = tt_strdup(rel_path);
    sym->name = tt_strdup(name);
    sym->qualified_name = tt_strdup(name);
    sym->kind = kind;
    sym->language = tt_strdup("gleam");
    sym->signature = tt_strdup(signature);
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
    (*count)++;
    return 0;
}

/* Read a \w+ word starting at p. Returns length, 0 if no match. */
static size_t read_word(const char *p)
{
    size_t len = 0;
    while (p[len] && (isalnum((unsigned char)p[len]) || p[len] == '_'))
        len++;
    return len;
}

int tt_parse_gleam(const char *project_root, const char **file_paths, int file_count,
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

        int split_count = 0;
        char **raw_lines = tt_str_split(content, '\n', &split_count);

        for (int i = 0; i < split_count; i++)
        {
            int line_num = i + 1;
            char *line = raw_lines[i];

            /* Trim the line for pattern matching. */
            char *trimmed_buf = tt_strdup(line);
            if (!trimmed_buf)
                continue;
            char *trimmed = tt_str_trim(trimmed_buf);

            /*
             * Pattern 1: Function
             * /^(pub\s+)?fn\s+(\w+)\s*\(([^)]*)\)/
             */
            {
                const char *p = trimmed;
                if (strncmp(p, "pub", 3) == 0 && (p[3] == ' ' || p[3] == '\t'))
                {
                    p += 3;
                    while (*p == ' ' || *p == '\t')
                        p++;
                }
                if (strncmp(p, "fn", 2) == 0 && (p[2] == ' ' || p[2] == '\t'))
                {
                    p += 2;
                    while (*p == ' ' || *p == '\t')
                        p++;
                    size_t name_len = read_word(p);
                    if (name_len > 0)
                    {
                        const char *after_name = p + name_len;
                        /* Skip whitespace, expect '(' */
                        const char *q = after_name;
                        while (*q == ' ' || *q == '\t')
                            q++;
                        if (*q == '(')
                        {
                            /* Find closing ')' for signature */
                            const char *close = strchr(q, ')');
                            if (close)
                            {
                                char *name = tt_strndup(p, name_len);
                                /* Signature = entire match from start to ')' */
                                size_t sig_len = (size_t)(close + 1 - trimmed);
                                char *sig = tt_strndup(trimmed, sig_len);
                                if (name && sig)
                                {
                                    add_gleam_symbol(&symbols, &sym_count, &sym_cap,
                                                     rel_path, name, sig,
                                                     TT_KIND_FUNCTION, line_num,
                                                     content, content_len, &lo);
                                }
                                free(name);
                                free(sig);
                                free(trimmed_buf);
                                continue;
                            }
                        }
                    }
                }
            }

            /*
             * Pattern 2: Type
             * /^(pub\s+)?type\s+(\w+)/
             */
            {
                const char *p = trimmed;
                if (strncmp(p, "pub", 3) == 0 && (p[3] == ' ' || p[3] == '\t'))
                {
                    p += 3;
                    while (*p == ' ' || *p == '\t')
                        p++;
                }
                if (strncmp(p, "type", 4) == 0 && (p[4] == ' ' || p[4] == '\t'))
                {
                    p += 4;
                    while (*p == ' ' || *p == '\t')
                        p++;
                    size_t name_len = read_word(p);
                    if (name_len > 0)
                    {
                        char *name = tt_strndup(p, name_len);
                        /* Signature = from start to end of name */
                        size_t sig_len = (size_t)(p + name_len - trimmed);
                        char *sig = tt_strndup(trimmed, sig_len);
                        if (name && sig)
                        {
                            add_gleam_symbol(&symbols, &sym_count, &sym_cap,
                                             rel_path, name, sig,
                                             TT_KIND_TYPE, line_num,
                                             content, content_len, &lo);
                        }
                        free(name);
                        free(sig);
                        free(trimmed_buf);
                        continue;
                    }
                }
            }

            /*
             * Pattern 3: Constant
             * /^(pub\s+)?const\s+(\w+)/
             */
            {
                const char *p = trimmed;
                if (strncmp(p, "pub", 3) == 0 && (p[3] == ' ' || p[3] == '\t'))
                {
                    p += 3;
                    while (*p == ' ' || *p == '\t')
                        p++;
                }
                if (strncmp(p, "const", 5) == 0 && (p[5] == ' ' || p[5] == '\t'))
                {
                    p += 5;
                    while (*p == ' ' || *p == '\t')
                        p++;
                    size_t name_len = read_word(p);
                    if (name_len > 0)
                    {
                        char *name = tt_strndup(p, name_len);
                        size_t sig_len = (size_t)(p + name_len - trimmed);
                        char *sig = tt_strndup(trimmed, sig_len);
                        if (name && sig)
                        {
                            add_gleam_symbol(&symbols, &sym_count, &sym_cap,
                                             rel_path, name, sig,
                                             TT_KIND_CONSTANT, line_num,
                                             content, content_len, &lo);
                        }
                        free(name);
                        free(sig);
                        free(trimmed_buf);
                        continue;
                    }
                }
            }

            /*
             * Pattern 4: Import
             * /^import\s+([\w\/]+)/
             */
            if (strncmp(trimmed, "import", 6) == 0 &&
                (trimmed[6] == ' ' || trimmed[6] == '\t'))
            {
                const char *p = trimmed + 6;
                while (*p == ' ' || *p == '\t')
                    p++;

                /* Read module path: [\w/]+ */
                const char *path_start = p;
                while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '/'))
                    p++;
                size_t path_len = (size_t)(p - path_start);
                if (path_len > 0)
                {
                    char *name = tt_strndup(path_start, path_len);
                    tt_strbuf_t sig_sb;
                    tt_strbuf_init(&sig_sb);
                    tt_strbuf_appendf(&sig_sb, "import %s", name);
                    char *sig = tt_strbuf_detach(&sig_sb);
                    if (name && sig)
                    {
                        add_gleam_symbol(&symbols, &sym_count, &sym_cap,
                                         rel_path, name, sig,
                                         TT_KIND_NAMESPACE, line_num,
                                         content, content_len, &lo);
                    }
                    free(name);
                    free(sig);
                }
            }

            free(trimmed_buf);
        }

        tt_str_split_free(raw_lines);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = symbols;
    *out_count = sym_count;
    return 0;
}
