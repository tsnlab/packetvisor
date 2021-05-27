#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PV_ETH_HDR_LEN (sizeof(struct pv_ethernet))
#define PV_ETH_PAYLOAD(ethernet) (void*)((uint8_t*)(ethernet) + PV_ETH_HDR_LEN)

enum pv_ethernet_type {
    PV_ETH_TYPE_IPv4 = 0x0800,
    PV_ETH_TYPE_ARP = 0x0806,
    PV_ETH_TYPE_IPv6 = 0x86DD,
    PV_ETH_TYPE_VLAN = 0x8100,
};

struct pv_ethernet {
    uint64_t dmac : 48;
    uint64_t smac : 48;
    uint16_t type;
} __attribute__((packed, scalar_storage_order("big-endian")));

#ifdef __cplusplus
}
#endif
