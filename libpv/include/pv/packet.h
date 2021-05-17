#ifndef __PV_NET_PACKET_H__
#define __PV_NET_PACKET_H__

#include <stdint.h>
#include <stdbool.h>
#include <pv/net/vlan.h>

struct pv_packet {
    uint8_t* buffer;  // Usable space starting address
    uint32_t buffer_len;  // Total length of usable space
    uint32_t start;  // Offset from usable space: likely 128
    uint32_t end;  // Offset from usable space

    uint16_t nic_id;
    uint32_t ol_flags;
    struct {
        bool is_exists;
        struct pv_vlan_tci tci;
    } vlan;
    struct rte_mbuf* mbuf;
};

inline uint8_t* pv_packet_data_start(const struct pv_packet* packet) {
    return packet->buffer + packet->start;
}

inline uint8_t* pv_packet_data_end(const struct pv_packet* packet) {
    return packet->buffer + packet->end;
}

inline uint32_t pv_packet_data_len(const struct pv_packet* packet) {
    return packet->end - packet->start;
}

/**
 * Allocate a new packet.
 *
 * @param pkt_pool
 *   Preallocated packet pool. Use default packet pool if the param is NULL.
 *
 * @return
 *   - The pointer to the allocated packet on success.
 *   - NULL if allocation failed
 */
struct pv_packet* pv_packet_alloc();

/**
 * Free an allocated packet.
 *
 */
void pv_packet_free(struct pv_packet* packet);

bool pv_packet_add_head_paylen(struct pv_packet* packet, uint32_t len);

bool pv_packet_remove_head_paylen(struct pv_packet* packet, uint32_t len);

bool pv_packet_add_tail_paylen(struct pv_packet* packet, uint32_t len);

bool pv_packet_add_tail_paylen(struct pv_packet* packet, uint32_t len);

#endif /* __PV_NET_PACKET_H__ */
