#include <stdio.h>

#include <pv/nic.h>
#include <pv/net/ethernet.h>
#include <pv/net/ipv4.h>
#include <pv/checksum.h>
#include "internal_nic.h"
#include "internal_offload.h"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

extern struct pv_offload_type rx_off_types[19];	// config.c
extern struct pv_offload_type tx_off_types[22];	// config.c
extern struct rte_mempool* pv_mbuf_pool;	// packet.c
extern struct pv_packet* pv_mbuf_to_packet(struct rte_mbuf* mbuf, uint16_t nic_id);	// packet.c

static struct pv_nic* nics;
static uint16_t nics_count;

#define VLAN_ID_SIZE (sizeof(uint16_t))


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

int pv_nic_init(uint16_t nic_count, struct rte_mempool* mbuf_pool) {
	pv_mbuf_pool = mbuf_pool;

	nics = calloc(nic_count, sizeof(struct pv_nic));
	if(nics == NULL)
		return 1;

	nics_count = nic_count;
	return 0;
}

void pv_nic_finalize(void) {
	for(int i = 1; i < nics_count; i += 1) {
		pv_list_destroy(nics[i].vlan_ids);
	}
	free(nics);
}

int pv_nic_add(uint16_t nic_id, char* dev_name, uint16_t nb_rx_queue, uint16_t nb_tx_queue, 
		uint16_t rx_queue_size, uint16_t tx_queue_size, uint32_t rx_offloads, uint32_t tx_offloads) {

	// get dpdk port id
	uint16_t dpdk_port_id = 0;
	if(pv_nic_get_dpdk_port_id(dev_name, &dpdk_port_id) == false) {
		printf("%s, Failed to get dpdk port id.\n", dev_name);
		printf("That NIC seems to be held by kernel.\n");
		printf("Please down your link and try again.\n");
		return 1;
	}

	nics[nic_id].dpdk_port_id = dpdk_port_id;

	// get mac
	struct rte_ether_addr tmp_mac = {};
	int ret = rte_eth_macaddr_get(dpdk_port_id, &tmp_mac);
	if(ret != 0) {
		printf("%s, Failed to get mac\n", dev_name);
		return ret;
	}
	nics[nic_id].mac_addr = 0;
	nics[nic_id].mac_addr |= (uint64_t)tmp_mac.addr_bytes[0];
	nics[nic_id].mac_addr |= (uint64_t)tmp_mac.addr_bytes[1] << 8;
	nics[nic_id].mac_addr |= (uint64_t)tmp_mac.addr_bytes[2] << 16;
	nics[nic_id].mac_addr |= (uint64_t)tmp_mac.addr_bytes[3] << 24;
	nics[nic_id].mac_addr |= (uint64_t)tmp_mac.addr_bytes[4] << 32;
	nics[nic_id].mac_addr |= (uint64_t)tmp_mac.addr_bytes[5] << 40;

	// get port info
	struct rte_eth_dev_info dev_info = {};
	ret = rte_eth_dev_info_get(dpdk_port_id, &dev_info);
	if(ret != 0) {
		printf("%s, Failed to get dpdk dev info\n", dev_name);
		return ret;
	}

	struct rte_eth_conf port_conf = {
		.rxmode = {
			.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
		},
	};

	// set rx offload
	port_conf.rxmode.offloads |= (dev_info.rx_offload_capa & rx_offloads);
	nics[nic_id].rx_offload_mask = rx_offloads;
	nics[nic_id].rx_offload_capa = dev_info.rx_offload_capa;

	// print debug msg
	uint32_t rx_incapa = rx_offloads & ~dev_info.rx_offload_capa;
	for(int i = 0; i < sizeof(rx_off_types) / sizeof(rx_off_types[0]); i++) {
		if(rx_incapa & rx_off_types[i].mask)
			printf("NIC doesn't support rx_offload: '%s'.\n", rx_off_types[i].name);
	}

	// set tx offload
	port_conf.txmode.offloads |= (dev_info.tx_offload_capa & tx_offloads);
	nics[nic_id].tx_offload_mask = tx_offloads;
	nics[nic_id].tx_offload_capa = dev_info.tx_offload_capa;

	// print debug msg
	uint32_t tx_incapa = tx_offloads & ~dev_info.tx_offload_capa;
	for(int i = 0; i < sizeof(tx_off_types) / sizeof(tx_off_types[0]); i++) {
		if(tx_incapa & tx_off_types[i].mask)
			printf("NIC doesn't support tx_offload: '%s'.\n", tx_off_types[i].name);
	}

	ret = rte_eth_dev_configure(dpdk_port_id, nb_rx_queue, nb_tx_queue, &port_conf);
	if(ret != 0) {
		printf("%s, Failed to configure dpdk dev\n", dev_name);
		return ret;
	}
	
	// rx queue setup
	struct rte_eth_rxconf rxconf = dev_info.default_rxconf;
	rxconf.offloads = port_conf.rxmode.offloads;
	for(uint16_t queue_id = 0; queue_id < nb_rx_queue; queue_id++) {
		ret = rte_eth_rx_queue_setup(dpdk_port_id, queue_id, rx_queue_size, SOCKET_ID_ANY, &rxconf, pv_mbuf_pool);
		if(ret < 0) {
			printf("%s, Failed to setup dpdk rx_queue\n", dev_name);
			return ret;
		}
	}

	// tx queue setup
	struct rte_eth_txconf txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	ret = rte_eth_tx_queue_setup(dpdk_port_id, 0, tx_queue_size, SOCKET_ID_ANY, &txconf);
	if(ret < 0) {
		printf("%s, Failed to setup dpdk tx_queue\n", dev_name);
		return ret;
	}

	// Create vlan ids
	nics[nic_id].vlan_ids = pv_list_create(VLAN_ID_SIZE, 0);

	return 0;
}

bool pv_nic_get_promisc(uint16_t nic_id, bool* is_promisc) {
	if(nic_id > nics_count)
		return false;

	int ret = rte_eth_promiscuous_get(nics[nic_id].dpdk_port_id);
	if(ret == -1)
		return false;

	*is_promisc = ret ? true : false;
	return true;
}

bool pv_nic_set_promisc(uint16_t nic_id, bool enable) {
	if(nic_id > nics_count)
		return false;

	int ret = 0;
	if(enable)
		ret = rte_eth_promiscuous_enable(nics[nic_id].dpdk_port_id);
	else
		ret = rte_eth_promiscuous_disable(nics[nic_id].dpdk_port_id);

	return ret == 0 ? true : false;
}

bool pv_nic_get_mac(uint16_t nic_id, uint64_t* mac_addr) {
	if(nic_id > nics_count)
		return false;

	*mac_addr = nics[nic_id].mac_addr;
	return true;
}

bool pv_nic_set_mac(uint16_t nic_id, uint64_t mac_addr) {
	if(nic_id > nics_count)
		return false;

	nics[nic_id].mac_addr = mac_addr;
	return true;
}

bool pv_nic_get_ipv4(uint16_t nic_id, uint32_t* ipv4_addr) {
	if(nic_id > nics_count)
		return false;
	
	*ipv4_addr = nics[nic_id].ipv4_addr;
	return true;
}

bool pv_nic_set_ipv4(uint16_t nic_id, uint32_t ipv4_addr) {
	if(nic_id > nics_count)
		return false;

	nics[nic_id].ipv4_addr = ipv4_addr;
	return true;
}

int pv_nic_start(uint16_t nic_id) {
	return rte_eth_dev_start(nics[nic_id].dpdk_port_id);
}

uint16_t pv_nic_avail_count() {
	return nics_count;
}

/**
 * Receive a packet from NIC
 *
 * @return
 *   - The pointer to the received packet on success.
 *   - NULL if receiving failed.
 */
struct pv_packet* pv_nic_rx(uint16_t nic_id, uint16_t queue_id) {
	struct pv_packet* rx_buf[1] = {};
	pv_nic_rx_burst(nic_id, queue_id, rx_buf, 1);
	return rx_buf[0];
}

/**
 * Receive packets from NIC
 *
 * @return
 *   The number of packets received to rx_buf.
 */
uint16_t pv_nic_rx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** rx_buf, uint16_t rx_buf_size) {
	uint16_t port_id = nics[nic_id].dpdk_port_id;
	struct rte_mbuf* mbufs;
	uint16_t nrecv = rte_eth_rx_burst(port_id, queue_id, &mbufs, rx_buf_size);
	if(nrecv == 0)
		return nrecv;

	*rx_buf = (struct pv_packet*)mbufs;
	for(uint16_t i = 0; i < nrecv; i += 1) {
			rx_buf[i] = pv_mbuf_to_packet(&mbufs[i], nic_id);
	}

	// Filter first, and process
	if (pv_nic_is_rx_offload_enabled(&nics[nic_id], DEV_RX_OFFLOAD_VLAN_FILTER) &&
			!pv_nic_is_rx_offload_supported(&nics[nic_id], DEV_RX_OFFLOAD_VLAN_FILTER)) {
		uint16_t w = 0;
		for(uint16_t r = 0; r < nrecv; r += 1) {
			if(!rx_offload_vlan_filter(&nics[nic_id], rx_buf[r])) {
				rte_pktmbuf_free(rx_buf[r]->mbuf);
			} else {
				rx_buf[w] = rx_buf[r];
				w += 1;
			}
		}
		nrecv = w;
	}

	for(uint16_t i = 0; i < nrecv; i++) {
		struct rte_mbuf* mbuf = rx_buf[i]->mbuf;

		if(pv_nic_is_rx_offload_enabled(&nics[nic_id], DEV_RX_OFFLOAD_IPV4_CKSUM)) {
			rx_offload_ipv4_checksum(&nics[nic_id], rx_buf[i], mbuf);
		}
		
		if(pv_nic_is_rx_offload_enabled(&nics[nic_id], DEV_RX_OFFLOAD_VLAN_STRIP)) {
			rx_offload_vlan_strip(&nics[nic_id], rx_buf[i], mbuf);
		}
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
bool pv_nic_tx(uint16_t nic_id, uint16_t queue_id, struct pv_packet* packet) {
	struct pv_packet* tx_buf[1] = {packet};
	return pv_nic_tx_burst(nic_id, queue_id, tx_buf, 1) == 1 ? true : false;
}

/**
 * Send packets to NIC
 *
 * @return
 *   The number of packets transmitted to NIC's tx_ring.
 */
uint16_t pv_nic_tx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet* pkts[], uint16_t nb_pkts) {
	uint16_t port_id = nics[nic_id].dpdk_port_id;
	struct rte_mbuf** tx_buf = (struct rte_mbuf**)pkts;

	for(uint16_t i = 0; i < nb_pkts; i++) {
		// Must be before set tx_buf[i].
		struct pv_packet* packet = pkts[i];
		struct pv_ethernet* ether = (struct pv_ethernet *)pkts[i]->payload;

		tx_buf[i] = pkts[i]->mbuf;

		// l3 checksum offload.
		if(ether->type == PV_ETH_TYPE_IPv4 && pv_nic_is_tx_offload_enabled(&nics[nic_id], DEV_TX_OFFLOAD_IPV4_CKSUM)) {
			tx_offload_ipv4_checksum(&nics[nic_id], ether, packet->mbuf);
		}
		
		if(pv_nic_is_tx_offload_enabled(&nics[nic_id], DEV_TX_OFFLOAD_VLAN_INSERT)) {
			tx_offload_vlan_insert(&nics[nic_id], packet);
		}
	}

	return rte_eth_tx_burst(port_id, queue_id, tx_buf, nb_pkts);
}

bool pv_nic_vlan_filter_on(uint16_t nic_id, uint16_t id) {
	pv_list_append(nics[nic_id].vlan_ids, &id);
	int res = rte_eth_dev_vlan_filter(nics[nic_id].dpdk_port_id, id, 1);
	return res == 0;
}

bool pv_nic_vlan_filter_off(uint16_t nic_id, uint16_t id) {
	struct pv_list* const vlan_ids = nics[nic_id].vlan_ids;
	for(int i = vlan_ids->current; i >= 0; i -= 1) {
		uint16_t id_ = *(uint16_t*)pv_list_get(vlan_ids, i);
		if(id_ == id) {
			pv_list_del(vlan_ids, i);
		}
	}
	int res = rte_eth_dev_vlan_filter(nics[nic_id].dpdk_port_id, id, 0);
	return res == 0;
}
