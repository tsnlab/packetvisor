#include <pv/packet.h>

#include <rte_mbuf.h>

// private variable
struct rte_mempool* pv_mbuf_pool;

struct pv_packet* pv_mbuf_to_packet(struct rte_mbuf* mbuf, uint16_t nic_id) {
	struct pv_packet* packet = mbuf->buf_addr;
	packet->mbuf = mbuf;
	packet->buffer = mbuf->buf_addr;
	packet->buffer_len = mbuf->buf_len;
	packet->start = mbuf->data_off;
	packet->end = mbuf->data_off + mbuf->data_len;
	packet->nic_id = nic_id;

	return packet;
}

struct pv_packet* pv_packet_alloc() {
	struct rte_mbuf* mbuf = rte_pktmbuf_alloc(pv_mbuf_pool);
	if(mbuf == NULL) return NULL;

	return pv_mbuf_to_packet(mbuf, 0);
}

void pv_packet_free(struct pv_packet* packet) {
	rte_pktmbuf_free(packet->mbuf);
}
