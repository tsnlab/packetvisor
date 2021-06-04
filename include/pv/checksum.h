#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * RFC 791 IPv4 header checksum implementation.
 * Calculate the IPv4 header checksum at once.
 *
 * @param start start of a buffer which is to calculate the checksum.
 * @param size  size of the buffer in bytes.
 * @return calculated checksum.
 */
uint16_t pv_checksum(const void* start, uint32_t size);

/**
 * Calculate partial checksum. It can be used then the payload
 * is separated in many parts. You must call #pv_checksum_finalize function
 * at the end.
 *
 * @param start start of a buffer which is to calculate the checksum.
 * @param size  size of the buffer in bytes.
 * @return intermediate checksum.
 */
uint32_t pv_checksum_partial(const void* start, uint32_t size);

/**
 * Calcaulte final phase of checksum. It's the pair of #pv_checksum_partial function.
 * This function will calculate the checksum from intermediate value.
 *
 * @param checksum interpediate checksum which is calculated from #pv_checksum_partial.
 * @return calculated checksum.
 */
uint16_t pv_checksum_finalize(uint32_t checksum);

#ifdef __cplusplus
}
#endif
