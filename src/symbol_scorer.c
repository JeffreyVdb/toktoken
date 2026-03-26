/*
 * symbol_scorer.c -- Symbol relevance scoring for search results.
 *
 * Zero-allocation scoring: uses tt_strcasestr() for all case-insensitive
 * matching instead of allocating lowercased copies.
 */

#include "symbol_scorer.h"
#include "platform.h"
#include "str_util.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Check if character is a word separator: whitespace, underscore, or dot. */
static bool is_word_sep(unsigned char c)
{
    return isspace(c) || c == '_' || c == '.';
}

/* Iterate words in a string without allocating. Calls cb(word_start, word_len)
 * for each word. Returns when cb returns non-zero or string is exhausted. */
static int for_each_word(const char *s,
                         int (*cb)(const char *word, size_t len, void *ctx),
                         void *ctx)
{
    if (!s) return 0;
    const char *p = s;
    while (*p)
    {
        while (*p && is_word_sep((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !is_word_sep((unsigned char)*p)) p++;
        int rc = cb(start, (size_t)(p - start), ctx);
        if (rc) return rc;
    }
    return 0;
}

/* Context for word-match scoring: check if any query word is a
 * case-insensitive substring of the current name word. */
typedef struct {
    const char **query_words;
    int query_word_count;
    int score;
} word_match_ctx_t;

static int word_match_cb(const char *word, size_t len, void *ctx)
{
    word_match_ctx_t *wm = (word_match_ctx_t *)ctx;
    /* We need a null-terminated copy for strcasestr.
     * Use a stack buffer to avoid heap allocation. */
    char buf[256];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, word, len);
    buf[len] = '\0';

    for (int w = 0; w < wm->query_word_count; w++)
    {
        if (tt_strcasestr(buf, wm->query_words[w]))
        {
            wm->score += TT_WEIGHT_NAME_WORD;
            break;
        }
    }
    return 0;
}

/* Parse a JSON array of strings ["a","b","c"] and check if any keyword
 * contains query_lower (case-insensitive). Returns the number of matches,
 * each worth TT_WEIGHT_KEYWORD points. */
static int score_keywords(const char *keywords_json, const char *query_lower)
{
    if (!keywords_json || keywords_json[0] != '[')
        return 0;

    int matches = 0;
    const char *p = keywords_json + 1;

    while (*p)
    {
        while (*p && (*p == ' ' || *p == ',' || *p == '\t'))
            p++;
        if (*p == ']' || !*p)
            break;

        if (*p != '"')
            break;
        p++;

        const char *start = p;
        while (*p && *p != '"')
        {
            if (*p == '\\' && *(p + 1))
                p += 2;
            else
                p++;
        }

        size_t kw_len = (size_t)(p - start);
        if (*p == '"')
            p++;

        /* Case-insensitive match using stack buffer */
        char buf[256];
        size_t copy_len = kw_len < sizeof(buf) - 1 ? kw_len : sizeof(buf) - 1;
        memcpy(buf, start, copy_len);
        buf[copy_len] = '\0';

        if (tt_strcasestr(buf, query_lower))
            matches++;
    }

    return matches;
}

/* Score a single text field: full query match OR per-word match.
 * full_weight: score for full query match.
 * word_weight: score per matching query word. */
static int score_field(const char *text, const char *query_lower,
                       const char **query_words, int query_word_count,
                       int full_weight, int word_weight)
{
    if (!text || !text[0])
        return 0;

    if (tt_strcasestr(text, query_lower))
        return full_weight;

    int score = 0;
    for (int w = 0; w < query_word_count; w++)
    {
        if (tt_strcasestr(text, query_words[w]))
            score += word_weight;
    }
    return score;
}

int tt_score_symbol(const char *name, const char *qualified_name,
                    const char *signature, const char *summary,
                    const char *keywords_json, const char *docstring,
                    const char *query_lower, const char **query_words,
                    int query_word_count)
{
    if (!query_lower || !query_lower[0])
        return 0;

    int score = 0;

    /* === Phase 1: Name matching (cascading exclusive) === */

    /* 1. Exact name match (+20) */
    if ((name && strcasecmp(name, query_lower) == 0) ||
        (qualified_name && strcasecmp(qualified_name, query_lower) == 0))
    {
        score += TT_WEIGHT_EXACT_NAME;
    }

    /* 2. Name substring (+10) -- only if no exact match */
    if (score < TT_WEIGHT_EXACT_NAME)
    {
        if (name && tt_strcasestr(name, query_lower))
        {
            score += TT_WEIGHT_NAME_SUBSTRING;
        }
    }

    /* 3. Name word overlap (+5 per query word) -- only if no substring match */
    if (score < TT_WEIGHT_NAME_SUBSTRING)
    {
        word_match_ctx_t wm = { query_words, query_word_count, 0 };
        for_each_word(name ? name : "", word_match_cb, &wm);
        score += wm.score;
    }

    /* === Phase 2: Signature === */
    score += score_field(signature, query_lower, query_words, query_word_count,
                         TT_WEIGHT_SIGNATURE_FULL, TT_WEIGHT_SIGNATURE_WORD);

    /* === Phase 3: Summary === */
    score += score_field(summary, query_lower, query_words, query_word_count,
                         TT_WEIGHT_SUMMARY_FULL, TT_WEIGHT_SUMMARY_WORD);

    /* === Phase 4: Keywords (cumulative, +3 per match) === */
    score += score_keywords(keywords_json, query_lower) * TT_WEIGHT_KEYWORD;

    /* === Phase 5: Docstring word match (cumulative, +1 per word) === */
    if (docstring && docstring[0])
    {
        for (int w = 0; w < query_word_count; w++)
        {
            if (tt_strcasestr(docstring, query_words[w]))
                score += TT_WEIGHT_DOCSTRING_WORD;
        }
    }

    /* === Phase 6: Multi-term match bonus (F9) ===
     * Reward symbols that match 2+ distinct query words across any field.
     * +5 per additional matching word beyond the first. */
    if (query_word_count >= 2)
    {
        int words_matched = 0;
        for (int w = 0; w < query_word_count; w++)
        {
            if ((name && tt_strcasestr(name, query_words[w])) ||
                (qualified_name && tt_strcasestr(qualified_name, query_words[w])) ||
                (summary && tt_strcasestr(summary, query_words[w])) ||
                (signature && tt_strcasestr(signature, query_words[w])) ||
                (keywords_json && tt_strcasestr(keywords_json, query_words[w])))
            {
                words_matched++;
            }
        }
        if (words_matched >= 2)
            score += (words_matched - 1) * TT_WEIGHT_NAME_WORD;
    }

    return score;
}

int tt_score_symbol_debug(const char *name, const char *qualified_name,
                           const char *signature, const char *summary,
                           const char *keywords_json, const char *docstring,
                           const char *query_lower, const char **query_words,
                           int query_word_count,
                           tt_score_breakdown_t *breakdown)
{
    if (!breakdown) {
        return tt_score_symbol(name, qualified_name, signature, summary,
                               keywords_json, docstring, query_lower,
                               query_words, query_word_count);
    }

    memset(breakdown, 0, sizeof(*breakdown));

    if (!query_lower || !query_lower[0])
        return 0;

    /* Phase 1: Name */
    if ((name && strcasecmp(name, query_lower) == 0) ||
        (qualified_name && strcasecmp(qualified_name, query_lower) == 0)) {
        breakdown->name_score += TT_WEIGHT_EXACT_NAME;
    }
    if (breakdown->name_score < TT_WEIGHT_EXACT_NAME &&
        name && tt_strcasestr(name, query_lower)) {
        breakdown->name_score += TT_WEIGHT_NAME_SUBSTRING;
    }
    if (breakdown->name_score < TT_WEIGHT_NAME_SUBSTRING) {
        word_match_ctx_t wm = { query_words, query_word_count, 0 };
        for_each_word(name ? name : "", word_match_cb, &wm);
        breakdown->name_score += wm.score;
    }

    /* Phase 2: Signature */
    breakdown->signature_score = score_field(
        signature, query_lower, query_words, query_word_count,
        TT_WEIGHT_SIGNATURE_FULL, TT_WEIGHT_SIGNATURE_WORD);

    /* Phase 3: Summary */
    breakdown->summary_score = score_field(
        summary, query_lower, query_words, query_word_count,
        TT_WEIGHT_SUMMARY_FULL, TT_WEIGHT_SUMMARY_WORD);

    /* Phase 4: Keywords */
    breakdown->keyword_score =
        score_keywords(keywords_json, query_lower) * TT_WEIGHT_KEYWORD;

    /* Phase 5: Docstring */
    if (docstring && docstring[0]) {
        for (int w = 0; w < query_word_count; w++) {
            if (tt_strcasestr(docstring, query_words[w]))
                breakdown->docstring_score += TT_WEIGHT_DOCSTRING_WORD;
        }
    }

    breakdown->total = breakdown->name_score + breakdown->signature_score +
                       breakdown->summary_score + breakdown->keyword_score +
                       breakdown->docstring_score;
    return breakdown->total;
}
