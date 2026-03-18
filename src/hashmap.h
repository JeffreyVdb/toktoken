/*
 * hashmap.h -- Hash table with string keys and void* values.
 *
 * Open addressing with linear probing, FNV-1a hash function.
 * Keys are copied internally; values are caller-owned (void*).
 *
 * Guarantees O(1) amortized lookup. Used for:
 *   - Change detection: path -> hash
 *   - Deduplication FTS: key -> counter
 *   - Gitignore rules per directory: rel_dir -> rules
 *   - Symbol deduplication: id -> bool
 */

#ifndef TT_HASHMAP_H
#define TT_HASHMAP_H

#include <stddef.h>
#include <stdbool.h>

typedef struct tt_hashmap tt_hashmap_t;

/*
 * tt_hashmap_new -- Create a hashmap with suggested initial capacity.
 *
 * initial_cap is rounded up to the next power of 2.
 */
tt_hashmap_t *tt_hashmap_new(size_t initial_cap);

/*
 * tt_hashmap_free -- Free the hashmap.
 *
 * Does NOT free values (void*). Use tt_hashmap_free_with_values for that.
 */
void tt_hashmap_free(tt_hashmap_t *m);

/*
 * tt_hashmap_free_with_values -- Free the hashmap AND call free() on each value.
 */
void tt_hashmap_free_with_values(tt_hashmap_t *m);

/*
 * tt_hashmap_set -- Insert or update a key-value pair.
 *
 * The key is copied internally. If the key already exists, the value is
 * overwritten and the old value is returned (caller must free it if needed).
 * If the key is new, returns NULL.
 */
void *tt_hashmap_set(tt_hashmap_t *m, const char *key, void *value);

/*
 * tt_hashmap_get -- Look up a value by key.
 *
 * Returns the value or NULL if not found.
 */
void *tt_hashmap_get(const tt_hashmap_t *m, const char *key);

/*
 * tt_hashmap_has -- Check if a key exists.
 */
bool tt_hashmap_has(const tt_hashmap_t *m, const char *key);

/*
 * tt_hashmap_remove -- Remove a key and return its value.
 *
 * Returns the removed value, or NULL if not found.
 */
void *tt_hashmap_remove(tt_hashmap_t *m, const char *key);

/*
 * tt_hashmap_count -- Number of entries in the hashmap.
 */
size_t tt_hashmap_count(const tt_hashmap_t *m);

/*
 * tt_hashmap_iter -- Iterate over all entries.
 *
 * Calls cb for each (key, value) pair. If cb returns non-zero, stops.
 */
typedef int (*tt_hashmap_iter_cb)(const char *key, void *value, void *userdata);
void tt_hashmap_iter(const tt_hashmap_t *m, tt_hashmap_iter_cb cb, void *userdata);

#endif /* TT_HASHMAP_H */
