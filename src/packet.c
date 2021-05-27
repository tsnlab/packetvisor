#include "internal.h"

static struct rte_mempool* _mbuf_pool;

struct pv_packet* _pv_mbuf_to_packet(struct rte_mbuf* mbuf, uint16_t nic_id, uint16_t queue_id) {
    struct pv_packet* packet = mbuf->buf_addr;
    packet->start = mbuf->data_off - sizeof(struct pv_packet);
    packet->end = mbuf->data_off + mbuf->data_len - sizeof(struct pv_packet);
    packet->size = mbuf->buf_len - sizeof(struct pv_packet);
    packet->buffer = mbuf->buf_addr + sizeof(struct pv_packet);
    packet->priv = mbuf;

    return packet;
}

struct pv_packet* pv_packet_alloc() {
    struct rte_mbuf* mbuf = rte_pktmbuf_alloc(_mbuf_pool);
    if(mbuf == NULL)
        return NULL;

    return _pv_mbuf_to_packet(mbuf, 0, 0);
}

void pv_packet_free(struct pv_packet* packet) {
    rte_pktmbuf_free(packet->priv);
}
