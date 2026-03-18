/*
 * database.c -- SQLite database wrapper for TokToken.
 */

#include "database.h"
#include "schema.h"
#include "storage_paths.h"
#include "platform.h"
#include "error.h"
#include "str_util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* PRAGMAs */
static const char *PRAGMAS =
    "PRAGMA journal_mode=WAL;"
    "PRAGMA foreign_keys=ON;"
    "PRAGMA synchronous=NORMAL;"
    "PRAGMA busy_timeout=5000;"
    "PRAGMA cache_size=-8000;"
    "PRAGMA mmap_size=268435456;";

int tt_database_open(tt_database_t *db, const char *project_root)
{
    memset(db, 0, sizeof(*db));

    char *db_path = tt_storage_db_path(project_root);
    if (!db_path)
    {
        tt_error_set("database: cannot compute db path for '%s'", project_root);
        return -1;
    }

    /* Ensure parent directory exists */
    char *dir = tt_storage_project_dir(project_root);
    if (!dir)
    {
        free(db_path);
        return -1;
    }

    if (tt_mkdir_p(dir) < 0)
    {
        tt_error_set("database: cannot create directory '%s'", dir);
        free(dir);
        free(db_path);
        return -1;
    }
    free(dir);

    /* Open SQLite */
    int rc = sqlite3_open(db_path, &db->db);
    if (rc != SQLITE_OK)
    {
        tt_error_set("database: sqlite3_open failed: %s",
                     db->db ? sqlite3_errmsg(db->db) : "out of memory");
        if (db->db)
        {
            sqlite3_close(db->db);
            db->db = NULL;
        }
        free(db_path);
        return -1;
    }

    db->path = db_path;
    db->project_root = tt_strdup(project_root);

    /* Apply PRAGMAs */
    char *errmsg = NULL;
    rc = sqlite3_exec(db->db, PRAGMAS, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        tt_error_set("database: PRAGMA failed: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        tt_database_close(db);
        return -1;
    }

    /* Create or migrate schema */
    int version = tt_schema_check_version(db->db);
    if (version < TT_SCHEMA_VERSION)
    {
        if (tt_schema_migrate(db->db) < 0)
        {
            tt_database_close(db);
            return -1;
        }
    }

    return 0;
}

void tt_database_close(tt_database_t *db)
{
    if (!db)
        return;

    if (db->in_transaction)
    {
        tt_database_rollback(db);
    }

    if (db->db)
    {
        sqlite3_close(db->db);
        db->db = NULL;
    }

    free(db->path);
    db->path = NULL;
    free(db->project_root);
    db->project_root = NULL;
    db->in_transaction = false;
}

bool tt_database_exists(const char *project_root)
{
    char *db_path = tt_storage_db_path(project_root);
    if (!db_path)
        return false;

    bool exists = tt_file_exists(db_path);
    free(db_path);
    return exists;
}

char *tt_database_get_path(const char *project_root)
{
    return tt_storage_db_path(project_root);
}

int tt_database_begin(tt_database_t *db)
{
    if (db->in_transaction)
        return 0;

    char *errmsg = NULL;
    int rc = sqlite3_exec(db->db, "BEGIN", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        tt_error_set("database: BEGIN failed: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return -1;
    }
    db->in_transaction = true;
    return 0;
}

int tt_database_commit(tt_database_t *db)
{
    if (!db->in_transaction)
        return 0;

    char *errmsg = NULL;
    int rc = sqlite3_exec(db->db, "COMMIT", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK)
    {
        tt_error_set("database: COMMIT failed: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return -1;
    }
    db->in_transaction = false;
    return 0;
}

int tt_database_rollback(tt_database_t *db)
{
    if (!db->in_transaction)
        return 0;

    char *errmsg = NULL;
    int rc = sqlite3_exec(db->db, "ROLLBACK", NULL, NULL, &errmsg);
    db->in_transaction = false;
    if (rc != SQLITE_OK)
    {
        tt_error_set("database: ROLLBACK failed: %s", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

int64_t tt_database_size(tt_database_t *db)
{
    if (!db || !db->path)
        return -1;
    return tt_file_size(db->path);
}
