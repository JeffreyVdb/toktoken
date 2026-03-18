/*
 * parser_graphql.h -- Parser for GraphQL (.graphql, .gql) files.
 */

#ifndef TT_PARSER_GRAPHQL_H
#define TT_PARSER_GRAPHQL_H

#include "symbol.h"

int tt_parse_graphql(const char *project_root, const char **file_paths, int file_count,
                      tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_GRAPHQL_H */
