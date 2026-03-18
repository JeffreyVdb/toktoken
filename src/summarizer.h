/*
 * summarizer.h -- Symbol summary generation (Tier 1 + Tier 3).
 *
 * Tier 1: Extract first sentence from docstring.
 * (Tier 2 AI summarization is out of scope.)
 * Tier 3: Generate "{KindLabel} {name}" fallback.
 */

#ifndef TT_SUMMARIZER_H
#define TT_SUMMARIZER_H

#include "symbol_kind.h"
#include "database.h"

#include <stdbool.h>

/*
 * tt_summarize_docstring -- Tier 1: Extract first sentence from docstring.
 *
 * Algorithm:
 *   1. Trim. Empty → return ""
 *   2. Split by "\n"
 *   3. Collect lines until blank line or @tag
 *   4. Join with space, extract first sentence (up to ". ")
 *   5. Truncate to 120 chars (117 + "...")
 *
 * [caller-frees]
 */
char *tt_summarize_docstring(const char *docstring);

/*
 * tt_summarize_signature -- Tier 3: Generate summary from kind + name.
 *
 * Format: "{KindLabel} {name}" (e.g. "Class User", "Method save")
 *
 * [caller-frees]
 */
char *tt_summarize_signature(tt_symbol_kind_e kind, const char *name);

/*
 * tt_is_tier3_fallback -- Check if summary is a Tier 3 auto-generated fallback.
 *
 * Returns true if:
 *   - summary is empty
 *   - summary == "{KindLabel} {name}" (exact match)
 */
bool tt_is_tier3_fallback(const char *summary, tt_symbol_kind_e kind, const char *name);

/*
 * tt_apply_sync_summaries -- Apply Tier 1 + Tier 3 summaries to all symbols.
 *
 * Loads symbols without summary from DB, generates summaries using
 * docstring (Tier 1) with signature fallback (Tier 3), and updates DB.
 *
 * Returns 0 on success, -1 on error.
 */
int tt_apply_sync_summaries(tt_database_t *db);

#endif /* TT_SUMMARIZER_H */
