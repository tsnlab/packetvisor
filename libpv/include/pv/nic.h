#ifndef __PV_NIC_H__
#define __PV_NIC_H__

#include <stdint.h>
#include <stdbool.h>

#include <pv/packet.h>

struct pv_nic {
	uint16_t dpdk_port_id;

	uint64_t mac_addr;
	uint32_t ipv4_addr;

	uint32_t rx_offload_mask;
	uint32_t tx_offload_mask;
	
	uint32_t rx_offload_capa;
	uint32_t tx_offload_capa;
};

bool pv_nic_get_mac(uint16_t nic_id, uint64_t* mac_addr);

bool pv_nic_get_ipv4(uint16_t nic_id, uint32_t* ipv4_addr);

bool pv_nic_get_promisc(uint16_t nic_id, bool* is_promisc);

/**
 * Return the number of available nic
*/
uint16_t pv_nic_get_avail_count();

/**
 * Receive a packet from NIC
 *
 * @return
 *   - The pointer to the received packet on success.
 *   - NULL if receiving failed.
 */
struct pv_packet* pv_nic_rx(uint16_t nic_id, uint16_t queue_id);

/**
 * Receive packets from NIC
 *
 * @return
 *   The number of packets received to rx_buf.
 */
uint16_t pv_nic_rx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** rx_buf, uint16_t rx_buf_size);

/**
 * Send a packet to NIC
 *
 * @return
 *   - True on success.
 *   - False if transmission failed.
 */
bool pv_nic_tx(uint16_t nic_id, uint16_t queue_id, struct pv_packet* pkt);

/**
 * Send packets to NIC
 *
 * @return
 *   The number of packets transmitted to NIC's tx_ring.
 */
uint16_t pv_nic_tx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** pkts, uint16_t nb_pkts);

/**
 * Check tx offload is enabled on config.
 * 
 * @return
 *   true if enabled
 */
bool inline pv_tx_offload_enabled(struct pv_nic nic, uint32_t feature);

/**
 * Check tx offload is supported by hardware.
 * 
 * @return
 *   true if supported by hardware
 */
bool inline pv_tx_offload_supported(struct pv_nic nic, uint32_t feature);


#endif /* __PV_NIC_H__ */
