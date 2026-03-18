/*
 * cmd_serve.h -- MCP server command (toktoken serve).
 *
 * Entry point for the MCP server running on STDIO transport.
 * Starts the JSON-RPC 2.0 message loop and blocks until EOF or signal.
 */

#ifndef TT_CMD_SERVE_H
#define TT_CMD_SERVE_H

#include "cli.h"

/*
 * tt_cmd_serve -- Start the MCP server.
 *
 * Reads --path from opts (default: cwd). Runs the message loop
 * on stdin/stdout. Returns 0 on clean shutdown, 1 on fatal error.
 */
int tt_cmd_serve(tt_cli_opts_t *opts);

#endif /* TT_CMD_SERVE_H */
