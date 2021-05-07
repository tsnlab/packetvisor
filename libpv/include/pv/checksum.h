#ifndef __PV_CHECKSUM_H__
#define __PV_CHECKSUM_H__

#include <stdint.h>

uint16_t checksum(const void* start, uint32_t size);
uint16_t checksum_partial(const void* start, uint32_t size);
uint16_t checksum_finalise(uint32_t checksum);

#endif // __PV_CHECKSUM_H__
