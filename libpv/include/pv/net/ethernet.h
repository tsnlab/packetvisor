#ifndef __PV_NET_ETHERNET_H__
#define __PV_NET_ETHERNET_H__

enum pv_ethernet_type {
	PV_ETHER_TYPE_IPv4 = 0x0800,
	PV_ETHER_TYPE_ARP  = 0x0806,
	PV_ETHER_TYPE_IPv6 = 0x86DD,
};

struct pv_ethernet {
	uint64_t dmac: 48;
	uint64_t smac: 48;
	uint16_t type;
	uint8_t payload[0];
} __attribute__ ((packed, scalar_storage_order("big-endian")));

#endif /* __PV_NET_ETHERNET_H__ */
