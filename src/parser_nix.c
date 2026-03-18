/*
 * parser_nix.c -- Parser for Nix (.nix) files.
 *
 * Line-by-line parser: extracts attribute bindings and inherit declarations.
 * No PCRE2 required (simple patterns).
 */

#include "parser_nix.h"
#include "symbol.h"
#include "symbol_kind.h"
#include "line_offsets.h"
#include "fast_hash.h"
#include "str_util.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Nix keywords to skip when matching attribute bindings. */
static const char *NIX_KEYWORDS[] = {
    "in", "then", "else", "true", "false", "null", "or", "and"};
static const int NIX_KEYWORD_COUNT = 8;

static bool is_nix_keyword(const char *name)
{
    for (int i = 0; i < NIX_KEYWORD_COUNT; i++)
    {
        if (strcmp(name, NIX_KEYWORDS[i]) == 0)
            return true;
    }
    return false;
}

/* Check if c is a valid Nix name character: \w, apostrophe, or hyphen. */
static bool is_nix_name_char(char c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '\'' || c == '-';
}

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
 * Build keywords for Nix: split on camelCase + [_\-.\s]+, lowercase, unique, filter >1.
 * Returns JSON array string. [caller-frees]
 */
static char *nix_extract_keywords(const char *name)
{
    char *words[64];
    int word_count = 0;

    tt_strbuf_t word;
    tt_strbuf_init(&word);

    for (const char *p = name; *p; p++)
    {
        bool is_sep = (*p == '_' || *p == '-' || *p == '.' ||
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

        /* Split on camelCase boundary (uppercase letter). */
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
    /* Last word */
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
static int add_nix_symbol(tt_symbol_t **syms, int *count, int *cap,
                          const char *rel_path, const char *name,
                          const char *signature, tt_symbol_kind_e kind,
                          int line_num, const char *content, size_t content_len,
                          const tt_line_offsets_t *lo)
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
    char *keywords = nix_extract_keywords(name);

    tt_symbol_t *sym = &(*syms)[*count];
    sym->id = id;
    sym->file = tt_strdup(rel_path);
    sym->name = tt_strdup(name);
    sym->qualified_name = tt_strdup(name);
    sym->kind = kind;
    sym->language = tt_strdup("nix");
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

int tt_parse_nix(const char *project_root, const char **file_paths, int file_count,
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

        /* Split into lines for line-by-line parsing. */
        int split_count = 0;
        char **raw_lines = tt_str_split(content, '\n', &split_count);

        for (int i = 0; i < split_count; i++)
        {
            int line_num = i + 1;
            char *line = raw_lines[i];
            char *trimmed = tt_str_trim(tt_strdup(line));
            if (!trimmed)
                continue;

            /*
             * Pattern 1: Attribute binding
             * /^(\s{0,4})(\w[\w'-]*)\s*=\s*(.*)/
             * Indent 0-4 spaces, name with word/apostrophe/hyphen chars.
             */
            {
                const char *p = line;
                int indent = 0;
                while (*p == ' ' && indent < 5)
                {
                    indent++;
                    p++;
                }

                if (indent <= 4 && (isalpha((unsigned char)*p) || *p == '_'))
                {
                    /* Read name: \w[\w'-]* */
                    const char *name_start = p;
                    p++;
                    while (*p && is_nix_name_char(*p))
                        p++;
                    size_t name_len = (size_t)(p - name_start);

                    /* Skip whitespace */
                    while (*p == ' ' || *p == '\t')
                        p++;

                    /* Expect '=' */
                    if (*p == '=' && *(p + 1) != '=')
                    {
                        p++; /* skip '=' */
                        while (*p == ' ' || *p == '\t')
                            p++;
                        const char *value_start = p;

                        char *name = tt_strndup(name_start, name_len);
                        if (name && !is_nix_keyword(name))
                        {
                            /* Determine kind from value */
                            tt_symbol_kind_e kind = TT_KIND_VARIABLE;
                            tt_strbuf_t sig_sb;
                            tt_strbuf_init(&sig_sb);

                            /* Check if value is a function pattern:
                             * /^(\w+\s*:|{[^}]*})\s*:/ */
                            bool is_func = false;
                            if (isalpha((unsigned char)*value_start) || *value_start == '_')
                            {
                                const char *vp = value_start;
                                while (*vp && (isalnum((unsigned char)*vp) || *vp == '_'))
                                    vp++;
                                while (*vp == ' ' || *vp == '\t')
                                    vp++;
                                if (*vp == ':')
                                    is_func = true;
                            }
                            if (!is_func && *value_start == '{')
                            {
                                const char *close = strchr(value_start, '}');
                                if (close)
                                {
                                    const char *vp = close + 1;
                                    while (*vp == ' ' || *vp == '\t')
                                        vp++;
                                    if (*vp == ':')
                                        is_func = true;
                                }
                            }

                            if (is_func)
                            {
                                kind = TT_KIND_FUNCTION;
                                /* sig = "name = " + substr(value, 0, 60) */
                                tt_strbuf_appendf(&sig_sb, "%s = ", name);
                                size_t vlen = strlen(value_start);
                                if (vlen > 60)
                                    vlen = 60;
                                tt_strbuf_append(&sig_sb, value_start, vlen);
                            }
                            else if (*value_start == '{' || *value_start == '[')
                            {
                                /* sig = "name = " + substr(value, 0, 40) */
                                tt_strbuf_appendf(&sig_sb, "%s = ", name);
                                size_t vlen = strlen(value_start);
                                if (vlen > 40)
                                    vlen = 40;
                                tt_strbuf_append(&sig_sb, value_start, vlen);
                            }
                            else
                            {
                                tt_strbuf_appendf(&sig_sb, "%s = ...", name);
                            }

                            char *sig = tt_strbuf_detach(&sig_sb);
                            add_nix_symbol(&symbols, &sym_count, &sym_cap,
                                           rel_path, name, sig, kind,
                                           line_num, content, content_len, &lo);
                            free(sig);
                        }
                        free(name);
                        free(trimmed);
                        continue;
                    }
                }
            }

            /*
             * Pattern 2: inherit
             * /^\s*inherit\s+(?:\(([^)]+)\)\s+)?([\w\s]+);/
             */
            if (tt_str_starts_with(trimmed, "inherit ") ||
                tt_str_starts_with(trimmed, "inherit\t"))
            {
                const char *p = trimmed + 7; /* skip "inherit" */
                while (*p == ' ' || *p == '\t')
                    p++;

                char *source = NULL;

                /* Optional (source) */
                if (*p == '(')
                {
                    p++;
                    const char *src_start = p;
                    while (*p && *p != ')')
                        p++;
                    if (*p == ')')
                    {
                        source = tt_strndup(src_start, (size_t)(p - src_start));
                        p++;
                        while (*p == ' ' || *p == '\t')
                            p++;
                    }
                }

                /* Read names until ';' */
                const char *names_start = p;
                const char *semi = strchr(p, ';');
                if (semi)
                {
                    char *names_str = tt_strndup(names_start, (size_t)(semi - names_start));
                    if (names_str)
                    {
                        /* Split names by whitespace */
                        int name_count = 0;
                        char **names = tt_str_split_words(names_str, &name_count);
                        if (names)
                        {
                            for (int n = 0; n < name_count; n++)
                            {
                                if (!names[n] || strlen(names[n]) == 0)
                                    continue;

                                tt_strbuf_t sig_sb;
                                tt_strbuf_init(&sig_sb);
                                if (source && strlen(source) > 0)
                                {
                                    tt_strbuf_appendf(&sig_sb, "inherit (%s) %s",
                                                      source, names[n]);
                                }
                                else
                                {
                                    tt_strbuf_appendf(&sig_sb, "inherit %s", names[n]);
                                }
                                char *sig = tt_strbuf_detach(&sig_sb);

                                add_nix_symbol(&symbols, &sym_count, &sym_cap,
                                               rel_path, names[n], sig,
                                               TT_KIND_VARIABLE, line_num,
                                               content, content_len, &lo);
                                free(sig);
                            }
                            tt_str_split_free(names);
                        }
                        free(names_str);
                    }
                }
                free(source);
            }

            free(trimmed);
        }

        tt_str_split_free(raw_lines);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = symbols;
    *out_count = sym_count;
    return 0;
}
