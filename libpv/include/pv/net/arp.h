#ifndef __PV_NET_ARP_H__
#define __PV_NET_ARP_H__

#define PV_ARP_HDR_LEN 28

struct pv_arp {
	uint16_t hw_type;		// Hardware type
	uint16_t proto_type;	// Protocol type
	uint8_t hw_size;		// Hardware address length
	uint8_t proto_size;		// Protocol address length
	uint16_t opcode;		// Operation
	uint64_t src_hw: 48;	// Sender hardware address
	uint32_t src_proto;		// Sender protocol address
	uint64_t dst_hw: 48;	// Target hardware address
	uint32_t dst_proto;		// Target protocol address
} __attribute__ ((packed, scalar_storage_order("big-endian")));

#endif /* __PV_NET_ARP_H__ */
