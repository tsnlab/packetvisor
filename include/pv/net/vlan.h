#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PV_VLAN_HDR_LEN (sizeof(struct pv_vlan))
#define PV_VLAN_PAYLOAD(vlan) (voild*)((uint8_t*)(vlan) + PV_VLAN_HDR_LEN)

struct pv_vlan {
    uint8_t priority : 3; // PCP
    uint8_t cfi : 1;      // DEI
    uint16_t id : 12;     // VID
    uint16_t type;        // ether type
} __attribute__((packed, scalar_storage_order("big-endian")));

#ifdef __cplusplus
}
#endif
