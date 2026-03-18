/*
 * parser_vue.c -- Parser for Vue Single File Components (.vue).
 *
 * Extracts component structure from <script setup> or <script> blocks:
 * defineProps/defineEmits/defineExpose, composables (useXxx), named functions,
 * computed, ref/reactive, watch/watchEffect, lifecycle hooks, provide/inject,
 * and Options API methods.
 */

#include "parser_vue.h"
#include "symbol.h"
#include "symbol_kind.h"
#include "line_offsets.h"
#include "fast_hash.h"
#include "str_util.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Options API skip list: keywords that look like method definitions. */
static const char *VUE_SKIP_NAMES[] = {
    "if", "for", "while", "switch", "catch", "function", "return"};
static const int VUE_SKIP_COUNT = 7;

static bool is_vue_skip_name(const char *name)
{
    for (int i = 0; i < VUE_SKIP_COUNT; i++)
    {
        if (strcmp(name, VUE_SKIP_NAMES[i]) == 0)
            return true;
    }
    return false;
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

/* Read a \w+ word at position p. Returns length, 0 if no match. */
static size_t read_word(const char *p)
{
    size_t len = 0;
    while (p[len] && (isalnum((unsigned char)p[len]) || p[len] == '_'))
        len++;
    return len;
}

/*
 * Build keywords for Vue: same as SourceAnalyzer (split on camelCase + [_.\s]+).
 * Returns JSON array string. [caller-frees]
 */
static char *vue_extract_keywords(const char *name)
{
    char *words[64];
    int word_count = 0;

    tt_strbuf_t word;
    tt_strbuf_init(&word);

    for (const char *p = name; *p; p++)
    {
        bool is_sep = (*p == '_' || *p == '.' ||
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

/* Add a Vue symbol to the array. */
static int add_vue_symbol(tt_symbol_t **syms, int *count, int *cap,
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
    char *keywords = vue_extract_keywords(name);

    tt_symbol_t *sym = &(*syms)[*count];
    sym->id = id;
    sym->file = tt_strdup(rel_path);
    sym->name = tt_strdup(name);
    sym->qualified_name = tt_strdup(name);
    sym->kind = kind;
    sym->language = tt_strdup("vue");
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

/*
 * Extract <script setup> or <script> block content.
 * Prefers <script setup>. Returns allocated copy, NULL if not found.
 * script_tag_line receives the 1-indexed line of the <script> tag.
 */
static char *extract_script_block(const char *content, size_t content_len,
                                  int *script_start_line)
{
    *script_start_line = 1;

    const char *found_tag = NULL;

    /* First pass: look for <script ... setup ...> */
    const char *pos = content;
    while ((pos = strstr(pos, "<script")) != NULL)
    {
        const char *tag_end = strchr(pos, '>');
        if (!tag_end)
            break;

        /* Check if "setup" appears between <script and > */
        bool has_setup = false;
        for (const char *p = pos + 7; p + 4 < tag_end; p++)
        {
            if (strncmp(p, "setup", 5) == 0)
            {
                has_setup = true;
                break;
            }
        }

        if (has_setup)
        {
            const char *close = strstr(tag_end + 1, "</script>");
            if (close)
            {
                found_tag = pos;
                break;
            }
        }
        pos = tag_end + 1;
    }

    /* Fallback: any <script> */
    if (!found_tag)
    {
        pos = strstr(content, "<script");
        if (pos)
        {
            const char *tag_end = strchr(pos, '>');
            if (tag_end)
            {
                const char *close = strstr(tag_end + 1, "</script>");
                if (close)
                {
                    found_tag = pos;
                }
            }
        }
    }

    if (!found_tag)
        return NULL;

    /* Find the line number of the script tag. */
    int line = 1;
    for (const char *p = content; p < found_tag; p++)
    {
        if (*p == '\n')
            line++;
    }
    /* Content starts on line AFTER the tag. */
    *script_start_line = line + 1;

    /* Extract content between > and </script> */
    const char *tag_end = strchr(found_tag, '>');
    if (!tag_end)
        return NULL;
    const char *close = strstr(tag_end + 1, "</script>");
    if (!close)
        return NULL;

    size_t block_len = (size_t)(close - (tag_end + 1));
    return tt_strndup(tag_end + 1, block_len);
}

/* Check if a name has already been added for this file at this line. */
static bool is_duplicate(const tt_symbol_t *syms, int count,
                         const char *file, const char *name, int line)
{
    for (int i = 0; i < count; i++)
    {
        if (syms[i].line == line &&
            strcmp(syms[i].file, file) == 0 &&
            strcmp(syms[i].name, name) == 0)
        {
            return true;
        }
    }
    return false;
}

int tt_parse_vue(const char *project_root, const char **file_paths, int file_count,
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

        int script_start_line = 1;
        char *script_content = extract_script_block(content, content_len,
                                                    &script_start_line);
        if (!script_content)
        {
            free(content);
            tt_line_offsets_free(&lo);
            continue;
        }

        /* Split script content into lines. */
        int sc_line_count = 0;
        char **sc_lines = tt_str_split(script_content, '\n', &sc_line_count);

        for (int si = 0; si < sc_line_count; si++)
        {
            int file_line = script_start_line + si;
            char *line = sc_lines[si];

            /* (a) defineProps: defineProps followed by < or ( */
            {
                const char *dp = strstr(line, "defineProps");
                if (dp)
                {
                    const char *after = dp + 11;
                    while (*after == ' ' || *after == '\t')
                        after++;
                    if (*after == '<' || *after == '(')
                    {
                        add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                       rel_path, "props", "defineProps",
                                       TT_KIND_PROPERTY, file_line,
                                       content, content_len, &lo);
                    }
                }
            }

            /* (b) defineEmits: defineEmits followed by < or ( */
            {
                const char *de = strstr(line, "defineEmits");
                if (de)
                {
                    const char *after = de + 11;
                    while (*after == ' ' || *after == '\t')
                        after++;
                    if (*after == '<' || *after == '(')
                    {
                        add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                       rel_path, "emits", "defineEmits",
                                       TT_KIND_PROPERTY, file_line,
                                       content, content_len, &lo);
                    }
                }
            }

            /* (b2) defineExpose: defineExpose({ ... }) */
            {
                const char *dx = strstr(line, "defineExpose");
                if (dx)
                {
                    const char *after = dx + 12;
                    while (*after == ' ' || *after == '\t')
                        after++;
                    if (*after == '(')
                    {
                        add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                       rel_path, "expose", "defineExpose",
                                       TT_KIND_PROPERTY, file_line,
                                       content, content_len, &lo);
                    }
                }
            }

            /* (b3) watch / watchEffect */
            {
                const char *w = strstr(line, "watchEffect");
                if (w)
                {
                    const char *after = w + 11;
                    while (*after == ' ' || *after == '\t')
                        after++;
                    if (*after == '(')
                    {
                        add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                       rel_path, "watchEffect", "watchEffect(...)",
                                       TT_KIND_FUNCTION, file_line,
                                       content, content_len, &lo);
                    }
                }
                else
                {
                    w = strstr(line, "watch");
                    if (w)
                    {
                        /* Ensure it's standalone 'watch(' not 'watchEffect' etc. */
                        const char *after = w + 5;
                        if (!isalnum((unsigned char)*after) && *after != '_')
                        {
                            while (*after == ' ' || *after == '\t')
                                after++;
                            if (*after == '(')
                            {
                                add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                               rel_path, "watch", "watch(...)",
                                               TT_KIND_FUNCTION, file_line,
                                               content, content_len, &lo);
                            }
                        }
                    }
                }
            }

            /* (b4) Lifecycle hooks */
            {
                static const char *hooks[] = {
                    "onMounted", "onUnmounted", "onBeforeMount",
                    "onBeforeUnmount", "onUpdated", "onBeforeUpdate",
                    "onActivated", "onDeactivated", "onErrorCaptured"};
                for (int h = 0; h < 9; h++)
                {
                    const char *hk = strstr(line, hooks[h]);
                    if (hk)
                    {
                        size_t hlen = strlen(hooks[h]);
                        const char *after = hk + hlen;
                        if (!isalnum((unsigned char)*after) && *after != '_')
                        {
                            while (*after == ' ' || *after == '\t')
                                after++;
                            if (*after == '(')
                            {
                                add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                               rel_path, hooks[h], hooks[h],
                                               TT_KIND_FUNCTION, file_line,
                                               content, content_len, &lo);
                                break;
                            }
                        }
                    }
                }
            }

            /* (b5) provide / inject */
            {
                const char *pv = strstr(line, "provide");
                if (pv && !isalnum((unsigned char)pv[7]) && pv[7] != '_')
                {
                    const char *after = pv + 7;
                    while (*after == ' ' || *after == '\t')
                        after++;
                    if (*after == '(')
                    {
                        add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                       rel_path, "provide", "provide(...)",
                                       TT_KIND_PROPERTY, file_line,
                                       content, content_len, &lo);
                    }
                }
            }
            {
                const char *ij = strstr(line, "inject");
                if (ij && !isalnum((unsigned char)ij[6]) && ij[6] != '_')
                {
                    const char *after = ij + 6;
                    while (*after == ' ' || *after == '\t')
                        after++;
                    if (*after == '(')
                    {
                        add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                       rel_path, "inject", "inject(...)",
                                       TT_KIND_PROPERTY, file_line,
                                       content, content_len, &lo);
                    }
                }
            }

            /* (c) Composables: (const|let) binding = useXxx(...) */
            {
                const char *kw = strstr(line, "const ");
                if (!kw)
                    kw = strstr(line, "const\t");
                if (!kw)
                    kw = strstr(line, "let ");
                if (!kw)
                    kw = strstr(line, "let\t");
                if (kw)
                {
                    const char *p = kw + (kw[0] == 'c' ? 5 : 3); /* skip const/let */
                    while (*p == ' ' || *p == '\t')
                        p++;

                    /* Read binding: { ... } or \w+ */
                    const char *bind_start = p;
                    if (*p == '{')
                    {
                        const char *close = strchr(p, '}');
                        if (close)
                            p = close + 1;
                    }
                    else
                    {
                        while (*p && (isalnum((unsigned char)*p) || *p == '_'))
                            p++;
                    }
                    size_t bind_len = (size_t)(p - bind_start);

                    /* Skip whitespace, expect '=' */
                    while (*p == ' ' || *p == '\t')
                        p++;
                    if (*p == '=')
                    {
                        p++;
                        while (*p == ' ' || *p == '\t')
                            p++;

                        /* Check for "use" prefix */
                        if (strncmp(p, "use", 3) == 0 && (isalnum((unsigned char)p[3]) || p[3] == '_'))
                        {
                            const char *comp_start = p;
                            size_t comp_len = read_word(p);
                            if (comp_len > 3)
                            { /* more than just "use" */
                                p += comp_len;
                                while (*p == ' ' || *p == '\t')
                                    p++;
                                if (*p == '(')
                                {
                                    char *composable = tt_strndup(comp_start, comp_len);
                                    char *binding = tt_strndup(bind_start, bind_len);

                                    tt_strbuf_t sig_sb;
                                    tt_strbuf_init(&sig_sb);
                                    tt_strbuf_appendf(&sig_sb, "%s = %s(...)",
                                                      binding ? binding : "?",
                                                      composable ? composable : "?");
                                    char *sig = tt_strbuf_detach(&sig_sb);

                                    if (composable)
                                    {
                                        add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                                       rel_path, composable, sig,
                                                       TT_KIND_FUNCTION, file_line,
                                                       content, content_len, &lo);
                                    }
                                    free(composable);
                                    free(binding);
                                    free(sig);
                                    continue;
                                }
                            }
                        }

                        /* (e) computed: (const|let) xxx = computed(...) */
                        if (strncmp(p, "computed", 8) == 0)
                        {
                            const char *cp = p + 8;
                            while (*cp == ' ' || *cp == '\t')
                                cp++;
                            if (*cp == '(')
                            {
                                /* The binding must be a word */
                                char *var_name = tt_strndup(bind_start, bind_len);
                                if (var_name)
                                {
                                    tt_strbuf_t sig_sb;
                                    tt_strbuf_init(&sig_sb);
                                    tt_strbuf_appendf(&sig_sb, "computed(%s)", var_name);
                                    char *sig = tt_strbuf_detach(&sig_sb);
                                    add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                                   rel_path, var_name, sig,
                                                   TT_KIND_PROPERTY, file_line,
                                                   content, content_len, &lo);
                                    free(sig);
                                    free(var_name);
                                    continue;
                                }
                            }
                        }

                        /* (f) ref, reactive, shallowRef, shallowReactive */
                        const char *reactive_types[] = {
                            "shallowReactive", "shallowRef", "reactive", "ref"};
                        for (int rt = 0; rt < 4; rt++)
                        {
                            size_t rt_len = strlen(reactive_types[rt]);
                            if (strncmp(p, reactive_types[rt], rt_len) == 0)
                            {
                                const char *after_rt = p + rt_len;
                                while (*after_rt == ' ' || *after_rt == '\t')
                                    after_rt++;
                                if (*after_rt == '<' || *after_rt == '(')
                                {
                                    char *var_name = tt_strndup(bind_start, bind_len);
                                    if (var_name)
                                    {
                                        tt_strbuf_t sig_sb;
                                        tt_strbuf_init(&sig_sb);
                                        tt_strbuf_appendf(&sig_sb, "%s(%s)",
                                                          reactive_types[rt], var_name);
                                        char *sig = tt_strbuf_detach(&sig_sb);
                                        add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                                       rel_path, var_name, sig,
                                                       TT_KIND_VARIABLE, file_line,
                                                       content, content_len, &lo);
                                        free(sig);
                                        free(var_name);
                                        goto next_line;
                                    }
                                }
                                break;
                            }
                        }

                        /*
                         * (d) Arrow function: const xxx = (...) => or const xxx = async (...) =>
                         * The binding must be a plain word (not destructured).
                         */
                        {
                            const char *ap = p;
                            /* Optional "async " */
                            if (strncmp(ap, "async", 5) == 0 &&
                                (ap[5] == ' ' || ap[5] == '\t'))
                            {
                                ap += 5;
                                while (*ap == ' ' || *ap == '\t')
                                    ap++;
                            }
                            if (*ap == '(')
                            {
                                /* Find matching ')' */
                                const char *close = strchr(ap, ')');
                                if (close)
                                {
                                    const char *after_close = close + 1;
                                    while (*after_close == ' ' || *after_close == '\t')
                                        after_close++;
                                    if (strncmp(after_close, "=>", 2) == 0)
                                    {
                                        char *name = tt_strndup(bind_start, bind_len);
                                        if (name && !tt_str_starts_with(name, "use"))
                                        {
                                            /* Signature: the "const name = ..." match */
                                            size_t match_len = (size_t)(after_close + 2 - kw);
                                            char *sig = tt_strndup(kw, match_len);
                                            add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                                           rel_path, name, sig ? sig : "",
                                                           TT_KIND_FUNCTION, file_line,
                                                           content, content_len, &lo);
                                            free(sig);
                                        }
                                        free(name);
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            /*
             * (d) Named function: function xxx(...)
             * /function\s+(\w+)\s*\(/
             * Skip if name starts with "use" (already captured as composable).
             */
            {
                const char *fn = strstr(line, "function ");
                if (!fn)
                    fn = strstr(line, "function\t");
                if (fn)
                {
                    const char *p = fn + 8; /* skip "function" */
                    while (*p == ' ' || *p == '\t')
                        p++;
                    size_t name_len = read_word(p);
                    if (name_len > 0)
                    {
                        const char *after = p + name_len;
                        while (*after == ' ' || *after == '\t')
                            after++;
                        if (*after == '(')
                        {
                            char *name = tt_strndup(p, name_len);
                            if (name && !tt_str_starts_with(name, "use"))
                            {
                                /* Signature: the full match */
                                size_t match_len = (size_t)(after + 1 - fn);
                                char *sig_full = tt_strndup(fn, match_len);
                                add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                               rel_path, name,
                                               sig_full ? sig_full : "",
                                               TT_KIND_FUNCTION, file_line,
                                               content, content_len, &lo);
                                free(sig_full);
                            }
                            free(name);
                        }
                    }
                }
            }

            /*
             * (g) Options API methods: indented name(...) {
             */
            {
                const char *p = line;
                /* Must be indented */
                if (*p == ' ' || *p == '\t')
                {
                    while (*p == ' ' || *p == '\t')
                        p++;
                    size_t name_len = read_word(p);
                    if (name_len > 0)
                    {
                        const char *after = p + name_len;
                        while (*after == ' ' || *after == '\t')
                            after++;
                        if (*after == '(')
                        {
                            /* Find ')' */
                            const char *close = strchr(after, ')');
                            if (close)
                            {
                                const char *after_close = close + 1;
                                while (*after_close == ' ' || *after_close == '\t')
                                    after_close++;
                                if (*after_close == '{')
                                {
                                    char *name = tt_strndup(p, name_len);
                                    if (name && !is_vue_skip_name(name))
                                    {
                                        /* Dedup check */
                                        if (!is_duplicate(symbols, sym_count,
                                                          rel_path, name, file_line))
                                        {
                                            tt_strbuf_t sig_sb;
                                            tt_strbuf_init(&sig_sb);
                                            tt_strbuf_appendf(&sig_sb, "%s()", name);
                                            char *sig = tt_strbuf_detach(&sig_sb);
                                            add_vue_symbol(&symbols, &sym_count, &sym_cap,
                                                           rel_path, name, sig,
                                                           TT_KIND_METHOD, file_line,
                                                           content, content_len, &lo);
                                            free(sig);
                                        }
                                    }
                                    free(name);
                                }
                            }
                        }
                    }
                }
            }

        next_line:;
        }

        tt_str_split_free(sc_lines);
        free(script_content);
        free(content);
        tt_line_offsets_free(&lo);
    }

    *out = symbols;
    *out_count = sym_count;
    return 0;
}
