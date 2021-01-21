#include <stdio.h>

#include <pv/nic.h>
#include "internal_nic.h"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

extern struct rte_mempool* pv_mbuf_pool;
static uint16_t nic_id_map[RTE_MAX_ETHPORTS];

static bool pv_nic_get_dpdk_port_id(char* dev_name, uint16_t* port_id) {
	uint16_t id;

	RTE_ETH_FOREACH_DEV(id) {
		struct rte_eth_dev_info dev_info = {};
		if(rte_eth_dev_info_get(id, &dev_info) != 0)
			return false;

		if(strncmp(dev_info.device->name, dev_name, strlen(dev_info.device->name)) == 0) {
			*port_id = id;
			return true;
		}
	}

	return false;
}

int pv_nic_init(char* dev_name, uint16_t nic_id, uint16_t nb_rx_queue, uint16_t nb_tx_queue, 
		uint16_t rx_queue_size, uint16_t tx_queue_size, struct rte_mempool* mbuf_pool) {
	int ret = 0;

	pv_mbuf_pool = mbuf_pool;

	uint16_t dpdk_port_id = 0;
	if(pv_nic_get_dpdk_port_id(dev_name, &dpdk_port_id) == false) {
		printf("%s %d\n", __FILE__, __LINE__);
		return 1;
	}

	nic_id_map[nic_id] = dpdk_port_id;

	struct rte_eth_dev_info dev_info = {};
	ret = rte_eth_dev_info_get(dpdk_port_id, &dev_info);
	if(ret != 0) {
		printf("%s %d\n", __FILE__, __LINE__);
		return ret;
	}

	struct rte_eth_conf port_conf = {
		.rxmode = {
			.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
		},
	};

	if(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_IPV4_CKSUM) {
		port_conf.txmode.offloads |= DEV_TX_OFFLOAD_IPV4_CKSUM;
		printf("INFO: IPv4 cksum tx_offload is set\n");
	}

	ret = rte_eth_dev_configure(dpdk_port_id, nb_rx_queue, nb_tx_queue, &port_conf);
	if(ret != 0) {
		printf("%s %d\n", __FILE__, __LINE__);
		return ret;
	}

	for(uint16_t queue_id = 0; queue_id < nb_rx_queue; queue_id++) {
		ret = rte_eth_rx_queue_setup(dpdk_port_id, queue_id, rx_queue_size, SOCKET_ID_ANY, NULL, mbuf_pool);
		if(ret < 0) {
			printf("%s %d\n", __FILE__, __LINE__);
			return ret;
		}
	}

	struct rte_eth_txconf txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	ret = rte_eth_tx_queue_setup(dpdk_port_id, 0, tx_queue_size, SOCKET_ID_ANY, &txconf);
	if(ret < 0) {
		printf("%s %d\n", __FILE__, __LINE__);
		return ret;
	}

	return 0;
}


int pv_nic_set_mac(uint16_t nic_id, uint64_t mac) {
	return 0;
}

bool pv_nic_set_promisc(uint16_t nic_id, bool enable) {
	return true;
}

int pv_nic_start(uint16_t nic_id) {
	uint16_t dpdk_port_id = nic_id_map[nic_id];

	return rte_eth_dev_start(dpdk_port_id);
}

uint64_t pv_nic_get_mac(uint16_t nic_id) {
	uint16_t port_id = nic_id_map[nic_id];
	struct rte_ether_addr tmp_mac = {};
	rte_eth_macaddr_get(port_id, &tmp_mac);

	uint8_t mac[8] = {};
	mac[0] = tmp_mac.addr_bytes[5];
	mac[1] = tmp_mac.addr_bytes[4];
	mac[2] = tmp_mac.addr_bytes[3];
	mac[3] = tmp_mac.addr_bytes[2];
	mac[4] = tmp_mac.addr_bytes[1];
	mac[5] = tmp_mac.addr_bytes[0];

	return *(uint64_t*)mac;
}

bool pv_nic_is_promisc();

/**
 * Receive a packet from NIC
 *
 * @return
 *   - The pointer to the received packet on success.
 *   - NULL if receiving failed.
 */
struct pv_packet* pv_nic_rx(uint16_t nic_id, uint16_t queue_id) {
	return NULL;
}

/**
 * Receive packets from NIC
 *
 * @return
 *   The number of packets received to rx_buf.
 */
uint16_t pv_nic_rx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** rx_buf, uint16_t rx_buf_size) {
	uint16_t port_id = nic_id_map[nic_id];
	uint16_t nrecv = rte_eth_rx_burst(port_id, queue_id, (struct rte_mbuf**)rx_buf, rx_buf_size);
	if(nrecv == 0)
		return nrecv;

	for(uint16_t i = 0; i < nrecv; i++) {
		struct rte_mbuf* mbuf = (struct rte_mbuf*)rx_buf[i];
		struct pv_packet* packet = mbuf->buf_addr;
		packet->mbuf = mbuf;
		packet->payload = rte_pktmbuf_mtod(mbuf, void*);
		packet->nic_id = nic_id;

		rx_buf[i] = packet;
	}

	return nrecv;
}

/**
 * Send a packet to NIC
 *
 * @return
 *   - True on success.
 *   - False if transmission failed.
 */
bool pv_nic_tx(uint16_t nic_id, uint16_t queue_id, struct pv_packet* packet);

/**
 * Send packets to NIC
 *
 * @return
 *   The number of packets transmitted to NIC's tx_ring.
 */
uint16_t pv_nic_tx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet* pkts[], uint16_t nb_pkts) {
	uint16_t port_id = nic_id_map[nic_id];
	struct rte_mbuf** tx_buf = (struct rte_mbuf**)pkts;

	for(uint16_t i = 0; i < nb_pkts; i++) {
		tx_buf[i] = pkts[i]->mbuf;
		tx_buf[i]->ol_flags = (tx_buf[i]->ol_flags | PKT_TX_IPV4 | PKT_TX_IP_CKSUM);
	}

	return rte_eth_tx_burst(port_id, queue_id, tx_buf, nb_pkts);
}
