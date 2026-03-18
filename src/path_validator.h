/*
 * path_validator.h -- Path validation and symlink escape detection.
 */

#ifndef TT_PATH_VALIDATOR_H
#define TT_PATH_VALIDATOR_H

#include <stdbool.h>

/*
 * tt_path_validate -- Check that path is a child of root.
 *
 * Both paths are resolved via realpath before comparison.
 * Returns true if resolved_path equals or is under resolved_root.
 */
bool tt_path_validate(const char *path, const char *root);

/*
 * tt_is_symlink_escape -- Check if a symlink points outside root.
 *
 * Returns true if path is a symlink whose target resolves outside root
 * (or whose target does not exist).
 * Returns false if path is not a symlink.
 */
bool tt_is_symlink_escape(const char *path, const char *root);

#endif /* TT_PATH_VALIDATOR_H */
