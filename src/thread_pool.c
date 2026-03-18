/*
 * thread_pool.c -- CPU count detection for parallel workers.
 */

#include "thread_pool.h"
#include "platform.h"

#ifdef TT_PLATFORM_UNIX
#include <unistd.h>
#endif
#ifdef TT_PLATFORM_WINDOWS
#include <windows.h>
#endif

int tt_cpu_count(void)
{
    int n = 4; /* fallback */

#ifdef TT_PLATFORM_UNIX
    long val = sysconf(_SC_NPROCESSORS_ONLN);
    if (val > 0)
        n = (int)val;
#endif
#ifdef TT_PLATFORM_WINDOWS
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    n = (int)si.dwNumberOfProcessors;
#endif

    if (n < 1)
        n = 1;
    if (n > 16)
        n = 16;
    return n;
}
