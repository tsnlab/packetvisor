#ifndef __PV_NIC_H__
#define __PV_NIC_H__

#include <stdint.h>
#include <stdbool.h>

#include <pv/net/pkt.h>

uint64_t pv_nic_mac_get();

bool pv_nic_is_promisc();

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
