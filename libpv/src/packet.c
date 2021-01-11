#include <pv/packet.h>

#include <rte_mbuf.h>

// private variable
struct rte_mempool* pv_mbuf_pool;

struct pv_packet* pv_packet_alloc() {
	return NULL;
}

void pv_packet_free(struct pv_packet* pkt) {

}
