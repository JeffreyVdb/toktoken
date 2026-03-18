/*
 * text_search.c -- Full-text search across indexed files.
 *
 * Dual strategy: ripgrep (fast) + manual fallback.
 */

#include "text_search.h"
#include "platform.h"
#include "str_util.h"
#include "error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef TT_PLATFORM_UNIX
#include <unistd.h>
#endif
#ifdef TT_PLATFORM_WINDOWS
#include <process.h>
#endif

/* Default options. */
static const tt_text_search_opts_t DEFAULT_OPTS = {
    .case_sensitive = false,
    .max_results = 100,
    .context_lines = 0};

/* Cached ripgrep path (NULL = not checked, "" = not available). */
static char *rg_path_cache = NULL;
static bool rg_path_checked = false;

/* Check if ripgrep is available. Caches the result. */
static const char *find_ripgrep(void)
{
    if (!rg_path_checked)
    {
        rg_path_cache = tt_which("rg");
        rg_path_checked = true;
    }
    return rg_path_cache;
}

/* Free a single result's strings. */
static void result_free_fields(tt_text_result_t *r)
{
    free(r->file);
    free(r->text);
    for (int i = 0; i < r->before_count; i++)
        free(r->before[i]);
    free(r->before);
    for (int i = 0; i < r->after_count; i++)
        free(r->after[i]);
    free(r->after);
}

/* Truncate text to max 200 chars, append "..." if needed.
 * Returns a new string. [caller-frees] */
static char *truncate_text(const char *text)
{
    if (!text)
        return tt_strdup("");

    /* Trim leading/trailing whitespace */
    while (*text && (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n'))
        text++;

    size_t len = strlen(text);

    /* Trim trailing whitespace */
    while (len > 0 && (text[len - 1] == ' ' || text[len - 1] == '\t' ||
                       text[len - 1] == '\r' || text[len - 1] == '\n'))
        len--;

    /* Check if UTF-8 char count > 200 */
    size_t char_count = 0;
    size_t byte_pos = 0;
    size_t trunc_byte = 0;

    while (byte_pos < len)
    {
        if (char_count == 200)
        {
            trunc_byte = byte_pos;
        }
        char_count++;

        unsigned char c = (unsigned char)text[byte_pos];
        if (c < 0x80)
            byte_pos++;
        else if (c < 0xE0)
            byte_pos += 2;
        else if (c < 0xF0)
            byte_pos += 3;
        else
            byte_pos += 4;

        if (byte_pos > len)
            byte_pos = len;
    }

    if (char_count > 200)
    {
        /* Truncate at UTF-8 boundary */
        char *result = malloc(trunc_byte + 4);
        if (!result)
            return tt_strdup("");
        memcpy(result, text, trunc_byte);
        memcpy(result + trunc_byte, "...", 3);
        result[trunc_byte + 3] = '\0';
        return result;
    }

    return tt_strndup(text, len);
}

/* Grow results array. Returns 0 on success. */
static int grow_results(tt_text_results_t *out, int *cap)
{
    if (out->count < *cap)
        return 0;
    int new_cap = (*cap) * 2;
    if (new_cap < 32)
        new_cap = 32;
    tt_text_result_t *tmp = realloc(out->results,
                                    (size_t)new_cap * sizeof(tt_text_result_t));
    if (!tmp)
        return -1;
    out->results = tmp;
    *cap = new_cap;
    return 0;
}

/* ---- ReDoS protection ---- */

/* Max allowed regex length. */
#define REGEX_MAX_LEN 200

const char *tt_regex_validate(const char *pattern)
{
    if (!pattern || !pattern[0])
        return "Empty regex pattern";

    if (strlen(pattern) > REGEX_MAX_LEN)
        return "Regex too long (max 200 characters)";

    /* Detect nested quantifiers: a quantifier after a closing group that
     * itself contains a quantifier. Patterns like (a+)+, (a*)+, (a+)*,
     * (?:a+){2,} cause catastrophic backtracking. */
    int depth = 0;
    /* Stack of "group has quantifier" flags, max 32 nesting levels. */
    bool stack[32] = {false};

    for (const char *p = pattern; *p; p++)
    {
        if (*p == '\\' && p[1])
        {
            p++; /* skip escaped character */
            continue;
        }
        if (*p == '[')
        {
            /* Skip character class contents */
            p++;
            if (*p == '^') p++;
            if (*p == ']') p++;
            while (*p && *p != ']')
            {
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            continue;
        }
        if (*p == '(')
        {
            if (depth < 31)
            {
                depth++;
                stack[depth] = false;
            }
            continue;
        }
        if (*p == ')')
        {
            bool inner_had_q = (depth > 0) ? stack[depth] : false;
            if (depth > 0) depth--;
            /* Check if a quantifier follows the closing paren */
            const char *next = p + 1;
            while (*next == ' ' || *next == '\t') next++;
            if (inner_had_q && (*next == '+' || *next == '*' || *next == '{'))
                return "Regex rejected: nested quantifiers can cause catastrophic backtracking";
            continue;
        }
        if (*p == '+' || *p == '*')
        {
            if (depth > 0 && depth < 32)
                stack[depth] = true;
        }
    }

    return NULL;
}

/* Escape a string for regex (like PHP preg_quote). [caller-frees] */
static char *regex_escape(const char *s)
{
    tt_strbuf_t sb;
    tt_strbuf_init(&sb);

    for (const char *p = s; *p; p++)
    {
        switch (*p)
        {
        case '\\':
        case '.':
        case '+':
        case '*':
        case '?':
        case '[':
        case ']':
        case '(':
        case ')':
        case '{':
        case '}':
        case '^':
        case '$':
        case '|':
            tt_strbuf_append_char(&sb, '\\');
            /* fallthrough */
        default:
            tt_strbuf_append_char(&sb, *p);
            break;
        }
    }

    return tt_strbuf_detach(&sb);
}

/* ---- Ripgrep search ---- */

static int search_ripgrep(const char *project_root, const char **file_paths, int file_count,
                          const char **queries, int query_count,
                          const tt_text_search_opts_t *opts,
                          tt_text_results_t *out, int *cap)
{
    /* Build regex pattern: escaped queries joined with | */
    tt_strbuf_t pattern;
    tt_strbuf_init(&pattern);

    for (int i = 0; i < query_count; i++)
    {
        if (i > 0)
            tt_strbuf_append_char(&pattern, '|');
        if (opts->is_regex)
        {
            tt_strbuf_append_str(&pattern, queries[i]);
        }
        else
        {
            char *escaped = regex_escape(queries[i]);
            if (escaped)
            {
                tt_strbuf_append_str(&pattern, escaped);
                free(escaped);
            }
        }
    }

    /* Write file list to temp file */
    char list_path[256];
#ifdef TT_PLATFORM_WINDOWS
    snprintf(list_path, sizeof(list_path), "tt_rg_%d.txt", (int)_getpid());
#else
    snprintf(list_path, sizeof(list_path), "/tmp/tt_rg_%d.txt", (int)getpid());
#endif

    tt_strbuf_t list_content;
    tt_strbuf_init(&list_content);
    for (int i = 0; i < file_count; i++)
    {
        tt_strbuf_append_str(&list_content, file_paths[i]);
        tt_strbuf_append_char(&list_content, '\n');
    }
    tt_write_file(list_path, list_content.data, list_content.len);
    tt_strbuf_free(&list_content);

    /* Build rg command */
    char max_count_str[32];
    snprintf(max_count_str, sizeof(max_count_str), "%d", opts->max_results);

    const char *argv[16];
    int argc = 0;
    argv[argc++] = "rg";
    argv[argc++] = "--json";
    argv[argc++] = "--max-count";
    argv[argc++] = max_count_str;

    if (!opts->case_sensitive)
    {
        argv[argc++] = "--ignore-case";
    }

    argv[argc++] = pattern.data;
    argv[argc++] = "--files-from";
    argv[argc++] = list_path;
    argv[argc] = NULL;

    /* Run rg with 30s timeout */
    tt_proc_result_t result = tt_proc_run(argv, NULL, 30000);

    tt_remove_file(list_path);

    /* rg exit codes: 0 = matches found, 1 = no matches, 2+ = error.
     * exit_code < 0 = process launch failure.
     * Any error → return -1 to trigger manual fallback in caller. */
    if (result.exit_code < 0 || result.exit_code >= 2 || !result.stdout_buf)
    {
        tt_proc_result_free(&result);
        tt_strbuf_free(&pattern);
        return -1;
    }

    /* Parse ripgrep JSON output */
    const char *line = result.stdout_buf;
    while (*line && out->count < opts->max_results)
    {
        const char *eol = strchr(line, '\n');
        if (!eol)
            eol = line + strlen(line);

        size_t line_len = (size_t)(eol - line);
        if (line_len > 0)
        {
            /* Quick check for "match" type */
            const char *type_pos = strstr(line, "\"type\":\"match\"");
            if (type_pos && type_pos < eol)
            {
                /* We need the path.text specifically. In ripgrep JSON:
                 * {"type":"match","data":{"path":{"text":"..."},"lines":{"text":"..."},"line_number":N,...}}
                 * Find "path" then "text" */
                const char *path_key = strstr(line, "\"path\":");
                char *file = NULL;
                int line_num = 0;
                char *match_text = NULL;

                if (path_key && path_key < eol)
                {
                    const char *pt = strstr(path_key, "\"text\":\"");
                    if (pt && pt < eol)
                    {
                        pt += 8;
                        const char *ptend = strchr(pt, '"');
                        if (ptend && ptend < eol)
                        {
                            file = tt_strndup(pt, (size_t)(ptend - pt));
                        }
                    }
                }

                /* Extract line_number */
                const char *ln_key = strstr(line, "\"line_number\":");
                if (ln_key && ln_key < eol)
                {
                    ln_key += 14;
                    line_num = atoi(ln_key);
                }

                /* Extract lines.text */
                const char *lines_key = strstr(line, "\"lines\":");
                if (lines_key && lines_key < eol)
                {
                    const char *lt = strstr(lines_key, "\"text\":\"");
                    if (lt && lt < eol)
                    {
                        lt += 8;
                        /* Read JSON string value (handle escapes) */
                        tt_strbuf_t txt;
                        tt_strbuf_init(&txt);
                        while (*lt && *lt != '"' && lt < eol)
                        {
                            if (*lt == '\\' && *(lt + 1))
                            {
                                lt++;
                                switch (*lt)
                                {
                                case 'n':
                                    tt_strbuf_append_char(&txt, '\n');
                                    break;
                                case 't':
                                    tt_strbuf_append_char(&txt, '\t');
                                    break;
                                case 'r':
                                    tt_strbuf_append_char(&txt, '\r');
                                    break;
                                case '\\':
                                    tt_strbuf_append_char(&txt, '\\');
                                    break;
                                case '"':
                                    tt_strbuf_append_char(&txt, '"');
                                    break;
                                case '/':
                                    tt_strbuf_append_char(&txt, '/');
                                    break;
                                default:
                                    tt_strbuf_append_char(&txt, *lt);
                                    break;
                                }
                            }
                            else
                            {
                                tt_strbuf_append_char(&txt, *lt);
                            }
                            lt++;
                        }
                        match_text = tt_strbuf_detach(&txt);
                    }
                }

                if (file && line_num > 0 && match_text)
                {
                    if (grow_results(out, cap) == 0)
                    {
                        tt_text_result_t *r = &out->results[out->count];
                        r->file = file;
                        r->line = line_num;
                        r->text = truncate_text(match_text);
                        r->before = NULL;
                        r->before_count = 0;
                        r->after = NULL;
                        r->after_count = 0;
                        out->count++;
                        file = NULL;
                    }
                }

                free(file);
                free(match_text);
            }
        }

        if (!*eol)
            break;
        line = eol + 1;
    }

    tt_proc_result_free(&result);
    tt_strbuf_free(&pattern);
    return 0;
}

/* ---- Manual (PHP-style) fallback search ---- */

/* Check if line contains any of the queries. */
static bool line_matches(const char *line_text,
                         const char **queries, int query_count,
                         const char **queries_lower,
                         bool case_sensitive)
{
    for (int q = 0; q < query_count; q++)
    {
        if (case_sensitive)
        {
            if (strstr(line_text, queries[q]))
                return true;
        }
        else
        {
            if (tt_strcasestr(line_text, queries_lower[q]))
                return true;
        }
    }
    return false;
}

static int search_manual(const char *project_root, const char **file_paths, int file_count,
                         const char **queries, int query_count,
                         const tt_text_search_opts_t *opts,
                         tt_text_results_t *out, int *cap)
{
    /* Pre-lowercase queries for case-insensitive matching */
    char **queries_lower = NULL;
    if (!opts->case_sensitive)
    {
        queries_lower = malloc(sizeof(char *) * (size_t)query_count);
        if (!queries_lower)
            return -1;
        for (int i = 0; i < query_count; i++)
        {
            queries_lower[i] = tt_str_tolower(queries[i]);
        }
    }

    for (int fi = 0; fi < file_count && out->count < opts->max_results; fi++)
    {
        char *full_path = tt_path_join(project_root, file_paths[fi]);
        if (!full_path)
            continue;

        size_t content_len = 0;
        char *content = tt_read_file(full_path, &content_len);
        free(full_path);
        if (!content)
            continue;

        /* Split into lines */
        int total_lines = 0;
        char **lines = tt_str_split(content, '\n', &total_lines);
        if (!lines)
        {
            free(content);
            continue;
        }

        for (int li = 0; li < total_lines && out->count < opts->max_results; li++)
        {
            const char *line_text = lines[li];

            if (!line_matches(line_text, queries, query_count,
                              (const char **)queries_lower, opts->case_sensitive))
                continue;

            if (grow_results(out, cap) < 0)
                break;

            tt_text_result_t *r = &out->results[out->count];
            r->file = tt_strdup(file_paths[fi]);
            r->line = li + 1;
            r->text = truncate_text(line_text);
            r->before = NULL;
            r->before_count = 0;
            r->after = NULL;
            r->after_count = 0;

            /* Context lines */
            if (opts->context_lines > 0)
            {
                int ctx = opts->context_lines;

                /* Before context */
                int before_start = li - ctx;
                if (before_start < 0)
                    before_start = 0;
                int before_cnt = li - before_start;
                if (before_cnt > 0)
                {
                    r->before = malloc(sizeof(char *) * (size_t)before_cnt);
                    if (r->before)
                    {
                        r->before_count = before_cnt;
                        for (int b = 0; b < before_cnt; b++)
                        {
                            r->before[b] = tt_strdup(lines[before_start + b]);
                        }
                    }
                }

                /* After context */
                int after_end = li + 1 + ctx;
                if (after_end > total_lines)
                    after_end = total_lines;
                int after_cnt = after_end - (li + 1);
                if (after_cnt > 0)
                {
                    r->after = malloc(sizeof(char *) * (size_t)after_cnt);
                    if (r->after)
                    {
                        r->after_count = after_cnt;
                        for (int a = 0; a < after_cnt; a++)
                        {
                            r->after[a] = tt_strdup(lines[li + 1 + a]);
                        }
                    }
                }
            }

            out->count++;
        }

        tt_str_split_free(lines);
        free(content);
    }

    if (queries_lower)
    {
        for (int i = 0; i < query_count; i++)
            free(queries_lower[i]);
        free(queries_lower);
    }

    return 0;
}

/* ---- Public API ---- */

int tt_text_search(const char *project_root, const char **file_paths, int file_count,
                   const char **queries, int query_count,
                   const tt_text_search_opts_t *opts,
                   tt_text_results_t *out)
{
    if (!out)
        return -1;
    memset(out, 0, sizeof(*out));

    if (!project_root || !file_paths || file_count <= 0 ||
        !queries || query_count <= 0)
    {
        return 0;
    }

    if (!opts)
        opts = &DEFAULT_OPTS;

    /* ReDoS validation for regex mode */
    if (opts->is_regex)
    {
        for (int i = 0; i < query_count; i++)
        {
            const char *err = tt_regex_validate(queries[i]);
            if (err)
            {
                tt_error_set("%s", err);
                return -1;
            }
        }
    }

    int cap = 32;
    out->results = malloc(sizeof(tt_text_result_t) * (size_t)cap);
    if (!out->results)
        return -1;

    const char *rg = find_ripgrep();
    int rc;

    if (rg)
    {
        rc = search_ripgrep(project_root, file_paths, file_count,
                            queries, query_count, opts, out, &cap);
        if (rc < 0)
        {
            /* Ripgrep failed, fallback to manual */
            for (int i = 0; i < out->count; i++)
                result_free_fields(&out->results[i]);
            out->count = 0;
            rc = search_manual(project_root, file_paths, file_count,
                               queries, query_count, opts, out, &cap);
        }
    }
    else
    {
        rc = search_manual(project_root, file_paths, file_count,
                           queries, query_count, opts, out, &cap);
    }

    return rc;
}

void tt_text_results_free(tt_text_results_t *r)
{
    if (!r)
        return;
    for (int i = 0; i < r->count; i++)
    {
        result_free_fields(&r->results[i]);
    }
    free(r->results);
    r->results = NULL;
    r->count = 0;
}
