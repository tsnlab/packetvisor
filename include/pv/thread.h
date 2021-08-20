#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

struct pv_thread_lock {
    volatile int32_t cnt;
};

enum pv_thread_state {
    PV_THREAD_WAIT,
    PV_THREAD_RUNNING,
    PV_THREAD_FINISHED,
};

/**
 * Init pv_thread_lock struct before use
 */
void pv_thread_lock_init(struct pv_thread_lock* lock);

/**
 * Set read lock before use
 */
void pv_thread_lock_read_lock(struct pv_thread_lock* lock);

/**
 * Unset read lock after use
 */
void pv_thread_lock_read_unlock(struct pv_thread_lock* lock);

/**
 * Set write lock before use
 */
void pv_thread_lock_write_lock(struct pv_thread_lock* lock);

/**
 * Unset write lock after use
 */
void pv_thread_lock_write_unlock(struct pv_thread_lock* lock);

/**
 * Run main loop on all core
 * All cores need to be PV_THREAD_WAIT state.
 * Can join thread with @see pv_thread_wait_all
 * @param main_function main function to run
 * @param arg custom arguments to be passed on main_function
 * @param call_main If true, Also called on main lcore. else run on non-main cores
 * @return main_function's exit code
 */
int pv_thread_run_all(int main_function(void*), void* arg, bool call_main);

/**
 * Wait all cores to finish their jobs.
 * After call this function. You can assume that all cores are in WAIT state.
 */
void pv_thread_wait_all();

/**
 * Run worker loop on specific core
 * Core need to be PV_THREAD_WAIT state.
 * Can join thread with @see pv_thread_wait_core
 * @param worker_function worker function to run
 * @param arg custom arguments to be passed on worker_function
 * @param worker_id lcore id to specify affinity. @see pv_thread_core_id
 * @return 0 if success
 */
int pv_thread_run_at(int worker_function(void*), void* arg, unsigned lcore_id);

/**
 * Join worker loop thread and make that core in WAIT state.
 * @param lcore_id to wait for
 * @return 0 if already in PV_THREAD_WAIT status, else return that process' return value
 */
int pv_thread_wait_core(unsigned lcore_id);

/**
 * Get specific core's current state.
 */
enum pv_thread_state pv_thread_get_core_state(unsigned lcore_id);

/**
 * Get current lcore id
 * Useful if run same function on different cores.
 * @see pv_thread_run_all, @see pv_thread_run_at.
 */
unsigned int pv_thread_core_id();

#ifdef __cplusplus
}
#endif
