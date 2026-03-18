/*
 * line_offsets.h -- Map line numbers to byte offsets in file content.
 *
 * Scans file content for newlines and builds a lookup table.
 * Line numbers are 1-indexed: line 1 starts at offset 0.
 */

#ifndef TT_LINE_OFFSETS_H
#define TT_LINE_OFFSETS_H

#include <stddef.h>

typedef struct
{
    int *offsets; /* [owns] offsets[i] = byte offset of line (i+1) */
    int count;    /* number of lines */
} tt_line_offsets_t;

/*
 * tt_line_offsets_build -- Build line offset table from file content.
 *
 * Line 1 starts at offset 0. Each subsequent line starts after a '\n'.
 */
void tt_line_offsets_build(tt_line_offsets_t *lo, const char *content, size_t len);

/*
 * tt_line_offsets_offset_at -- Get byte offset for a 1-indexed line.
 *
 * Returns 0 if line is out of range.
 */
int tt_line_offsets_offset_at(const tt_line_offsets_t *lo, int line);

/*
 * tt_line_offsets_line_at -- Find the 1-indexed line for a byte offset.
 *
 * Uses linear scan (matches PHP behavior).
 * Returns 1 if offset is before the first newline.
 */
int tt_line_offsets_line_at(const tt_line_offsets_t *lo, int byte_offset);

/*
 * tt_line_offsets_free -- Free the offset array.
 */
void tt_line_offsets_free(tt_line_offsets_t *lo);

#endif /* TT_LINE_OFFSETS_H */
