#ifndef __PV_INTERNAL_OFFLOAD_H__
#define __PV_INTERNAL_OFFLOAD_H__

#include <pv/net/ethernet.h>
#include <pv/nic.h>

void rx_offload_vlan_strip(const struct pv_nic* nic, struct pv_packet* const packet, struct rte_mbuf* const mbuf);

/**
 * Filter based on VLAN id
 * @see pv_nic_vlan_filter_on
 * @see pv_nic_vlan_filter_off
 *
 * @return false if this packet need to be filtered out. else true
 */
bool rx_offload_vlan_filter(const struct pv_nic* nic, struct pv_packet* const packet);
void rx_offload_ipv4_checksum(const struct pv_nic* nic, struct pv_packet* const packet, struct rte_mbuf* const mbuf);

void tx_offload_vlan_insert(const struct pv_nic* nic, struct pv_packet* const packet);
void tx_offload_ipv4_checksum(const struct pv_nic* nic, struct pv_ethernet* const ether, struct rte_mbuf* const mbuf);

#endif
