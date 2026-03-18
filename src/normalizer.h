/*
 * normalizer.h -- Normalize ctags language and kind names to canonical form.
 */

#ifndef TT_NORMALIZER_H
#define TT_NORMALIZER_H

/*
 * tt_normalize_language -- Normalize ctags language name to canonical.
 *
 * Lowercases first, then applies LANGUAGE_MAP (5 entries).
 * E.g.: "Sh" -> "bash", "C++" -> "cpp", "C#" -> "csharp".
 * Unknown languages are returned lowercased as-is.
 * Returns static string (do not free).
 */
const char *tt_normalize_language(const char *ctags_lang);

/*
 * tt_normalize_kind -- Normalize user kind input to canonical.
 *
 * Tries to match lowercased input against canonical kind names.
 * If recognized, returns canonical value. Otherwise returns input as-is.
 * Returns static string or the input pointer.
 */
const char *tt_normalize_kind(const char *kind);

#endif /* TT_NORMALIZER_H */
