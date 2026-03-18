/*
 * parser_blade.c -- Parser for Laravel Blade templates.
 *
 * Scans .blade.php files for Blade directives and produces symbols.
 * Pattern: @directive('argument')
 * 13 recognized directives. Manual pattern matching (no PCRE2 required).
 */

#include "parser_blade.h"
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

/* 13 recognized Blade directives (from BladeParser.php DIRECTIVE_PATTERN). */
static const char *BLADE_DIRECTIVES[] = {
    "extends", "section", "yield", "component", "livewire",
    "include", "includeIf", "includeWhen", "includeFirst",
    "slot", "push", "stack", "prepend"};
static const int BLADE_DIRECTIVE_COUNT = 13;

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

/* Normalize directive name: includeIf/includeWhen/includeFirst → include, prepend → push. */
static const char *normalize_directive(const char *directive)
{
    if (strcmp(directive, "includeIf") == 0 ||
        strcmp(directive, "includeWhen") == 0 ||
        strcmp(directive, "includeFirst") == 0)
    {
        return "include";
    }
    if (strcmp(directive, "prepend") == 0)
    {
        return "push";
    }
    return directive;
}

/*
 * Build keywords for a Blade directive.
 * words = [directive] + split(argument, ".") → lowercase, unique, filter >1 char.
 * Returns JSON array string. [caller-frees]
 */
static char *blade_extract_keywords(const char *directive, const char *argument)
{
    /* Collect words: directive + argument parts split on '.' */
    char *parts[64];
    int part_count = 0;

    /* Add directive (lowercased) */
    parts[part_count++] = tt_str_tolower(directive);

    /* Split argument by '.' */
    int split_count = 0;
    char **arg_parts = tt_str_split(argument, '.', &split_count);
    if (arg_parts)
    {
        for (int i = 0; i < split_count && part_count < 63; i++)
        {
            char *lower = tt_str_tolower(arg_parts[i]);
            if (lower && strlen(lower) > 1)
            {
                parts[part_count++] = lower;
            }
            else
            {
                free(lower);
            }
        }
        tt_str_split_free(arg_parts);
    }

    /* Build JSON array with dedup */
    tt_strbuf_t sb;
    tt_strbuf_init(&sb);
    tt_strbuf_append_char(&sb, '[');
    int written = 0;
    for (int i = 0; i < part_count; i++)
    {
        /* Check for duplicate */
        bool dup = false;
        for (int j = 0; j < i; j++)
        {
            if (strcmp(parts[i], parts[j]) == 0)
            {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;
        if (strlen(parts[i]) <= 1)
            continue;

        if (written > 0)
            tt_strbuf_append_char(&sb, ',');
        tt_strbuf_appendf(&sb, "\"%s\"", parts[i]);
        written++;
    }
    tt_strbuf_append_char(&sb, ']');

    for (int i = 0; i < part_count; i++)
        free(parts[i]);

    return tt_strbuf_detach(&sb);
}

int tt_parse_blade(const char *project_root, const char **file_paths, int file_count,
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

        /* Dedup map: key = "normalizedDirective:argument" → occurrence count */
        tt_hashmap_t *seen = tt_hashmap_new(32);

        /* Scan content for @directive('argument') patterns */
        size_t pos = 0;
        while (pos < content_len)
        {
            /* Find next '@' */
            const char *at = memchr(content + pos, '@', content_len - pos);
            if (!at)
                break;
            size_t at_pos = (size_t)(at - content);
            pos = at_pos + 1;

            /* Check each directive */
            for (int d = 0; d < BLADE_DIRECTIVE_COUNT; d++)
            {
                size_t dlen = strlen(BLADE_DIRECTIVES[d]);
                if (at_pos + 1 + dlen > content_len)
                    continue;
                if (strncmp(at + 1, BLADE_DIRECTIVES[d], dlen) != 0)
                    continue;

                /* After directive: expect \s*\(\s*['"] */
                const char *p = at + 1 + dlen;
                const char *end = content + content_len;

                /* Skip whitespace */
                while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
                    p++;
                if (p >= end || *p != '(')
                    continue;
                p++;

                /* Skip whitespace */
                while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
                    p++;
                if (p >= end || (*p != '\'' && *p != '"'))
                    continue;
                char quote = *p;
                p++;

                /* Read argument until matching quote */
                const char *arg_start = p;
                while (p < end && *p != quote)
                    p++;
                if (p >= end)
                    continue;

                size_t arg_len = (size_t)(p - arg_start);
                char *directive_name = tt_strndup(BLADE_DIRECTIVES[d], dlen);
                char *argument = tt_strndup(arg_start, arg_len);
                if (!directive_name || !argument)
                {
                    free(directive_name);
                    free(argument);
                    continue;
                }

                const char *normalized = normalize_directive(directive_name);

                /* Dedup: key = "normalizedDirective:argument" */
                tt_strbuf_t key_sb;
                tt_strbuf_init(&key_sb);
                tt_strbuf_appendf(&key_sb, "%s:%s", normalized, argument);
                char *dedup_key = tt_strbuf_detach(&key_sb);

                intptr_t occurrence = 0;
                if (tt_hashmap_has(seen, dedup_key))
                {
                    occurrence = (intptr_t)tt_hashmap_get(seen, dedup_key);
                }
                occurrence++;
                tt_hashmap_set(seen, dedup_key, (void *)occurrence);

                /* Build qualified name */
                tt_strbuf_t qn_sb;
                tt_strbuf_init(&qn_sb);
                tt_strbuf_appendf(&qn_sb, "%s.%s", normalized, argument);
                if (occurrence > 1)
                {
                    tt_strbuf_appendf(&qn_sb, "~%d", (int)occurrence);
                }
                char *qualified_name = tt_strbuf_detach(&qn_sb);

                /* Signature: uses ORIGINAL directive name */
                tt_strbuf_t sig_sb;
                tt_strbuf_init(&sig_sb);
                tt_strbuf_appendf(&sig_sb, "@%s('%s')", directive_name, argument);
                char *signature = tt_strbuf_detach(&sig_sb);

                /* Line number from byte position of '@' */
                int line_num = tt_line_offsets_line_at(&lo, (int)at_pos);

                /* Byte offset/length for the entire line */
                int line_byte_offset, line_byte_length;
                get_line_bytes(&lo, line_num, content_len,
                               &line_byte_offset, &line_byte_length);

                /* Content hash of the line */
                char *content_hash = NULL;
                if (line_byte_length > 0 &&
                    line_byte_offset + line_byte_length <= (int)content_len)
                {
                    content_hash = tt_fast_hash_hex(content + line_byte_offset,
                                                 (size_t)line_byte_length);
                }
                if (!content_hash)
                    content_hash = tt_strdup("");

                /* ID */
                char *id = tt_symbol_make_id(rel_path, qualified_name, "directive", 0);

                /* Keywords */
                char *keywords = blade_extract_keywords(directive_name, argument);

                /* Add symbol */
                if (grow_symbols(&symbols, &sym_count, &sym_cap) < 0)
                {
                    free(id);
                    free(qualified_name);
                    free(signature);
                    free(keywords);
                    free(content_hash);
                    free(directive_name);
                    free(argument);
                    free(dedup_key);
                    continue;
                }

                tt_symbol_t *sym = &symbols[sym_count];
                sym->id = id;
                sym->file = tt_strdup(rel_path);
                sym->name = tt_strdup(argument);
                sym->qualified_name = qualified_name;
                sym->kind = TT_KIND_DIRECTIVE;
                sym->language = tt_strdup("blade");
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

                free(directive_name);
                free(argument);
                free(dedup_key);
                break; /* matched this '@', move on */
            }
        }

        tt_hashmap_free(seen);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = symbols;
    *out_count = sym_count;
    return 0;
}
