/*
 * thread_pool.h -- CPU count detection for parallel workers.
 */

#ifndef TT_THREAD_POOL_H
#define TT_THREAD_POOL_H

/*
 * tt_cpu_count -- Return usable CPU count for parallel work.
 *
 * Returns min(online_cpus, 8). Falls back to 4 on error.
 */
int tt_cpu_count(void);

#endif /* TT_THREAD_POOL_H */
