/*
 * cmd_update.h -- Self-update command.
 *
 * Downloads, verifies (SHA256), and atomically replaces the running binary.
 */

#ifndef TT_CMD_UPDATE_H
#define TT_CMD_UPDATE_H

#include "cli.h"

#include <cJSON.h>

/* ---- Core function (returns cJSON*, caller frees) ---- */

cJSON *tt_cmd_self_update_exec(tt_cli_opts_t *opts);

/* ---- CLI wrapper (prints JSON, returns exit code) ---- */

int tt_cmd_self_update(tt_cli_opts_t *opts);

#endif /* TT_CMD_UPDATE_H */
