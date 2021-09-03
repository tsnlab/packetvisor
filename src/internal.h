#pragma once

#include <rte_mbuf.h>
#include <pv/packet.h>

struct pv_packet* _pv_mbuf_to_packet(struct rte_mbuf* mbuf, uint16_t nic_id, uint16_t queue_id);
struct rte_mbuf* _pv_packet_to_mbuf(struct pv_packet* pkt, uint16_t nic_id, uint16_t queue_id);
void pv_packet_set_mbuf_pool(struct rte_mempool* pool);
