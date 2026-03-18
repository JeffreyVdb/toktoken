/*
 * jinja_strip.h -- Strip Jinja2 template syntax from SQL files.
 *
 * Replaces {{ }}, {% %}, {# #} with spaces, preserving newlines
 * so that line numbers remain stable for ctags parsing.
 */

#ifndef TT_JINJA_STRIP_H
#define TT_JINJA_STRIP_H

#include <stddef.h>

/*
 * tt_jinja_strip -- Strip Jinja2 template delimiters from content.
 *
 * Returns a new buffer with Jinja blocks replaced by spaces (newlines preserved).
 * Returns NULL if content has no Jinja delimiters (no allocation needed).
 * Caller must free the returned buffer.
 */
char *tt_jinja_strip(const char *content, size_t len, size_t *out_len);

#endif /* TT_JINJA_STRIP_H */
