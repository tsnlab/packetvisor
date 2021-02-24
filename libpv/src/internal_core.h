#ifndef __PV_INTERNAL_CORE_H__
#define __PV_INTERNAL_CORE_H__

#include <stdbool.h>

bool pv_core_init(uint32_t* cores, uint32_t cores_count);

void pv_core_finalize(void);

#endif /* __PV_INTERNAL_CORE_H__ */
