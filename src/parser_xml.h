/*
 * parser_xml.h -- Parser for XML/XUL (.xml, .xul) files.
 */

#ifndef TT_PARSER_XML_H
#define TT_PARSER_XML_H

#include "symbol.h"

int tt_parse_xml(const char *project_root, const char **file_paths, int file_count,
                  tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_XML_H */
