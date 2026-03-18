/*
 * arena.h -- Arena allocator for batch allocation of correlated objects.
 *
 * Typical use: during ctags parsing, all Symbols and their strings are
 * allocated from an arena. When parsing completes, the arena is freed
 * in one shot, eliminating the risk of individual leaks.
 *
 * The arena allocates large blocks (default 64KB) and serves small
 * allocations from each block. Individual frees are not supported.
 */

#ifndef TT_ARENA_H
#define TT_ARENA_H

#include <stddef.h>

typedef struct tt_arena tt_arena_t;

/*
 * tt_arena_new -- Create an arena with the given block size.
 *
 * If block_size == 0, uses the default (64KB).
 * Returns NULL on allocation failure.
 */
tt_arena_t *tt_arena_new(size_t block_size);

/*
 * tt_arena_alloc -- Allocate size bytes from the arena.
 *
 * Aligned to 8 bytes. Returns NULL on allocation failure.
 */
void *tt_arena_alloc(tt_arena_t *a, size_t size);

/*
 * tt_arena_strdup -- Duplicate a string into the arena.
 *
 * [borrows] The pointer is valid until tt_arena_free().
 */
char *tt_arena_strdup(tt_arena_t *a, const char *s);

/*
 * tt_arena_strndup -- Duplicate up to n bytes + null terminator.
 */
char *tt_arena_strndup(tt_arena_t *a, const char *s, size_t n);

/*
 * tt_arena_free -- Free the entire arena and all its blocks.
 */
void tt_arena_free(tt_arena_t *a);

/*
 * tt_arena_used -- Total bytes allocated from the arena.
 */
size_t tt_arena_used(const tt_arena_t *a);

#endif /* TT_ARENA_H */
