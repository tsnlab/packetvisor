#include <stdio.h>

#include <pv/nic.h>
#include <pv/net/ethernet.h>
#include <pv/net/ipv4.h>
#include <pv/checksum.h>
#include "internal_nic.h"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>

extern struct pv_offload_type rx_off_types[19];	// config.c
extern struct pv_offload_type tx_off_types[22];	// config.c
extern struct rte_mempool* pv_mbuf_pool;	// packet.c
extern struct pv_packet* pv_mbuf_to_packet(struct rte_mbuf* mbuf, uint16_t nic_id);	// packet.c

static struct pv_nic* nics;
static uint16_t nics_count;

void offload_ipv4_checksum(const struct pv_nic* nic, struct pv_ethernet* const ether, struct rte_mbuf* const mbuf);


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
	uint32_t rx_incapa = (dev_info.rx_offload_capa & rx_offloads) ^ rx_offloads;
	for(int i = 0; i < sizeof(rx_off_types) / sizeof(rx_off_types[0]); i++) {
		if(rx_incapa & rx_off_types[i].mask)
			printf("NIC doesn't support rx_offload: '%s'.\n", rx_off_types[i].name);
	}

	// set tx offload
	port_conf.txmode.offloads |= (dev_info.tx_offload_capa & tx_offloads);
	nics[nic_id].tx_offload_mask = tx_offloads;
	nics[nic_id].tx_offload_capa = dev_info.tx_offload_capa;

	// print debug msg
	uint32_t tx_incapa = (dev_info.tx_offload_capa & tx_offloads) ^ tx_offloads;
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
	uint16_t nrecv = rte_eth_rx_burst(port_id, queue_id, (struct rte_mbuf**)rx_buf, rx_buf_size);
	if(nrecv == 0)
		return nrecv;

	for(uint16_t i = 0; i < nrecv; i++) {
		struct rte_mbuf* mbuf = (struct rte_mbuf*)rx_buf[i];
		rx_buf[i] = pv_mbuf_to_packet(mbuf, nic_id);
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
		struct pv_ethernet* ether = (struct pv_ethernet *)pkts[i]->payload;

		tx_buf[i] = pkts[i]->mbuf;

		// l3 checksum offload.
		if(ether->type == PV_ETH_TYPE_IPv4 && pv_nic_is_tx_offload_enabled(&nics[nic_id], DEV_TX_OFFLOAD_IPV4_CKSUM)) {
			offload_ipv4_checksum(&nics[nic_id], ether, tx_buf[i]);
		}
	}

	return rte_eth_tx_burst(port_id, queue_id, tx_buf, nb_pkts);
}


bool inline pv_nic_is_tx_offload_enabled(const struct pv_nic* nic, uint32_t feature) {
	return nic->tx_offload_mask & feature;
}

bool inline pv_nic_is_tx_offload_supported(const struct pv_nic* nic, uint32_t feature) {
	return nic->tx_offload_capa & feature;
}

void offload_ipv4_checksum(const struct pv_nic* nic, struct pv_ethernet* const ether, struct rte_mbuf* const mbuf) {
	struct pv_ipv4 * const ipv4 = (struct pv_ipv4 *)PV_ETH_PAYLOAD(ether);

	if(pv_nic_is_tx_offload_supported(nic, DEV_TX_OFFLOAD_IPV4_CKSUM)) {
		mbuf->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM;
		mbuf->l2_len = sizeof(struct pv_ethernet);
		mbuf->l3_len = ipv4->hdr_len * 4;
	} else {
		ipv4->checksum = 0;
		ipv4->checksum = checksum(ipv4, ipv4->hdr_len * 4);
	}
}
