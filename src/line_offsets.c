/*
 * line_offsets.c -- Map line numbers to byte offsets in file content.
 */

#include "line_offsets.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

void tt_line_offsets_build(tt_line_offsets_t *lo, const char *content, size_t len)
{
    if (!lo)
        return;
    lo->offsets = NULL;
    lo->count = 0;

    if (!content || len == 0)
    {
        lo->count = 1;
        lo->offsets = malloc(sizeof(int));
        if (lo->offsets)
            lo->offsets[0] = 0;
        return;
    }

    /* Count newlines to determine number of lines.
     * Clamp to INT_MAX-1 to avoid overflow in line_count+1 and offset array. */
    int nl_count = 0;
    for (size_t i = 0; i < len; i++)
    {
        if (content[i] == '\n')
        {
            if (nl_count == INT_MAX - 1)
                break;
            nl_count++;
        }
    }

    /* Number of lines = newlines + 1 (last line may not end with \n). */
    int line_count = nl_count + 1;
    int *offsets = malloc((size_t)line_count * sizeof(int));
    if (!offsets)
        return;

    offsets[0] = 0;
    int idx = 1;
    for (size_t i = 0; i < len && idx < line_count; i++)
    {
        if (content[i] == '\n')
        {
            offsets[idx] = (i + 1 <= (size_t)INT_MAX) ? (int)(i + 1) : INT_MAX;
            idx++;
        }
    }

    lo->offsets = offsets;
    lo->count = line_count;
}

int tt_line_offsets_offset_at(const tt_line_offsets_t *lo, int line)
{
    if (!lo || !lo->offsets)
        return 0;
    if (line < 1 || line > lo->count)
        return 0;
    return lo->offsets[line - 1];
}

int tt_line_offsets_line_at(const tt_line_offsets_t *lo, int byte_offset)
{
    if (!lo || !lo->offsets || lo->count == 0)
        return 1;

    /* Binary search: find the last offset <= byte_offset */
    int low = 0, high = lo->count - 1, result = 0;
    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        if (lo->offsets[mid] <= byte_offset)
        {
            result = mid;
            low = mid + 1;
        }
        else
        {
            high = mid - 1;
        }
    }
    return result + 1;
}

void tt_line_offsets_free(tt_line_offsets_t *lo)
{
    if (!lo)
        return;
    free(lo->offsets);
    lo->offsets = NULL;
    lo->count = 0;
}
