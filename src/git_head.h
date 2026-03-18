/*
 * git_head.h -- Read the current git HEAD commit SHA.
 */

#ifndef TT_GIT_HEAD_H
#define TT_GIT_HEAD_H

/*
 * tt_git_head -- Get the current HEAD commit SHA for a project.
 *
 * Reads .git/HEAD directly (no subprocess).
 * If HEAD is a symbolic ref (e.g. "ref: refs/heads/main"), resolves it.
 * If HEAD is a detached SHA, uses that directly.
 * Falls back to packed-refs if the loose ref file is missing.
 *
 * Returns 40-char hex string (caller frees), or empty string "" if
 * the directory is not a git repo.
 */
char *tt_git_head(const char *project_root);

#endif /* TT_GIT_HEAD_H */
