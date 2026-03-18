/*
 * cmd_github.h -- GitHub repository indexing and management commands.
 *
 * Commands:
 *   index:github <owner/repo>   Clone/update and index a GitHub repository
 *   repos:list                  List all cloned repositories
 *   repos:remove <owner/repo>   Remove a cloned repository
 *   repos:clear --confirm       Remove all cloned repositories
 *
 * Dual-mode: _exec() returns cJSON* for MCP, wrapper returns exit code for CLI.
 */

#ifndef TT_CMD_GITHUB_H
#define TT_CMD_GITHUB_H

#include "cli.h"

#include <cJSON.h>

/* ---- Core functions (return cJSON*, caller frees) ---- */

/*
 * tt_cmd_index_github_exec -- Core index:github logic.
 *
 * opts->positional[0]: "owner/repo"
 * opts->depth:  clone depth (default 1 = shallow)
 * opts->all:    --full-clone (depth=0)
 * opts->confirm: --force (re-clone)
 *
 * Returns cJSON result, or NULL on internal error.
 */
cJSON *tt_cmd_index_github_exec(tt_cli_opts_t *opts);

/*
 * tt_cmd_repos_list_exec -- List cloned repositories.
 */
cJSON *tt_cmd_repos_list_exec(tt_cli_opts_t *opts);

/*
 * tt_cmd_repos_remove_exec -- Remove a specific cloned repository.
 */
cJSON *tt_cmd_repos_remove_exec(tt_cli_opts_t *opts);

/*
 * tt_cmd_repos_clear_exec -- Remove all cloned repositories.
 *
 * Requires opts->confirm == true.
 */
cJSON *tt_cmd_repos_clear_exec(tt_cli_opts_t *opts);

/* ---- CLI wrappers (print JSON, return exit code) ---- */

int tt_cmd_index_github(tt_cli_opts_t *opts);
int tt_cmd_repos_list(tt_cli_opts_t *opts);
int tt_cmd_repos_remove(tt_cli_opts_t *opts);
int tt_cmd_repos_clear(tt_cli_opts_t *opts);

#endif /* TT_CMD_GITHUB_H */
