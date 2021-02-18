#ifndef __PV_NET_ARP_H__
#define __PV_NET_ARP_H__

struct pv_arp {
	uint16_t hw_type;
	uint16_t proto_type;
	uint8_t hw_size;
	uint8_t proto_size;
	uint16_t opcode;
};
#endif /* __PV_NET_ARP_H__ */
