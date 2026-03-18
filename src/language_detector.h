/*
 * language_detector.h -- Detect programming language from file path.
 *
 * Maps file extensions to canonical language names.
 * Special case: .blade.php is detected before extension check.
 */

#ifndef TT_LANGUAGE_DETECTOR_H
#define TT_LANGUAGE_DETECTOR_H

/*
 * tt_detect_language -- Detect language from file path.
 *
 * Checks .blade.php first, then uses extension mapping.
 * Returns static string (do not free).
 * Unknown extensions return the extension lowercased as-is.
 */
const char *tt_detect_language(const char *file_path);

/*
 * tt_language_from_extension -- Map extension (without dot) to language.
 *
 * Returns static string (do not free).
 * Unknown extensions return NULL.
 */
const char *tt_language_from_extension(const char *ext);

/*
 * tt_lang_set_extra_extensions -- Set runtime extension->language overrides.
 *
 * Borrows the arrays (must remain valid until clear is called).
 * Extra extensions take priority over built-in mappings.
 */
void tt_lang_set_extra_extensions(const char **ext_keys, const char **languages, int count);

/*
 * tt_lang_clear_extra_extensions -- Clear runtime overrides.
 */
void tt_lang_clear_extra_extensions(void);

#endif /* TT_LANGUAGE_DETECTOR_H */
