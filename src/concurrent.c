#include <pv/concurrent.h>

void pv_concurrent_lock_init(struct pv_concurrent_lock* lock) {
    rte_rwlock_init(&lock->rte_lock);
}
void pv_concurrent_lock_read_lock(struct pv_concurrent_lock* lock) {
    rte_rwlock_read_lock(&lock->rte_lock);
}

void pv_concurrent_lock_read_unlock(struct pv_concurrent_lock* lock) {
    rte_rwlock_read_unlock(&lock->rte_lock);
}

void pv_concurrent_lock_write_lock(struct pv_concurrent_lock* lock) {
    rte_rwlock_write_lock(&lock->rte_lock);
}

void pv_concurrent_lock_write_unlock(struct pv_concurrent_lock* lock) {
    rte_rwlock_write_unlock(&lock->rte_lock);
}

int pv_concurrent_run_main(int main_function(void*), void* arg, bool call_main) {
    return rte_eal_mp_remote_launch(main_function, arg, call_main ? CALL_MAIN : SKIP_MAIN);
}

int pv_concurrent_run_worker(int worker_function(void*), void* arg, unsigned worker_id) {
    return rte_eal_remote_launch(worker_function, arg, worker_id);
}

unsigned int pv_concurrent_core_id() {
    return rte_lcore_id();
}
