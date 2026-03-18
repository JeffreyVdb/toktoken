/*
 * normalizer.c -- Normalize ctags language and kind names to canonical form.
 */

#include "normalizer.h"
#include "symbol_kind.h"
#include "platform.h"

#include <string.h>
#include <ctype.h>

/* LANGUAGE_MAP (from Normalizer.php, 5 entries). */
typedef struct
{
    const char *from;
    const char *to;
} lang_map_entry_t;

static const lang_map_entry_t LANGUAGE_MAP[] = {
    {"sh", "bash"},
    {"c++", "cpp"},
    {"c#", "csharp"},
    {"emacslisp", "elisp"},
    {"systemverilog", "verilog"},
};

#define LANGUAGE_MAP_SIZE (sizeof(LANGUAGE_MAP) / sizeof(LANGUAGE_MAP[0]))

const char *tt_normalize_language(const char *ctags_lang)
{
    if (!ctags_lang || !ctags_lang[0])
        return "unknown";

    /* Lowercase into thread-local buffer. */
    static _Thread_local char buf[128];
    size_t len = strlen(ctags_lang);
    if (len >= sizeof(buf))
        len = sizeof(buf) - 1;
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)tolower((unsigned char)ctags_lang[i]);
    buf[len] = '\0';

    /* Check LANGUAGE_MAP. */
    for (size_t i = 0; i < LANGUAGE_MAP_SIZE; i++)
    {
        if (strcmp(LANGUAGE_MAP[i].from, buf) == 0)
            return LANGUAGE_MAP[i].to;
    }

    return buf;
}

const char *tt_normalize_kind(const char *kind)
{
    if (!kind || !kind[0])
        return kind;

    /* Try matching against canonical kind names. */
    for (int i = 0; i < TT_KIND_COUNT; i++)
    {
        if (tt_strcasecmp(tt_kind_to_str((tt_symbol_kind_e)i), kind) == 0)
            return tt_kind_to_str((tt_symbol_kind_e)i);
    }

    return kind;
}
