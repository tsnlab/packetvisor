#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint16_t pv_checksum(const void* start, uint32_t size);
uint16_t pv_checksum_partial(const void* start, uint32_t size);
uint16_t pv_checksum_finalise(uint32_t checksum);

#ifdef __cplusplus
}
#endif
