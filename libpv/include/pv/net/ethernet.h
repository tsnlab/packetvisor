#ifndef __PV_NET_ETHERNET_H__
#define __PV_NET_ETHERNET_H__

#include <stdint.h>

#define PV_ETH_HDR_LEN (sizeof(struct pv_ethernet))

#define PV_ETH_PAYLOAD(ETH) ((uint8_t*)(ETH) + PV_ETH_HDR_LEN)

enum pv_ethernet_type {
	PV_ETH_TYPE_IPv4 = 0x0800,
	PV_ETH_TYPE_ARP  = 0x0806,
	PV_ETH_TYPE_IPv6 = 0x86DD,
	PV_ETH_TYPE_VLAN = 0x8100,
};

struct pv_ethernet {
	uint64_t dmac: 48;
	uint64_t smac: 48;
	uint16_t type;
} __attribute__ ((packed, scalar_storage_order("big-endian")));

#endif /* __PV_NET_ETHERNET_H__ */
