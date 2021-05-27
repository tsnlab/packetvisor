#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define PV_ICMP_HDR_LEN 4
#define PV_ICMP_DATA(icmp) (void*)((uint8_t*)(icmp) + PV_ICMP_HDR_LEN)

enum PV_ICMP_TYPE {
    PV_ICMP_TYPE_ECHO_REPLY = 0,
    PV_ICMP_TYPE_ECHO_REQUEST = 8,
};

struct pv_icmp {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
} __attribute__((packed, scalar_storage_order("big-endian")));

void pv_icmp_checksum(struct pv_icmp * icmp, uint16_t size);

#ifdef __cplusplus
}
#endif
