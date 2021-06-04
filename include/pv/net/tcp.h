#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * TCP header length is 20 bytes.
 */
#define PV_TCP_HDR_LEN 20
/**
 * Get start pointer of payload from TCP data structure.
 *
 * @param tcp   TCP data structure
 * @return start pointer of payload in void* type.
 */
#define PV_TCP_PAYLOAD(tcp) (void*)((uint8_t*)(tcp) + (tcp)->hdr_len * 4) // Get tcp data ptr

/**
 * TCP data structure.
 */
struct pv_tcp {
    uint16_t srcport;           // Source port
    uint16_t dstport;           // Destination port
    uint32_t seq;               // Sequence number
    uint32_t ack;               // Acknowledgement number
    uint8_t hdr_len : 4;        // Data offset
    uint8_t flags_res : 3;      // Reserved
    uint8_t flags_ns : 1;       // ECN-nonce - concealment protection
    uint8_t flags_cwr : 1;      // Congestion window reduced
    uint8_t flags_ecn : 1;      // ECN-Echo
    uint8_t flags_urg : 1;      // Urgent pointer
    uint8_t flags_ack : 1;      // Acknowledgment field is significant
    uint8_t flags_push : 1;     // Push function
    uint8_t flags_reset : 1;    // Reset the connection
    uint8_t flags_syn : 1;      // Synchronize sequence numbers
    uint8_t flags_fin : 1;      // Last packet from sender
    uint16_t window_size_value; // The size of the receive window
    uint16_t checksum;          // The 16-bit checksum field is used for error-checking
    uint16_t urgent_pointer;    // Urgent pointer
    uint8_t options[0];
} __attribute__((packed, scalar_storate_order("big-endian")));

#ifdef __cplusplus
}
#endif
