#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pv/net/ip.h>

/**
 * Get IPv4 header length including header options in bytes.
 *
 * @param ipv4  IPv4 data structure.
 * @return IPv4 header length in bytes.
 */
#define PV_IPv4_HDR_LEN(ipv4) ((ipv4)->hdr_len * 4)
/**
 * Get start pointer of payload from IPv4 data structure.
 *
 * @param ipv4  IPv4 data structure.
 * @return start pointer of payload in void* type.
 */
#define PV_IPv4_PAYLOAD(ipv4) (void*)((uint8_t*)(ipv4) + PV_IPv4_HDR_LEN(ipv4)) // Get ip data pointer
/**
 * Get IPv4 payload length in bytes.
 *
 * @param ipv4  IPv4 data structure.
 * @return payload length in bytes.
 */
#define PV_IPv4_PAYLOAD_LEN(ipv4) ((ipv4)->len - PV_IPv4_HDR_LEN(ipv4)) // Get ip body length

/**
 * IPv4 data structure.
 */
struct pv_ipv4 {
    uint8_t version : 4;       // IP version, this is always equals to 4
    uint8_t hdr_len : 4;       // Internet Header Length
    uint8_t dscp : 6;          // Differentiated Services Code Point
    uint8_t ecn : 2;           // Explicit Congestion Notification
    uint16_t len;              // Total length, including header and data
    uint16_t id;               // Identification
    uint8_t flags : 3;         // bit0: Reserved, bit1: Dont' Fragment, bit2: More Fragment
    uint16_t frag_offset : 13; // Fragment Offset
    uint8_t ttl;               // Time To Live
    uint8_t proto;             // Protocol used in the data portion
    uint16_t checksum;         // IPv4 header checksum
    uint32_t src;              // Source IPv4 address
    uint32_t dst;              // Destination IPv4 address
    uint8_t opt[0];            // Options
} __attribute__((packed, scalar_storage_order("big-endian")));

/**
 * Calculate IPv4 header checksum. checksum field will be updated.
 *
 * @param ipv4  IPv4 data structure.
 */
void pv_ipv4_checksum(struct pv_ipv4* ipv4);

#ifdef __cplusplus
}
#endif
