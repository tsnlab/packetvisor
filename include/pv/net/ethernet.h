#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * Ethernet header length is 14 bytes.
 */
#define PV_ETH_HDR_LEN (sizeof(struct pv_ethernet))
/**
 * Get start pointer of payload from Ethernet data structure.
 *
 * @param ethernet  ethernet data structure.
 * @return start pointer of payload in void* type.
 */
#define PV_ETH_PAYLOAD(ethernet) (void*)((uint8_t*)(ethernet) + PV_ETH_HDR_LEN)

/**
 * Ethernet types.
 */
enum pv_ethernet_type {
    PV_ETH_TYPE_IPv4 = 0x0800,
    PV_ETH_TYPE_ARP = 0x0806,
    PV_ETH_TYPE_IPv6 = 0x86DD,
    PV_ETH_TYPE_VLAN = 0x8100,
    PV_ETH_TYPE_PTP = 0x88F7,
};

/**
 * Ethernet data structure.
 */
struct pv_ethernet {
    uint64_t dmac : 48;
    uint64_t smac : 48;
    uint16_t type;
} __attribute__((packed, scalar_storage_order("big-endian")));

enum pv_ethernet_addr {
    PV_ETH_ADDR_MULTICAST = 0x011b19000000,
};

#ifdef __cplusplus
}
#endif
