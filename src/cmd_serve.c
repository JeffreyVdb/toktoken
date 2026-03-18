/*
 * cmd_serve.c -- MCP server command (toktoken serve).
 */

#include "cmd_serve.h"
#include "mcp_server.h"
#include "error.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>

int tt_cmd_serve(tt_cli_opts_t *opts)
{
    const char *project_root = opts->path;

    /* Default: cwd */
    char *cwd = NULL;
    if (!project_root || project_root[0] == '\0')
    {
        cwd = tt_getcwd();
        if (!cwd)
        {
            fprintf(stderr, "[mcp] Error: cannot determine working directory\n");
            return 1;
        }
        project_root = cwd;
    }

    /* Resolve to canonical path */
    char *resolved = tt_realpath(project_root);
    if (resolved)
    {
        free(cwd);
        cwd = NULL;
        project_root = resolved;
    }

    fprintf(stderr, "[mcp] TokToken MCP server starting\n");
    fprintf(stderr, "[mcp] Project root: %s\n", project_root);
    fprintf(stderr, "[mcp] Protocol version: %s\n", TT_MCP_PROTOCOL_VERSION);
    fprintf(stderr, "[mcp] Waiting for client connection on stdin...\n");

    tt_mcp_server_t srv;
    if (tt_mcp_server_init(&srv, project_root) < 0)
    {
        fprintf(stderr, "[mcp] Error: failed to initialize server: %s\n",
                tt_error_get());
        free(resolved);
        free(cwd);
        return 1;
    }

    int result = tt_mcp_server_run(&srv);

    fprintf(stderr, "[mcp] Server shutting down\n");
    tt_mcp_server_free(&srv);

    free(resolved);
    free(cwd);

    return result < 0 ? 1 : 0;
}
