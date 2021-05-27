#pragma once

#include <rte_mbuf.h>
#include <pv/packet.h>

struct pv_packet* _pv_mbuf_to_packet(struct rte_mbuf* mbuf, uint16_t nic_id, uint16_t queue_id);
