#ifndef __PV_NET_PACKET_H__
#define __PV_NET_PACKET_H__

#include <stdint.h>
#include <stdbool.h>

struct pv_pkt {

};

struct pv_mempool {

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
struct pv_pkt* pv_pkt_alloc(struct pv_pkt_pool* pkt_pool);

/**
 * Free an allocated packet.
 *
 */
void pv_pkt_free(struct pv_pkt* pkt);

struct pv_mempool* pv_pkt_pool_create(const char* name, unsigned int nb_pkt /*, cache_size, priv_size, data_room_size, socket_id(?)*/);

#endif /* __PV_NET_PACKET_H__ */
