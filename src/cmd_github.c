/*
 * cmd_github.c -- GitHub repository indexing and management commands.
 */

#include "cmd_github.h"
#include "github.h"
#include "cmd_index.h"
#include "json_output.h"
#include "error.h"
#include "platform.h"
#include "str_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Helpers ---- */

static cJSON *make_error(const char *code, const char *message, const char *hint)
{
    cJSON *json = cJSON_CreateObject();
    if (!json)
        return NULL;
    cJSON_AddStringToObject(json, "error", code);
    cJSON_AddStringToObject(json, "message", message);
    if (hint && hint[0])
        cJSON_AddStringToObject(json, "hint", hint);
    return json;
}

/* ---- index:github ---- */

cJSON *tt_cmd_index_github_exec(tt_cli_opts_t *opts)
{
    /* Get repo spec from positional args or search field */
    const char *repo_spec = NULL;
    if (opts->positional_count > 0)
        repo_spec = opts->positional[0];
    else if (opts->search)
        repo_spec = opts->search;

    if (!repo_spec || repo_spec[0] == '\0')
    {
        return make_error("missing_argument",
                          "Missing repository argument",
                          "Usage: toktoken index:github owner/repo");
    }

    /* 1. Validate owner/repo (cheap, no I/O) */
    char owner[64], repo[128];
    if (tt_gh_validate_repo(repo_spec, owner, sizeof(owner),
                            repo, sizeof(repo)) < 0)
    {
        return make_error("invalid_repository", tt_error_get(), NULL);
    }

    /* 2. Check gh CLI */
    if (tt_gh_check() < 0)
    {
        const char *err = tt_error_get();
        if (strstr(err, "non trovato") || strstr(err, "not found"))
        {
            return make_error("gh_not_found", err,
                              "Installa gh: https://cli.github.com/ — poi esegui 'gh auth login'");
        }
        return make_error("gh_not_authenticated", err,
                          "Esegui 'gh auth login' per autenticarti con il tuo account GitHub");
    }

    /* 3. Determine target directory */
    char *target_dir = tt_gh_repo_dir(owner, repo);
    if (!target_dir)
    {
        return make_error("internal_error", "Cannot determine repository directory", NULL);
    }

    /* 4. Clone or pull */
    bool already_exists = tt_gh_repo_exists(owner, repo);
    const char *action = NULL;
    char *pull_message = NULL;

    /* --force: re-clone */
    if (opts->force && already_exists)
    {
        tt_progress("Force re-clone: removing existing clone...\n");
        tt_gh_remove_repo(owner, repo);
        already_exists = false;
    }

    if (already_exists)
    {
        /* Pull */
        if (tt_gh_pull(target_dir, &pull_message) < 0)
        {
            cJSON *err = make_error("pull_failed", tt_error_get(), NULL);
            free(target_dir);
            return err;
        }
        action = "updated";
    }
    else
    {
        /* Clone */
        int depth = 1; /* shallow by default */
        if (opts->full_clone)
            depth = 0;
        if (opts->depth > 0)
            depth = opts->depth;

        const char *branch = opts->branch;

        if (tt_gh_clone(owner, repo, target_dir, depth, branch) < 0)
        {
            cJSON *err = make_error("clone_failed", tt_error_get(), NULL);
            free(target_dir);
            return err;
        }
        action = "cloned";
    }

    /* 5. Index the cloned directory (unless --update-only) */
    cJSON *index_result = NULL;

    if (!opts->update_only)
    {
        tt_cli_opts_t index_opts;
        memset(&index_opts, 0, sizeof(index_opts));
        index_opts.path = target_dir;
        index_opts.truncate_width = 120;

        if (already_exists)
        {
            index_result = tt_cmd_index_update_exec(&index_opts);
        }
        else
        {
            index_result = tt_cmd_index_create_exec(&index_opts);
        }
    }

    /* 6. Build output JSON */
    cJSON *result = cJSON_CreateObject();
    cJSON_AddTrueToObject(result, "success");
    cJSON_AddStringToObject(result, "action", action);

    char repo_full[256];
    snprintf(repo_full, sizeof(repo_full), "%s/%s", owner, repo);
    cJSON_AddStringToObject(result, "repository", repo_full);
    cJSON_AddStringToObject(result, "local_path", target_dir);

    if (pull_message)
    {
        /* Trim trailing newline */
        size_t len = strlen(pull_message);
        while (len > 0 && (pull_message[len - 1] == '\n' || pull_message[len - 1] == '\r'))
            pull_message[--len] = '\0';
        cJSON_AddStringToObject(result, "changes", pull_message);
        free(pull_message);
    }

    /* Attach index result if available */
    if (index_result)
    {
        /* Check if indexing returned an error */
        cJSON *idx_err = cJSON_GetObjectItemCaseSensitive(index_result, "error");
        if (idx_err)
        {
            /* Indexing failed but clone/pull succeeded */
            cJSON_AddItemToObject(result, "index_error", cJSON_Duplicate(index_result, 1));
        }
        else
        {
            /* Extract key metrics from index result */
            cJSON *index_info = cJSON_CreateObject();
            cJSON *files = cJSON_GetObjectItemCaseSensitive(index_result, "files_indexed");
            cJSON *symbols = cJSON_GetObjectItemCaseSensitive(index_result, "symbols_found");
            cJSON *changed = cJSON_GetObjectItemCaseSensitive(index_result, "files_changed");
            cJSON *duration = cJSON_GetObjectItemCaseSensitive(index_result, "duration_sec");

            if (files)
                cJSON_AddNumberToObject(index_info, "files_indexed",
                                        cJSON_GetNumberValue(files));
            if (symbols)
                cJSON_AddNumberToObject(index_info, "symbols_found",
                                        cJSON_GetNumberValue(symbols));
            if (changed)
                cJSON_AddNumberToObject(index_info, "files_changed",
                                        cJSON_GetNumberValue(changed));
            if (duration)
                cJSON_AddNumberToObject(index_info, "duration_sec",
                                        cJSON_GetNumberValue(duration));

            cJSON_AddItemToObject(result, "index", index_info);
        }
        cJSON_Delete(index_result);
    }

    free(target_dir);
    return result;
}

/* ---- repos:list ---- */

cJSON *tt_cmd_repos_list_exec(tt_cli_opts_t *opts)
{
    (void)opts;

    tt_gh_list_entry_t *entries = NULL;
    int count = 0;

    if (tt_gh_list_repos(&entries, &count) < 0)
    {
        return make_error("list_failed", tt_error_get(), NULL);
    }

    cJSON *result = cJSON_CreateObject();
    cJSON *repos = cJSON_CreateArray();

    for (int i = 0; i < count; i++)
    {
        cJSON *item = cJSON_CreateObject();
        char full_name[256];
        snprintf(full_name, sizeof(full_name), "%s/%s",
                 entries[i].owner, entries[i].repo);
        cJSON_AddStringToObject(item, "repository", full_name);
        cJSON_AddStringToObject(item, "local_path", entries[i].local_path);
        cJSON_AddItemToArray(repos, item);

        tt_gh_list_entry_free(&entries[i]);
    }
    free(entries);

    cJSON_AddItemToObject(result, "repos", repos);
    cJSON_AddNumberToObject(result, "count", count);

    return result;
}

/* ---- repos:remove ---- */

cJSON *tt_cmd_repos_remove_exec(tt_cli_opts_t *opts)
{
    const char *repo_spec = NULL;
    if (opts->positional_count > 0)
        repo_spec = opts->positional[0];
    else if (opts->search)
        repo_spec = opts->search;

    if (!repo_spec || repo_spec[0] == '\0')
    {
        return make_error("missing_argument",
                          "Missing repository argument",
                          "Usage: toktoken repos:remove owner/repo");
    }

    char owner[64], repo[128];
    if (tt_gh_validate_repo(repo_spec, owner, sizeof(owner),
                            repo, sizeof(repo)) < 0)
    {
        return make_error("invalid_repository", tt_error_get(), NULL);
    }

    if (!tt_gh_repo_exists(owner, repo))
    {
        return make_error("repo_not_found",
                          "Repository clone not found",
                          "Run 'toktoken repos:list' to see cloned repositories");
    }

    if (tt_gh_remove_repo(owner, repo) < 0)
    {
        return make_error("remove_failed", tt_error_get(), NULL);
    }

    cJSON *result = cJSON_CreateObject();
    cJSON_AddTrueToObject(result, "success");

    char full_name[256];
    snprintf(full_name, sizeof(full_name), "%s/%s", owner, repo);
    cJSON_AddStringToObject(result, "repository", full_name);
    cJSON_AddStringToObject(result, "action", "removed");

    return result;
}

/* ---- repos:clear ---- */

cJSON *tt_cmd_repos_clear_exec(tt_cli_opts_t *opts)
{
    if (!opts->confirm)
    {
        return make_error("confirmation_required",
                          "This will remove ALL cloned repositories",
                          "Add --confirm to proceed");
    }

    /* Count repos before clearing */
    tt_gh_list_entry_t *entries = NULL;
    int count = 0;
    tt_gh_list_repos(&entries, &count);
    for (int i = 0; i < count; i++)
        tt_gh_list_entry_free(&entries[i]);
    free(entries);

    if (tt_gh_remove_all_repos() < 0)
    {
        return make_error("clear_failed", tt_error_get(), NULL);
    }

    cJSON *result = cJSON_CreateObject();
    cJSON_AddTrueToObject(result, "success");
    cJSON_AddStringToObject(result, "action", "cleared");
    cJSON_AddNumberToObject(result, "repos_removed", count);

    return result;
}

/* ---- CLI wrappers ---- */

int tt_cmd_index_github(tt_cli_opts_t *opts)
{
    cJSON *result = tt_cmd_index_github_exec(opts);
    if (!result)
        return tt_output_error("internal_error", tt_error_get(), NULL);

    cJSON *err = cJSON_GetObjectItemCaseSensitive(result, "error");
    if (err)
    {
        tt_json_print(result);
        cJSON_Delete(result);
        return 1;
    }

    return tt_output_success(result);
}

int tt_cmd_repos_list(tt_cli_opts_t *opts)
{
    cJSON *result = tt_cmd_repos_list_exec(opts);
    if (!result)
        return tt_output_error("internal_error", tt_error_get(), NULL);
    return tt_output_success(result);
}

int tt_cmd_repos_remove(tt_cli_opts_t *opts)
{
    cJSON *result = tt_cmd_repos_remove_exec(opts);
    if (!result)
        return tt_output_error("internal_error", tt_error_get(), NULL);

    cJSON *err = cJSON_GetObjectItemCaseSensitive(result, "error");
    if (err)
    {
        tt_json_print(result);
        cJSON_Delete(result);
        return 1;
    }

    return tt_output_success(result);
}

int tt_cmd_repos_clear(tt_cli_opts_t *opts)
{
    cJSON *result = tt_cmd_repos_clear_exec(opts);
    if (!result)
        return tt_output_error("internal_error", tt_error_get(), NULL);

    cJSON *err = cJSON_GetObjectItemCaseSensitive(result, "error");
    if (err)
    {
        tt_json_print(result);
        cJSON_Delete(result);
        return 1;
    }

    return tt_output_success(result);
}
