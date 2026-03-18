/*
 * error.h -- Thread-local error buffer for structured error reporting.
 *
 * Allows functions to return -1 and the caller to read the reason.
 *
 * Example:
 *   if (tt_database_open(&db, path) < 0) {
 *       fprintf(stderr, "Error: %s\n", tt_error_get());
 *       return -1;
 *   }
 */

#ifndef TT_ERROR_H
#define TT_ERROR_H

/*
 * tt_error_set -- Set the last error message (printf-like).
 *
 * Overwrites any previous error. The message is stored in a thread-local
 * buffer and remains valid until the next call to tt_error_set or
 * tt_error_clear.
 */
void tt_error_set(const char *fmt, ...);

/*
 * tt_error_get -- Return the last error message, or "" if none.
 *
 * [borrows] The returned pointer is valid until the next call to
 * tt_error_set or tt_error_clear.
 */
const char *tt_error_get(void);

/*
 * tt_error_clear -- Reset the error buffer to empty.
 */
void tt_error_clear(void);

#endif /* TT_ERROR_H */
