/*
 * mcp_server.h -- MCP (Model Context Protocol) server core.
 *
 * Message loop, JSON-RPC 2.0 dispatch, lifecycle management.
 * The server reads newline-delimited JSON from stdin, writes responses
 * to stdout, and logs diagnostics to stderr.
 *
 * Ref: MCP spec https://modelcontextprotocol.io/specification/2025-11-25
 */

#ifndef TT_MCP_SERVER_H
#define TT_MCP_SERVER_H

#include "database.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <cJSON.h>

/* Protocol version supported by this server */
#define TT_MCP_PROTOCOL_VERSION "2025-11-25"

/* Maximum static line buffer (1 MB). Lines exceeding this use getline(). */
#define TT_MCP_MAX_LINE (1024 * 1024)

/* JSON-RPC 2.0 standard error codes */
#define TT_JSONRPC_PARSE_ERROR      -32700
#define TT_JSONRPC_INVALID_REQUEST  -32600
#define TT_JSONRPC_METHOD_NOT_FOUND -32601
#define TT_JSONRPC_INVALID_PARAMS   -32602
#define TT_JSONRPC_INTERNAL_ERROR   -32603

/* MCP-specific error codes */
#define TT_MCP_NOT_INITIALIZED      -32002

/*
 * Server state.
 */
typedef struct tt_mcp_server_t {
    bool initialized;       /* true after initialize handshake */
    bool running;           /* false -> exit loop */
    char *project_root;     /* [owns] project path (from --path or cwd) */
    tt_database_t *db;      /* [owns] lazily opened database (may be NULL) */
    /* Staleness detection cache */
    uint64_t stale_check_ms;    /* monotonic ms of last check */
    bool stale_cached;          /* cached result */
    char stale_reason[256];     /* cached reason string */
    /* Progress token for current tool call (set by dispatch, cleared after) */
    const char *progress_token; /* [borrows] NULL if no progress requested */
} tt_mcp_server_t;

/*
 * tt_mcp_server_init -- Initialize server state.
 *
 * project_root is duplicated internally.
 * Returns 0 on success, -1 on error.
 */
int tt_mcp_server_init(tt_mcp_server_t *srv, const char *project_root);

/*
 * tt_mcp_server_run -- Execute the message loop (blocking).
 *
 * Reads from stdin, writes to stdout, logs to stderr.
 * Returns 0 on clean shutdown, -1 on fatal error.
 */
int tt_mcp_server_run(tt_mcp_server_t *srv);

/*
 * tt_mcp_server_free -- Release all server resources.
 */
void tt_mcp_server_free(tt_mcp_server_t *srv);

/* ---- JSON-RPC helpers (used by mcp_tools.c) ---- */

/*
 * mcp_make_result -- Build a JSON-RPC 2.0 success response.
 *
 * Takes ownership of result. [caller-frees returned cJSON]
 */
cJSON *mcp_make_result(const cJSON *id, cJSON *result);

/*
 * mcp_make_error -- Build a JSON-RPC 2.0 error response.
 *
 * [caller-frees returned cJSON]
 */
cJSON *mcp_make_error(const cJSON *id, int code, const char *message);

/*
 * mcp_tool_error -- Build a tool-level error JSON (not a protocol error).
 *
 * Returns {"error":"message"}. [caller-frees]
 */
cJSON *mcp_tool_error(const char *message);

/*
 * mcp_ensure_db -- Open the database lazily.
 *
 * Returns 0 if database is available, -1 on error.
 * If already open, returns 0 immediately.
 */
int mcp_ensure_db(tt_mcp_server_t *srv);

/* ---- Progress notifications ---- */

/*
 * mcp_send_progress -- Send a progress notification to the client.
 *
 * Only sends if srv->progress_token is set. Safe to call unconditionally.
 * progress and total are file counts; message is a human-readable string.
 */
void mcp_send_progress(tt_mcp_server_t *srv, int64_t progress, int64_t total,
                        const char *message);

/* ---- JSON extraction helpers ---- */

int mcp_get_int_or_default(const cJSON *obj, const char *key, int def);
bool mcp_get_bool(const cJSON *obj, const char *key);

#endif /* TT_MCP_SERVER_H */
