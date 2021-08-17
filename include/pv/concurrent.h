#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_rwlock.h>

struct pv_concurrent_lock {
    rte_rwlock_t rte_lock;
};

/**
 * Init pv_concurrent_lock struct before use
 */
void pv_concurrent_lock_init(struct pv_concurrent_lock* lock);

/**
 * Set read lock before use
 */
void pv_concurrent_lock_read_lock(struct pv_concurrent_lock* lock);

/**
 * Unset read lock after use
 */
void pv_concurrent_lock_read_unlock(struct pv_concurrent_lock* lock);

/**
 * Set write lock before use
 */
void pv_concurrent_lock_write_lock(struct pv_concurrent_lock* lock);

/**
 * Unset write lock after use
 */
void pv_concurrent_lock_write_unlock(struct pv_concurrent_lock* lock);

/**
 * Run main loop
 * @param main_function main function to run
 * @param arg custom arguments to be passed on main_function
 * @param call_main If true, Also called on main lcore
 * @return main_function's exit code
 */
int pv_concurrent_run_main(int main_function(void*), void* arg, bool call_main);

/**
 * Run worker loop
 * @param worker_function worker function to run
 * @param arg custom arguments to be passed on main_function
 * @param worker_id lcore id to specify affinity. @see pv_concurrent_core_id
 * @return 0 if success
 */
int pv_concurrent_run_worker(int worker_function(void*), void* arg, unsigned worker_id);

/**
 * Get current lcore id
 */
unsigned int pv_concurrent_core_id();

#ifdef __cplusplus
}
#endif
