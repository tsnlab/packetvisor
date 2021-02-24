#ifndef __PV_CORE_H__
#define __PV_CORE_H__

#include <stdint.h>

typedef int (pv_core_function_t)(void*);

uint32_t pv_core_count();

int pv_core_start(uint32_t core_id, pv_core_function_t* func, void* context);

int pv_core_wait(uint32_t core_id);

#endif /* __PV_CORE_H__ */
