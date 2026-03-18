/*
 * secret_patterns.h -- Detection of secret/credential files.
 */

#ifndef TT_SECRET_PATTERNS_H
#define TT_SECRET_PATTERNS_H

#include <stdbool.h>

/*
 * tt_is_secret -- Check if a filename matches a secret pattern.
 *
 * Takes the basename only (not a full path).
 * Broad patterns (e.g. *secret*) are excluded for documentation files.
 */
bool tt_is_secret(const char *filename);

#endif /* TT_SECRET_PATTERNS_H */
