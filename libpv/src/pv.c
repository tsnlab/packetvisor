#include <stdio.h>

#include <pv/pv.h>
#include <pv/packet.h>
#include "internal_nic.h"

#include <rte_eal.h>

#define NUM_MBUFS 2048

int pv_init(int argc, char** argv) {
	printf("in %d\n", __LINE__);

	int ret = rte_eal_init(argc, argv);
	if(ret < 0)
		return ret;
	printf("in %d\n", __LINE__);

// static var start
    char* dev_name = "0000:01:00.1";
    uint16_t nic_id = 0;
    uint16_t nb_rx_queue = 1;
    uint16_t nb_tx_queue = 1;
    uint16_t rx_queue_size = 64;
    uint16_t tx_queue_size = 64;
    
	uint16_t priv_size = sizeof(struct pv_packet);
	priv_size = RTE_ALIGN(priv_size, RTE_MBUF_PRIV_ALIGN);
	struct rte_mempool* mbuf_pool = rte_pktmbuf_pool_create("PV", NUM_MBUFS * 1, 250, priv_size, RTE_MBUF_DEFAULT_BUF_SIZE, SOCKET_ID_ANY);
	if(mbuf_pool == NULL) {
		printf("mempool is NULL\n");
		return 1;
	}

// static var end
	printf("in %d\n", __LINE__);

	ret = pv_nic_init(dev_name, nic_id, nb_rx_queue, nb_tx_queue, rx_queue_size, tx_queue_size, mbuf_pool);
	if(ret != 0)
	    return ret;

	printf("in %d\n", __LINE__);
	ret = pv_nic_start(nic_id);
	if(ret != 0)
	    return ret;
	printf("in %d\n", __LINE__);

	return 0;

}

void pv_finalize(void) {

}

int pv_start_core(unsigned int core_id, pv_core_function_t* func, void* context) {
	return rte_eal_remote_launch(func, context, core_id);
}

int pv_wiat_core(unsigned int core_id) {
	return rte_eal_wait_lcore(core_id);
}
