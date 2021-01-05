#ifndef __PV_NIC_H__
#define __PV_NIC_H__

#include <stdint.h>
#include <stdbool.h>

//#include <rte_ethdev.h>

#include <pv/net/pkt.h>

// Map port id defined in user config and detected by dpdk
// nic_id_map[user_nic_id] == dpdk_port_id
static uint16_t nic_id_map[RTE_MAX_ETHPORTS];

//if(rte_eth_dev_is_valid_port(port_id) == false)
//    return false;
//
//struct rte_eth_dev_info dev_info;
//ret = rte_eth_dev_info_get(port_id, &dev_info);
//
//struct rte_eth_conf port_conf = { 
//    .rxmode = { 
//        .max_rx_pkt_len = RTE_ETHER_MAX_LEN,
//    },
//};
//ret = rte_eth_dev_start(port_id);

bool pv_nic_is_valid();
/**
 * Set nic id that has 'dev_name'. Set nic_id_map internally
 *
 * @return
 *   - True on success
 *   - False if there is no 'dev_name' in dev_list of dpdk.
 */
bool pv_nic_id_set_by_name(char* dev_name, uint16_t nic_id);

int pv_nic_rx_init(nic_id, nb_rx_queue, rx_queue_size, pkt_pool /*, offload */);
int pv_nic_tx_init(nic_id, nb_tx_queue, tx_queue_size /*, offload */);
uint64_t pv_nic_mac_get();
int pv_nic_mac_set(uint64_t mac);
bool pv_nic_is_promisc();
int pv_nic_promisc_set(nic_id, enable)
int pv_nic_start();
/**
 * Receive a packet from NIC
 *
 * @return
 *   - The pointer to the received packet on success.
 *   - NULL if receiving failed.
 */
struct pv_pkt* pv_nic_rx(uint16_t nic_id, uint16_t queue_id);

/**
 * Receive packets from NIC
 *
 * @return
 *   The number of packets received to rx_buf.
 */
uint16_t pv_nic_rx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_pkt** rx_buf, uint16_t rx_buf_size);

/**
 * Send a packet to NIC
 *
 * @return
 *   - True on success.
 *   - False if transmission failed.
 */
bool pv_nic_tx(uint16_t nic_id, uint16_t queue_id, struct pv_pkt* pkt);

/**
 * Send packets to NIC
 *
 * @return
 *   The number of packets transmitted to NIC's tx_ring.
 */
uint16_t pv_nic_tx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_pkt** pkts, uint16_t nb_pkts);


#endif /* __PV_NIC_H__ */
