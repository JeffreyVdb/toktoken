/*
 * cmd_manage.h -- Management commands: stats, projects:list, cache:clear, codebase:detect.
 *
 * Dual-mode: _exec() returns cJSON* for MCP, wrapper returns exit code for CLI.
 */

#ifndef TT_CMD_MANAGE_H
#define TT_CMD_MANAGE_H

#include "cli.h"

#include <cJSON.h>

/* ---- Core functions (return cJSON*, caller frees) ---- */

cJSON *tt_cmd_stats_exec(tt_cli_opts_t *opts);
cJSON *tt_cmd_projects_list_exec(tt_cli_opts_t *opts);
cJSON *tt_cmd_cache_clear_exec(tt_cli_opts_t *opts);

/*
 * tt_cmd_codebase_detect_exec -- Core codebase:detect logic.
 *
 * out_exit_code receives 0 (is codebase) or 1 (not a codebase).
 */
cJSON *tt_cmd_codebase_detect_exec(tt_cli_opts_t *opts, int *out_exit_code);

/* ---- CLI wrappers (print JSON, return exit code) ---- */

int tt_cmd_stats(tt_cli_opts_t *opts);
int tt_cmd_projects_list(tt_cli_opts_t *opts);
int tt_cmd_cache_clear(tt_cli_opts_t *opts);
int tt_cmd_codebase_detect(tt_cli_opts_t *opts);

#endif /* TT_CMD_MANAGE_H */
