/*
 * parser_vue.h -- Parser for Vue Single File Components (.vue).
 *
 * Extracts component structure: defineProps/defineEmits, composables (useXxx),
 * named functions, computed, ref/reactive, and Options API methods.
 */

#ifndef TT_PARSER_VUE_H
#define TT_PARSER_VUE_H

#include "symbol.h"

/*
 * tt_parse_vue -- Parse Vue SFC files and extract symbols.
 *
 * project_root: absolute path to the project root.
 * file_paths:   array of RELATIVE paths to .vue files.
 * file_count:   number of file paths.
 * out:          receives allocated array of symbols [caller-frees with tt_symbol_array_free].
 * out_count:    receives number of symbols produced.
 *
 * Returns 0 on success, -1 on error.
 */
int tt_parse_vue(const char *project_root, const char **file_paths, int file_count,
                 tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_VUE_H */
