#ifndef __PV_NET_IPv4_H__
#define __PV_NET_IPv4_H__

#include <pv/net/ip.h>

#define PV_IPv4_HDR_LEN 20

#define PV_IPv4_DATA(IP) ((uint8_t*)(IP) + (IP)->hdr_len * 4)	// Get ip data ptr
#define PV_IPv4_DATA_LEN(IP) ((IP)->len - (IP)->hdr_len * 4)	// Get ip body len

struct pv_ipv4 {
	uint8_t version: 4;			// IP version, this is always equals to 4
	uint8_t hdr_len: 4;			// Internet Header Length
	uint8_t dscp: 6;			// Differentiated Services Code Point
	uint8_t ecn: 2;				// Explicit Congestion Notification
	uint16_t len;				// Total length, including header and data
	uint16_t id;				// Identification
	uint8_t flags: 3;			// bit0: Reserved, bit1: Dont' Fragment, bit2: More Fragment
	uint16_t frag_offset: 13;	// Fragment Offset
	uint8_t ttl;				// Time To Live
	uint8_t proto;				// Protocol used in the data portion
	uint16_t checksum;			// IPv4 header checksum
	uint32_t src;				// Source IPv4 address
	uint32_t dst;				// Destination IPv4 address
	uint8_t opt[0];				// Options

} __attribute__ ((packed, scalar_storage_order("big-endian")));

#endif /* __PV_NET_IPv4_H__ */
