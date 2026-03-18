/*
 * parser_common.c -- Shared helpers for custom language parsers.
 */

#include "parser_common.h"
#include "fast_hash.h"
#include "str_util.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int tt_parser_grow(tt_symbol_t **syms, int *count, int *cap)
{
    if (*count < *cap) return 0;
    int new_cap = (*cap) * 2;
    if (new_cap < 16) new_cap = 16;
    tt_symbol_t *tmp = realloc(*syms, (size_t)new_cap * sizeof(tt_symbol_t));
    if (!tmp) return -1;
    *syms = tmp;
    *cap = new_cap;
    return 0;
}

char *tt_parser_extract_keywords(const char *name)
{
    char *words[64];
    int word_count = 0;

    tt_strbuf_t word;
    tt_strbuf_init(&word);

    for (const char *p = name; *p; p++) {
        bool is_sep = (*p == '_' || *p == '-' || *p == '.' ||
                       *p == ' ' || *p == '\t' || *p == '\n');
        bool is_upper = isupper((unsigned char)*p);

        if (is_sep) {
            if (word.len > 0 && word_count < 63) {
                char *w = tt_str_tolower(word.data);
                if (w && strlen(w) > 1) words[word_count++] = w;
                else free(w);
                tt_strbuf_reset(&word);
            }
            continue;
        }

        if (is_upper && word.len > 0) {
            if (word_count < 63) {
                char *w = tt_str_tolower(word.data);
                if (w && strlen(w) > 1) words[word_count++] = w;
                else free(w);
            }
            tt_strbuf_reset(&word);
        }

        tt_strbuf_append_char(&word, *p);
    }
    if (word.len > 0 && word_count < 63) {
        char *w = tt_str_tolower(word.data);
        if (w && strlen(w) > 1) words[word_count++] = w;
        else free(w);
    }
    tt_strbuf_free(&word);

    tt_strbuf_t sb;
    tt_strbuf_init(&sb);
    tt_strbuf_append_char(&sb, '[');
    int written = 0;
    for (int i = 0; i < word_count; i++) {
        bool dup = false;
        for (int j = 0; j < i; j++) {
            if (strcmp(words[i], words[j]) == 0) { dup = true; break; }
        }
        if (dup) continue;
        if (written > 0) tt_strbuf_append_char(&sb, ',');
        tt_strbuf_appendf(&sb, "\"%s\"", words[i]);
        written++;
    }
    tt_strbuf_append_char(&sb, ']');

    for (int i = 0; i < word_count; i++) free(words[i]);
    return tt_strbuf_detach(&sb);
}

int tt_parser_add_symbol(tt_symbol_t **syms, int *count, int *cap,
                          const char *rel_path, const char *name,
                          const char *signature, tt_symbol_kind_e kind,
                          const char *language, int line_num,
                          const char *content, size_t content_len,
                          const tt_line_offsets_t *lo)
{
    if (tt_parser_grow(syms, count, cap) < 0) return -1;

    int off = tt_line_offsets_offset_at(lo, line_num);
    int next = (line_num + 1 <= lo->count)
        ? tt_line_offsets_offset_at(lo, line_num + 1)
        : (int)content_len;
    int line_len = next - off;

    char *content_hash = NULL;
    if (line_len > 0 && off + line_len <= (int)content_len)
        content_hash = tt_fast_hash_hex(content + off, (size_t)line_len);
    if (!content_hash) content_hash = tt_strdup("");

    const char *kind_str = tt_kind_to_str(kind);
    char *id = tt_symbol_make_id(rel_path, name, kind_str, 0);
    char *keywords = tt_parser_extract_keywords(name);

    tt_symbol_t *sym = &(*syms)[*count];
    sym->id = id;
    sym->file = tt_strdup(rel_path);
    sym->name = tt_strdup(name);
    sym->qualified_name = tt_strdup(name);
    sym->kind = kind;
    sym->language = tt_strdup(language);
    sym->signature = tt_strdup(signature);
    sym->docstring = tt_strdup("");
    sym->summary = tt_strdup("");
    sym->decorators = tt_strdup("[]");
    sym->keywords = keywords;
    sym->parent_id = NULL;
    sym->line = line_num;
    sym->end_line = line_num;
    sym->byte_offset = off;
    sym->byte_length = line_len;
    sym->content_hash = content_hash;
    (*count)++;
    return 0;
}

size_t tt_parser_read_word(const char *p)
{
    size_t len = 0;
    while (p[len] && (isalnum((unsigned char)p[len]) || p[len] == '_'))
        len++;
    return len;
}
