#include <pv/nic.h>
#include "internal_nic.h"

#include <rte_ethdev.h>

static uint16_t nic_id_map[RTE_MAX_ETHPORTS];

int pv_nic_init_info(char* dev_name, uint16_t nic_id, uint16_t nb_rx_queue, uint16_t nb_tx_queue, 
					uint16_t rx_queue_size, uint16_t tx_queue_size, struct rte_mempool* mbuf_pool) {
	int ret = 0;

	uint16_t dpdk_port_id = 0;
	if(pv_nic_get_dpdk_port_id(dev_name, &dpdk_port_id) == false)
		return 1;

	nic_id_map[nic_id] = dpdk_port_id;

	struct rte_eth_conf port_conf = {
		.rxmode = {
			.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
		},
	};
	ret = rte_eth_dev_configure(dpdk_port_id, nb_rx_queue, nb_tx_queue, &port_conf);
	if(ret != 0)
		return ret;

	for(uint16_t queue_id = 0; queue_id < nb_rx_queue, queue_id++) {
		ret = rte_eth_rx_queue_setup(dpdk_port_id, queue_id, rx_queue_size, SOCKET_ID_ANY, NULL, mbuf_pool);
		if(ret < 0)
			return ret;
	}

	struct rte_eth_txconf txconf = dev_info.default_txconf;
	return 0;
}

bool pv_nic_get_dpdk_port_id(char* dev_name, uint16_t* port_id) {
	uint16_t id;

	RTE_ETH_FOREACH_DEV(id) {
		struct rte_eth_dev_info dev_info = {};
		if(rte_eth_dev_info_get(id, &dev_info) != 0)
			return false;

		if(strncmp(strlen(dev_info.driver_name), dev_info.driver_name, dev_name) == 0) {
			*port_id = id;
			return true;
		}
	}

	return false;
}

int pv_nic_init_rx(uint16_t nic_id, uint16_t nb_rx_queue, uint16_t rx_queue_size) {
	uint16_t dpdk_port_id = nic_id_map[nic_id];
	
	uint16_t nb_rxd = 

}

int pv_nic_init_tx(uint16_t nic_id, uint16_t nb_tx_queue, uint16_t tx_queue_size) {
	uint16_t dpdk_port_id = nic_id_map[nic_id];
}

int pv_nic_set_mac(uint16_t nic_id, uint64_t mac) {
}

bool pv_nic_set_promisc(uint16_t nic_id, bool enable) {
}

int pv_nic_start(uint16_t nic_id) {
}
