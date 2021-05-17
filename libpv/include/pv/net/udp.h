#ifndef __PV_NET_UDP_H__
#define __PV_NET_UDP_H__

#include <stdint.h>

#define PV_UDP_HDR_LEN	8

#define PV_UDP_PAYLOAD(UDP) ((uint8_t*)(UDP) + PV_UDP_HDR_LEN)	// Get UDP data ptr

struct pv_udp {
    uint16_t srcport;
    uint16_t dstport;
    uint16_t length;
    uint16_t checksum;
} __attribute__ ((packed, scalar_storage_order("big-endian")));

#endif /* __PV_NET_UDP_H__ */
