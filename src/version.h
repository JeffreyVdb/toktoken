/*
 * version.h -- TokToken version information
 */

#ifndef TT_VERSION_H
#define TT_VERSION_H

/* TT_VERSION is injected by CMake from project(VERSION ...) */
#ifndef TT_VERSION
#define TT_VERSION "0.0.0-dev"
#endif

/*
 * tt_version -- Returns the version string.
 */
const char *tt_version(void);

#endif /* TT_VERSION_H */
