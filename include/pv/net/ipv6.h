#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pv/net/ip.h>

/**
 * Get IPv6 header length is 40 bytes.
 */
#define PV_IPv6_HDR_LEN 40
/**
 * Get start pointer of payload from IPv6 data structure.
 *
 * @param ipv6  IPv6 data structure.
 * @return start pointer of payload in void* type.
 */
#define PV_IPv6_PAYLOAD(ipv6) (void*)((uint8_t*)(ipv6) + PV_IPv6_HEADER_LENGTH)

/**
 * IPv6 data structure.
 */
struct pv_ipv6 {
    uint8_t version : 4; // Version. The constant 6 (bit sequence 0110)
    uint8_t tclass; // Traffic Class. 6 bits 'Differentiated Services field' + 2 bits 'Explicit Congestion Notification'
    uint32_t flow : 20; // Flow Label
    uint16_t plen;      // Payload Length
    uint8_t nxt;        // Next Header
    uint8_t hlim;       // Hop Limit
    uint8_t src[16];    // Source Address
    uint8_t dst[16];    // Destination Address
} __attribute__((packed, scalar_storage_order("big-endian")));

/**
 * Calculate IPv6 header checksum. checksum field will be updated.
 *
 * @param ipv6  IPv6 data structure.
 */
void pv_ipv6_checksum(struct pv_ipv6* ipv6);

#ifdef __cplusplus
}
#endif
