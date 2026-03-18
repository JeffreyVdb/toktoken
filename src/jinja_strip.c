/*
 * jinja_strip.c -- Strip Jinja2 template syntax from SQL files.
 *
 * Single-pass scan: replace {{ }}, {% %}, {# #} with spaces.
 * Newlines inside Jinja blocks are preserved for line number stability.
 */

#include "jinja_strip.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

char *tt_jinja_strip(const char *content, size_t len, size_t *out_len)
{
    if (!content || len == 0)
    {
        if (out_len)
            *out_len = 0;
        return NULL;
    }

    /* Quick scan: does content contain any Jinja delimiters? */
    bool has_jinja = false;
    for (size_t i = 0; i + 1 < len; i++)
    {
        if (content[i] == '{' &&
            (content[i + 1] == '{' || content[i + 1] == '%' || content[i + 1] == '#'))
        {
            has_jinja = true;
            break;
        }
    }

    if (!has_jinja)
    {
        if (out_len)
            *out_len = len;
        return NULL; /* no modification needed */
    }

    /* Copy content, then blank out Jinja blocks in-place */
    char *buf = malloc(len + 1);
    if (!buf)
    {
        if (out_len)
            *out_len = 0;
        return NULL;
    }
    memcpy(buf, content, len);
    buf[len] = '\0';

    size_t i = 0;
    while (i + 1 < len)
    {
        if (buf[i] != '{')
        {
            i++;
            continue;
        }

        char opener = buf[i + 1];
        char closer1, closer2;

        if (opener == '{')
        {
            closer1 = '}';
            closer2 = '}';
        }
        else if (opener == '%')
        {
            closer1 = '%';
            closer2 = '}';
        }
        else if (opener == '#')
        {
            closer1 = '#';
            closer2 = '}';
        }
        else
        {
            i++;
            continue;
        }

        /* Find matching closer */
        size_t j = i + 2;
        bool found = false;
        while (j + 1 < len)
        {
            if (buf[j] == closer1 && buf[j + 1] == closer2)
            {
                found = true;
                break;
            }
            j++;
        }

        if (!found)
        {
            /* Unclosed delimiter: leave as-is */
            i += 2;
            continue;
        }

        /* Replace [i..j+1] with spaces, preserving newlines */
        size_t end = j + 2;
        for (size_t k = i; k < end && k < len; k++)
        {
            if (buf[k] != '\n')
                buf[k] = ' ';
        }
        i = end;
    }

    if (out_len)
        *out_len = len;
    return buf;
}
