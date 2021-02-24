#include <pv/packet.h>

#include <rte_mbuf.h>

// private variable
struct rte_mempool* pv_mbuf_pool;

struct pv_packet* pv_mbuf_to_packet(struct rte_mbuf* mbuf, uint16_t nic_id) {
	struct pv_packet* packet = mbuf->buf_addr;
	packet->mbuf = mbuf;
	packet->payload = rte_pktmbuf_mtod(mbuf, void*);
	packet->payload_len = rte_pktmbuf_data_len(mbuf);
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

bool pv_packet_add_head_paylen(struct pv_packet* packet, uint32_t len) {
	if(len + sizeof(struct pv_packet) > rte_pktmbuf_headroom(packet->mbuf))
		return false;

	packet->payload = (uint8_t*)rte_pktmbuf_prepend(packet->mbuf, len);
	packet->payload_len = rte_pktmbuf_data_len(packet->mbuf);
	return true;
}

bool pv_packet_remove_head_paylen(struct pv_packet* packet, uint32_t len) {
	void* new_payload = rte_pktmbuf_adj(packet->mbuf, len);
	if(new_payload == NULL)
		return false;
	
	packet->payload = new_payload;
	packet->payload_len = rte_pktmbuf_data_len(packet->mbuf);
	return true;
}

bool pv_packet_add_tail_paylen(struct pv_packet* packet, uint32_t len) {
	if(rte_pktmbuf_append(packet->mbuf, len) == NULL)
		return false;
	
	packet->payload_len = rte_pktmbuf_data_len(packet->mbuf);
	return true;
}

bool pv_packet_remove_tail_paylen(struct pv_packet* packet, uint32_t len) {
	if(rte_pktmbuf_trim(packet->mbuf, len) == -1)
		return false;
	
	packet->payload_len = rte_pktmbuf_data_len(packet->mbuf);
	return false;
}

bool pv_packet_set_payload_len(struct pv_packet* packet, uint32_t len) {
	if(rte_pktmbuf_append(packet->mbuf, len) == NULL)
		return false;

	packet->payload_len = len;
	return true;
}
