#ifndef __PV_CORE_PV_H__
#define __PV_CORE_PV_H__

#include <rte_lcore.h>

#define PV_FOREACH_CORE(c) RTE_LCORE_FOREACH_WORKER((c))

typedef int (pv_core_function_t)(void*);

int pv_init(int argc, char** argv);

void pv_finalize(void);

int pv_start_core(unsigned int core_id, pv_core_function_t* func, void* context);

int pv_wait_core(unsigned int core_id);

void pv_debug();

#endif /* __PV_CORE_PV_H__ */
