#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * ICMP(ICMPv4) header length is 4 bytes.
 *
 * @return 4
 */
#define PV_ICMP_HDR_LEN 4
/**
 * Get start pointer of payload from ICMP data structure.
 *
 * @param icmp  ICMP data structure.
 * @return start pointer of payload in void* type.
 */
#define PV_ICMP_PAYLOAD(icmp) (void*)((uint8_t*)(icmp) + PV_ICMP_HDR_LEN)

/**
 * ICMP type field values.
 */
enum PV_ICMP_TYPE {
    PV_ICMP_TYPE_ECHO_REPLY = 0,
    PV_ICMP_TYPE_ECHO_REQUEST = 8,
};

/**
 * ICMP data structure.
 */
struct pv_icmp {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
} __attribute__((packed, scalar_storage_order("big-endian")));

/**
 * Calcaulate ICMP header checksum. checksum field will be updated.
 *
 * @param icmp  ICMP data structure.
 * @param size  header + payload size in bytes.
 */
void pv_icmp_checksum(struct pv_icmp* icmp, uint16_t size);

#ifdef __cplusplus
}
#endif
