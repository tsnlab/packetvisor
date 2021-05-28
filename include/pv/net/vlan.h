#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * VLAN header is is actually 2 bytes but here we return 4 bytes including ether type.
 */
#define PV_VLAN_HDR_LEN (sizeof(struct pv_vlan))
/**
 * Get start pointer of payload from VLAN data structure.
 *
 * @param vlan  VLAN data structure.
 * @return start pointer of payload in void* type.
 */
#define PV_VLAN_PAYLOAD(vlan) (voild*)((uint8_t*)(vlan) + PV_VLAN_HDR_LEN)

/**
 * VLAN data structure.
 */
struct pv_vlan {
    uint8_t priority : 3; // PCP
    uint8_t cfi : 1;      // DEI
    uint16_t id : 12;     // VID
    uint16_t type;        // ether type
} __attribute__((packed, scalar_storage_order("big-endian")));

#ifdef __cplusplus
}
#endif
