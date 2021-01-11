#ifndef __PV_NET_PACKET_H__
#define __PV_NET_PACKET_H__

#include <stdint.h>
#include <stdbool.h>

struct pv_packet {
	uint8_t* payload;
	uint16_t nic_id;
	struct rte_mbuf* mbuf;
};

/**
 * Allocate a new packet.
 *
 * @param pkt_pool
 *   Preallocated packet pool. Use default packet pool if the param is NULL.
 *
 * @return
 *   - The pointer to the allocated packet on succes.
 *   - NULL if allocation failed
 */
struct pv_packet* pv_packet_alloc();

/**
 * Free an allocated packet.
 *
 */
void pv_packet_free(struct pv_packet* packet);

#endif /* __PV_NET_PACKET_H__ */
