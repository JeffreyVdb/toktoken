/*
 * symbol.c -- Symbol struct and ID generation.
 */

#include "symbol.h"
#include "str_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

char *tt_symbol_make_id(const char *file, const char *qualified_name,
                        const char *kind_str, int overload)
{
    if (!file || !qualified_name || !kind_str)
        return NULL;

    size_t flen = strlen(file);
    size_t qlen = strlen(qualified_name);
    size_t klen = strlen(kind_str);

    /* "file::qualified#kind" = flen + 2 + qlen + 1 + klen + NUL
     * with overload: + "~" + digits (max 11 for INT_MAX) */
    size_t need = flen + 2 + qlen + 1 + klen + 1;
    if (overload > 0)
        need += 12; /* ~2147483647 */

    char *buf = malloc(need);
    if (!buf) return NULL;

    if (overload > 0)
        snprintf(buf, need, "%s::%s#%s~%d", file, qualified_name, kind_str, overload);
    else
        memcpy(buf, file, flen),
        buf[flen] = ':', buf[flen + 1] = ':',
        memcpy(buf + flen + 2, qualified_name, qlen),
        buf[flen + 2 + qlen] = '#',
        memcpy(buf + flen + 2 + qlen + 1, kind_str, klen + 1);

    return buf;
}

void tt_symbol_free(tt_symbol_t *sym)
{
    if (!sym)
        return;
    free(sym->id);
    free(sym->file);
    free(sym->name);
    free(sym->qualified_name);
    free(sym->language);
    free(sym->signature);
    free(sym->docstring);
    free(sym->summary);
    free(sym->decorators);
    free(sym->keywords);
    free(sym->parent_id);
    free(sym->content_hash);
    memset(sym, 0, sizeof(*sym));
}

void tt_symbol_array_free(tt_symbol_t *syms, int count)
{
    if (!syms)
        return;
    for (int i = 0; i < count; i++)
        tt_symbol_free(&syms[i]);
    free(syms);
}
