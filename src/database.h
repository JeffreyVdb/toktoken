/*
 * database.h -- SQLite database wrapper for TokToken.
 *
 * Handles database lifecycle: open (with directory creation, schema
 * creation/migration, PRAGMA setup), close, transactions, and existence checks.
 */

#ifndef TT_DATABASE_H
#define TT_DATABASE_H

#include <sqlite3.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    sqlite3 *db;
    char *path;         /* [owns] path to db.sqlite */
    char *project_root; /* [owns] path to the indexed project */
    bool in_transaction;
} tt_database_t;

/*
 * tt_database_open -- Open (or create) the database for a project.
 *
 * Creates the directory if needed, opens SQLite, applies PRAGMAs,
 * and creates/migrates the schema.
 * Returns 0 on success, -1 on error.
 */
int tt_database_open(tt_database_t *db, const char *project_root);

/*
 * tt_database_close -- Close the database and free resources.
 */
void tt_database_close(tt_database_t *db);

/*
 * tt_database_exists -- Check if a database file exists for a project.
 */
bool tt_database_exists(const char *project_root);

/*
 * tt_database_get_path -- Compute the database file path for a project.
 *
 * [caller-frees] Returns NULL on error.
 */
char *tt_database_get_path(const char *project_root);

/*
 * tt_database_begin -- Start a transaction.
 *
 * Returns 0 on success, -1 on error.
 */
int tt_database_begin(tt_database_t *db);

/*
 * tt_database_commit -- Commit the current transaction.
 *
 * Returns 0 on success, -1 on error.
 */
int tt_database_commit(tt_database_t *db);

/*
 * tt_database_rollback -- Rollback the current transaction.
 *
 * Safe to call when not in a transaction (no-op).
 * Returns 0 on success, -1 on error.
 */
int tt_database_rollback(tt_database_t *db);

/*
 * tt_database_size -- Get the database file size in bytes.
 *
 * Returns -1 on error.
 */
int64_t tt_database_size(tt_database_t *db);

#endif /* TT_DATABASE_H */
