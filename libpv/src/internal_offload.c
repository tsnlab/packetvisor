#include "internal_offload.h"

#include <rte_mbuf.h>
#include <rte_ethdev.h>

#include <pv/nic.h>
#include <pv/net/vlan.h>
#include <pv/net/ipv4.h>
#include <pv/checksum.h>
#include <pv/offload.h>


void rx_offload_vlan_strip(const struct pv_nic* nic, struct pv_packet* const packet, struct rte_mbuf* const mbuf) {
	struct pv_ethernet* ether = (struct pv_ethernet*) packet->payload;
	if (ether->type != PV_ETH_TYPE_VLAN) {
		return;
	}
	
	mbuf->ol_flags |= PKT_RX_VLAN | PKT_RX_VLAN_STRIPPED;
	
	struct pv_vlan* vlan = (struct pv_vlan*)PV_ETH_PAYLOAD(ether);
	
	mbuf->vlan_tci = vlan->tci;

	// MAGIC: PV_ETHER_HDR_LEN - sizeof(ether->type) -- vlan->etype becomes ether->type
	memmove(packet->payload + PV_VLAN_HDR_LEN, packet->payload, PV_ETH_HDR_LEN - sizeof(ether->type));
	
	mbuf->data_off += PV_VLAN_HDR_LEN;
	mbuf->data_len -= PV_VLAN_HDR_LEN;
	// XXX: Maybe pv_mbuf_to_packet?
	packet->payload += PV_VLAN_HDR_LEN;
	packet->payload_len -= PV_VLAN_HDR_LEN;
}

void rx_offload_ipv4_checksum(const struct pv_nic* nic, struct pv_packet* const packet, struct rte_mbuf* const mbuf) {
	struct pv_ethernet * const ether = (struct pv_ethernet *) packet->payload;
	
	if (ether->type != PV_ETH_TYPE_IPv4) {
		return;
	}
	
	if (pv_nic_is_rx_offload_supported(nic, DEV_RX_OFFLOAD_IPV4_CKSUM)) {
		uint32_t mask = mbuf->ol_flags & PKT_RX_IP_CKSUM_MASK;

		switch(mask) {
		case PKT_RX_IP_CKSUM_NONE:
			packet->ol_flags |= PV_PKT_RX_IP_CKSUM_NONE;
			break;
		case PKT_RX_IP_CKSUM_GOOD:
			packet->ol_flags |= PV_PKT_RX_IP_CKSUM_GOOD;
			break;
		case PKT_RX_IP_CKSUM_BAD:
			packet->ol_flags |= PV_PKT_RX_IP_CKSUM_BAD;
			break;
		case PKT_RX_IP_CKSUM_UNKNOWN:
			packet->ol_flags |= PV_PKT_RX_IP_CKSUM_UNKNOWN;
			break;
		}
	} else {
		struct pv_ipv4 * const ipv4 = (struct pv_ipv4 *)PV_ETH_PAYLOAD(ether);
		uint16_t pkt_checksum = ipv4->checksum;

		ipv4->checksum = 0;
		uint16_t calculated_checksum = checksum(ipv4, ipv4->hdr_len * 4);

		if (pkt_checksum == calculated_checksum) {
			packet->ol_flags |= PV_PKT_RX_IP_CKSUM_GOOD;
		} else {
			packet->ol_flags |= PV_PKT_RX_IP_CKSUM_BAD;
		}
		
		// Restore original checksum
		ipv4->checksum = pkt_checksum;
	}
}

void tx_offload_ipv4_checksum(const struct pv_nic* nic, struct pv_ethernet* const ether, struct rte_mbuf* const mbuf) {
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
