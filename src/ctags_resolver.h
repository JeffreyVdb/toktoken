/*
 * ctags_resolver.h -- Find the Universal Ctags binary.
 *
 * Strategy (in order):
 * 1. Bundled binary relative to toktoken executable
 * 2. "universal-ctags" in PATH
 * 3. "ctags" in PATH (must be Universal, not Exuberant)
 */

#ifndef TT_CTAGS_RESOLVER_H
#define TT_CTAGS_RESOLVER_H

/*
 * tt_ctags_resolve -- Find the ctags binary path.
 *
 * [caller-frees] Returns NULL if not found.
 */
char *tt_ctags_resolve(void);

/*
 * tt_ctags_version -- Get ctags version string (first line of --version).
 *
 * [caller-frees] Returns NULL on error.
 */
char *tt_ctags_version(const char *ctags_path);

#endif /* TT_CTAGS_RESOLVER_H */
