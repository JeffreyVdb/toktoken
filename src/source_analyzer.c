/*
 * source_analyzer.c -- Source code analysis: end-line estimation, docstring
 *                      extraction, keyword extraction.
 */

#include "source_analyzer.h"
#include "str_util.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- estimateEndLine ---- */

int tt_estimate_end_line(const char **lines, int line_count,
                         int start_line, tt_symbol_kind_e kind)
{
    if (tt_kind_is_single_line(kind))
        return start_line;

    if (!lines || line_count <= 0 || start_line < 1)
        return start_line;

    int brace_count = 0;
    int paren_depth = 0;
    bool found_open = false;
    bool in_block_comment = false;

    for (int i = start_line - 1; i < line_count; i++)
    {
        const char *line = lines[i];
        if (!line)
            continue;
        int len = (int)strlen(line);
        char in_string = 0; /* 0 = not in string, '"' or '\'' = in that type */

        for (int c = 0; c < len; c++)
        {
            char ch = line[c];

            /* Block comment state */
            if (in_block_comment)
            {
                if (ch == '*' && (c + 1 < len) && line[c + 1] == '/')
                {
                    in_block_comment = false;
                    c++; /* skip '/' */
                }
                continue;
            }

            /* String state */
            if (in_string)
            {
                if (ch == '\\')
                {
                    c++; /* skip escaped char */
                }
                else if (ch == in_string)
                {
                    in_string = 0;
                }
                continue;
            }

            /* Normal state */
            if (ch == '"' || ch == '\'')
            {
                in_string = ch;
            }
            else if (ch == '/' && (c + 1 < len))
            {
                if (line[c + 1] == '/')
                {
                    break; /* rest of line is comment */
                }
                if (line[c + 1] == '*')
                {
                    in_block_comment = true;
                    c++; /* skip '*' */
                }
            }
            else if (ch == '#' && (c == 0 || line[c - 1] != '$'))
            {
                break; /* # line comment (not $#) */
            }
            else if (ch == '(')
            {
                paren_depth++;
            }
            else if (ch == ')')
            {
                if (paren_depth > 0)
                    paren_depth--;
            }
            else if (paren_depth == 0 && ch == '{')
            {
                brace_count++;
                found_open = true;
            }
            else if (paren_depth == 0 && ch == '}')
            {
                brace_count--;
                if (found_open && brace_count == 0)
                {
                    return i + 1; /* 1-indexed */
                }
            }
        }
        /* in_string resets per line (PHP behavior) */
    }

    /* Fallback */
    int fallback = start_line + 50;
    return fallback < line_count ? fallback : line_count;
}

/* ---- extractDocstring helpers ---- */

/* Check if line matches ^\s*(?:#\[|@) (decorator/attribute, NOT # comment). */
static bool is_decorator_line(const char *line)
{
    if (!line)
        return false;
    const char *p = line;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '#' && p[1] == '[')
        return true;
    if (*p == '@')
        return true;
    return false;
}

/* Check if line matches ^\s*(\/\/|#)\s (line comment with space after). */
static bool is_line_comment(const char *line)
{
    if (!line)
        return false;
    const char *p = line;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p[0] == '/' && p[1] == '/')
    {
        /* Must have space or tab or end after // */
        return (p[2] == ' ' || p[2] == '\t' || p[2] == '\0');
    }
    if (p[0] == '#')
    {
        /* Must have space or tab after #, and NOT #[ (that's decorator) */
        if (p[1] == '[')
            return false;
        return (p[1] == ' ' || p[1] == '\t' || p[1] == '\0');
    }
    return false;
}

/* Clean docblock: strip delimiters and leading asterisks. */
static char *clean_docblock(const char *raw)
{
    tt_strbuf_t sb;
    tt_strbuf_init(&sb);

    /* Split into lines */
    int count = 0;
    char **parts = tt_str_split(raw, '\n', &count);
    if (!parts)
    {
        tt_strbuf_free(&sb);
        return tt_strdup("");
    }

    for (int i = 0; i < count; i++)
    {
        char *line = parts[i];

        /* First line: strip leading whitespace + opening delimiter */
        if (i == 0)
        {
            char *p = line;
            while (*p == ' ' || *p == '\t')
                p++;
            if (p[0] == '/' && p[1] == '*')
            {
                p += 2;
                if (*p == '*')
                    p++; /* skip third star in doc-comment opener */
                while (*p == ' ')
                    p++;
            }
            line = p;
        }

        /* Last line: strip trailing delimiter */
        if (i == count - 1)
        {
            char *end = strstr(line, "*/");
            if (end)
            {
                /* Remove trailing close-comment delimiter */
                *end = '\0';
            }
        }

        /* Strip leading " * " pattern for middle lines */
        if (i > 0)
        {
            char *p = line;
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '*')
            {
                p++;
                if (*p == ' ')
                    p++;
            }
            line = p;
        }

        /* Trim the line */
        char *trimmed = tt_str_trim(line);

        if (sb.len > 0 || trimmed[0] != '\0')
            tt_strbuf_appendf(&sb, "%s%s", sb.len > 0 ? "\n" : "", trimmed);
    }

    tt_str_split_free(parts);

    char *result = tt_strbuf_detach(&sb);
    /* Trim the whole result */
    if (result)
        tt_str_trim(result);
    return result ? result : tt_strdup("");
}

/* Clean line comments: strip // or # prefix. */
static char *clean_line_comments(const char *raw)
{
    tt_strbuf_t sb;
    tt_strbuf_init(&sb);

    int count = 0;
    char **parts = tt_str_split(raw, '\n', &count);
    if (!parts)
    {
        tt_strbuf_free(&sb);
        return tt_strdup("");
    }

    for (int i = 0; i < count; i++)
    {
        char *line = parts[i];
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (p[0] == '/' && p[1] == '/')
        {
            p += 2;
            if (*p == ' ')
                p++;
        }
        else if (p[0] == '#')
        {
            p++;
            if (*p == ' ')
                p++;
        }
        char *trimmed = tt_str_trim(p);

        if (sb.len > 0 || trimmed[0] != '\0')
            tt_strbuf_appendf(&sb, "%s%s", sb.len > 0 ? "\n" : "", trimmed);
    }

    tt_str_split_free(parts);

    char *result = tt_strbuf_detach(&sb);
    if (result)
        tt_str_trim(result);
    return result ? result : tt_strdup("");
}

char *tt_extract_docstring(const char **lines, int line_count, int symbol_line)
{
    if (!lines || line_count <= 0 || symbol_line < 1)
        return tt_strdup("");

    int i = symbol_line - 2; /* 0-indexed, line before symbol */

    /* Skip decorators/attributes */
    while (i >= 0 && is_decorator_line(lines[i]))
        i--;

    if (i < 0)
        return tt_strdup("");

    /* Check for docblock ending (star-slash) */
    if (strstr(lines[i], "*/"))
    {
        int end = i;

        /* Walk up to find the opening */
        while (i >= 0)
        {
            if (strstr(lines[i], "/**") || strstr(lines[i], "/*"))
                break;
            i--;
        }
        if (i < 0)
            i = 0;

        /* Collect lines forward from start to end */
        tt_strbuf_t sb;
        tt_strbuf_init(&sb);
        for (int j = i; j <= end; j++)
        {
            if (j > i)
                tt_strbuf_append_char(&sb, '\n');
            tt_strbuf_append_str(&sb, lines[j]);
        }

        char *raw = tt_strbuf_detach(&sb);
        char *result = clean_docblock(raw);
        free(raw);
        return result;
    }

    /* Check for line comments (// or # style) */
    if (is_line_comment(lines[i]))
    {
        tt_strbuf_t sb;
        tt_strbuf_init(&sb);
        int end = i;

        while (i >= 0 && is_line_comment(lines[i]))
        {
            i--;
        }
        i++; /* back to first comment line */

        for (int j = i; j <= end; j++)
        {
            if (j > i)
                tt_strbuf_append_char(&sb, '\n');
            tt_strbuf_append_str(&sb, lines[j]);
        }

        char *raw = tt_strbuf_detach(&sb);
        char *result = clean_line_comments(raw);
        free(raw);
        return result;
    }

    return tt_strdup("");
}

/* ---- extractKeywords ---- */

char *tt_extract_keywords(const char *qualified_name)
{
    if (!qualified_name || !qualified_name[0])
        return tt_strdup("[]");

    /* Split on camelCase boundaries, underscores, dots, whitespace.
     * PHP regex: /(?=[A-Z])|[_\.\s]+/
     */
    tt_array_t words;
    tt_array_init(&words);

    const char *p = qualified_name;
    const char *word_start = p;

    while (*p)
    {
        if (*p == '_' || *p == '.' || *p == ' ' || *p == '\t')
        {
            /* Delimiter: emit current word, skip delimiter(s). */
            if (p > word_start)
            {
                tt_array_push(&words, tt_strndup(word_start, (size_t)(p - word_start)));
            }
            while (*p == '_' || *p == '.' || *p == ' ' || *p == '\t')
                p++;
            word_start = p;
            continue;
        }

        /* CamelCase boundary: uppercase letter that's not at word start. */
        if (p > word_start && isupper((unsigned char)*p))
        {
            tt_array_push(&words, tt_strndup(word_start, (size_t)(p - word_start)));
            word_start = p;
        }

        p++;
    }

    /* Emit last word */
    if (p > word_start)
    {
        tt_array_push(&words, tt_strndup(word_start, (size_t)(p - word_start)));
    }

    /* Lowercase all words, deduplicate, filter length <= 1 */
    tt_strbuf_t sb;
    tt_strbuf_init(&sb);
    tt_strbuf_append_char(&sb, '[');

    /* Simple dedup with linear scan (small arrays) */
    int json_count = 0;
    for (size_t i = 0; i < words.len; i++)
    {
        char *w = (char *)words.items[i];

        /* Lowercase in-place */
        for (char *c = w; *c; c++)
            *c = (char)tolower((unsigned char)*c);

        /* Filter single-char */
        if (strlen(w) <= 1)
            continue;

        /* Dedup check */
        bool dup = false;
        for (size_t j = 0; j < i; j++)
        {
            if (strcmp((char *)words.items[j], w) == 0)
            {
                dup = true;
                break;
            }
        }
        if (dup)
            continue;

        if (json_count > 0)
            tt_strbuf_append_char(&sb, ',');
        tt_strbuf_appendf(&sb, "\"%s\"", w);
        json_count++;
    }

    tt_strbuf_append_char(&sb, ']');

    /* Free word array */
    tt_array_free_items(&words);

    return tt_strbuf_detach(&sb);
}
