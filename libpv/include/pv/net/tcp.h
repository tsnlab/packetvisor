#ifndef __PV_NET_TCP_H__
#define __PV_NET_TCP_H__

#include <stdint.h>

#define PV_TCP_HDR_LEN	20

#define PV_TCP_DATA(TCP) ((uint8_t*)(TCP) + (TCP)->hdr_len * 4)	// Get tcp data ptr

struct pv_tcp {
    uint16_t srcport;			// Source port 
    uint16_t dstport;			// Destination port
    uint32_t seq;				// Sequence number
    uint32_t ack;				// Acknowledgement number
    uint8_t	hdr_len: 4;			// Data offset
    uint8_t flags_res: 3;		// Reserved
    uint8_t flags_ns: 1;		// ECN-nonce - concealment protection
    uint8_t flags_cwr: 1;		// Congestion window reduced
    uint8_t flags_ecn: 1;		// ECN-Echo
    uint8_t flags_urg: 1;		// Urgent pointer
    uint8_t flags_ack: 1;		// Acknowledgment field is significant
    uint8_t flags_push: 1;		// Push function
    uint8_t flags_reset: 1;		// Reset the connection
    uint8_t flags_syn: 1;		// Synchronize sequence numbers
    uint8_t flags_fin: 1;		// Last packet from sender
    uint16_t window_size_value;	// The size of the receive window
    uint16_t checksum;			// The 16-bit checksum field is used for error-checking
    uint16_t urgent_pointer;	// Urgent pointer
    uint8_t options[0];
} __attribute__ ((packed, scalar_storate_order("big-endian")));

#endif /* __PV_NET_TCP_H__ */
