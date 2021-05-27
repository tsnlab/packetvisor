#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pv/net/ipv4.h>

#define PV_UDP_HDR_LEN 8
#define PV_UDP_PAYLOAD(udp) (void*)((uint8_t*)(udp) + PV_UDP_HDR_LEN) // Get UDP data ptr

struct pv_udp {
    uint16_t srcport;
    uint16_t dstport;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed, scalar_storage_order("big-endian")));

void pv_udp_checksum_ipv4(struct pv_udp* udp, struct pv_ipv4 * ipv4);

#ifdef __cplusplus
}
#endif
