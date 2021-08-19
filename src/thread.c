#include <pv/thread.h>

#include <rte_launch.h>
#include <rte_rwlock.h>

void pv_thread_lock_init(struct pv_thread_lock* lock) {
    rte_rwlock_init((rte_rwlock_t*)&lock);
}
void pv_thread_lock_read_lock(struct pv_thread_lock* lock) {
    rte_rwlock_read_lock((rte_rwlock_t*)&lock);
}

void pv_thread_lock_read_unlock(struct pv_thread_lock* lock) {
    rte_rwlock_read_unlock((rte_rwlock_t*)&lock);
}

void pv_thread_lock_write_lock(struct pv_thread_lock* lock) {
    rte_rwlock_write_lock((rte_rwlock_t*)&lock);
}

void pv_thread_lock_write_unlock(struct pv_thread_lock* lock) {
    rte_rwlock_write_unlock((rte_rwlock_t*)&lock);
}

int pv_thread_run_main(int main_function(void*), void* arg, bool call_main) {
    return rte_eal_mp_remote_launch(main_function, arg, call_main ? CALL_MAIN : SKIP_MAIN);
}

int pv_thread_run_worker(int worker_function(void*), void* arg, unsigned worker_id) {
    return rte_eal_remote_launch(worker_function, arg, worker_id);
}

unsigned int pv_thread_core_id() {
    return rte_lcore_id();
}
