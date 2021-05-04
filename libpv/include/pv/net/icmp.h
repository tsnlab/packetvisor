#ifndef __PV_NET_ICMP_H__
#define __PV_NET_ICMP_H__

#define PV_ICMP_HDR_LEN 4
#define PV_ICMP_DATA(ICMP) ((uint8_t*)(ICMP) + PV_ICMP_HDR_LEN)

struct pv_icmp {
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
} __attribute__ ((packed, scalar_storage_order("big-endian")));

enum PV_ICMP_TYPE {
	PV_ICMP_TYPE_ECHO_REPLY = 0,
	PV_ICMP_TYPE_ECHO_REQUEST = 8,
};

#endif /* __PV_NET_ICMP_H__ */
