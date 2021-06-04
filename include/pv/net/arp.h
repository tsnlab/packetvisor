#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * ARP header length is 28 bytes.
 */
#define PV_ARP_HDR_LEN 28

/**
 * ARP opcode.
 */
enum pv_arp_opcode {
    PV_ARP_OPCODE_ARP_REQUEST = 1,
    PV_ARP_OPCODE_ARP_REPLY = 2,
    PV_ARP_OPCODE_RARP_REQUEST = 3,
    PV_ARP_OPCODE_RARP_REPLY = 4,
    PV_ARP_OPCODE_DRARP_REQUEST = 5,
    PV_ARP_OPCODE_DRARP_REPLY = 6,
    PV_ARP_OPCODE_DRARP_ERROR = 7,
    PV_ARP_OPCODE_INARP_REQUEST = 8,
    PV_ARP_OPCODE_INARP_REPLY = 9,
};

/**
 * ARP data structure.
 */
struct pv_arp {
    uint16_t hw_type;     // Hardware type
    uint16_t proto_type;  // Protocol type
    uint8_t hw_size;      // Hardware address length
    uint8_t proto_size;   // Protocol address length
    uint16_t opcode;      // Operation
    uint64_t src_hw : 48; // Sender hardware address
    uint32_t src_proto;   // Sender protocol address
    uint64_t dst_hw : 48; // Target hardware address
    uint32_t dst_proto;   // Target protocol address
} __attribute__((packed, scalar_storage_order("big-endian")));

#ifdef __cplusplus
}
#endif
