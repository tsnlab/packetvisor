#ifndef __PV_NET_PACKET_H__
#define __PV_NET_PACKET_H__

#include <stdint.h>
#include <stdbool.h>
#include <pv/net/vlan.h>

struct pv_packet {
	uint8_t* payload;
	uint32_t payload_len;
	uint16_t nic_id;
	uint32_t ol_flags;
	struct {
		bool is_exists;
		struct pv_vlan_tci tci;
	} vlan;
	struct rte_mbuf* mbuf;
};

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
