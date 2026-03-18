/*
 * parser_hcl.h -- Parser for HCL/Terraform (.tf, .hcl, .tfvars) files.
 */

#ifndef TT_PARSER_HCL_H
#define TT_PARSER_HCL_H

#include "symbol.h"

int tt_parse_hcl(const char *project_root, const char **file_paths, int file_count,
                  tt_symbol_t **out, int *out_count);

#endif /* TT_PARSER_HCL_H */
