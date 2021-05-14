#include "internal_offload.h"

#include <rte_mbuf.h>
#include <rte_ethdev.h>

#include <pv/nic.h>
#include <pv/net/vlan.h>
#include <pv/net/ipv4.h>
#include <pv/checksum.h>
#include <pv/offload.h>


void rx_offload_vlan_strip(const struct pv_nic* nic, struct pv_packet* const packet) {
	
	if(pv_nic_is_rx_offload_supported(nic, DEV_RX_OFFLOAD_VLAN_STRIP)) {
		struct rte_mbuf* const mbuf = packet->mbuf;
		if (mbuf->ol_flags & PKT_RX_VLAN) {
			packet->ol_flags |= PV_PKT_RX_VLAN;
			packet->vlan.is_exists = true;

			packet->vlan.tci = pv_vlan_uint16_to_tci(mbuf->vlan_tci);

			if (mbuf->ol_flags & PKT_RX_VLAN_STRIPPED)
			{
				packet->ol_flags |= PV_PKT_RX_VLAN_STRIPPED;
			}
		} else {
			packet->vlan.is_exists = false;
		}
	} else {
		struct pv_ethernet* ether = (struct pv_ethernet*) pv_packet_data_start(packet);
		if (ether->type != PV_ETH_TYPE_VLAN) {
			return;
		}

		packet->ol_flags |= PV_PKT_RX_VLAN | PV_PKT_RX_VLAN_STRIPPED;

		struct pv_vlan* vlan = (struct pv_vlan*)PV_ETH_PAYLOAD(ether);

		packet->vlan.is_exists = true;
		packet->vlan.tci = vlan->tci;

		// MAGIC: PV_ETHER_HDR_LEN - sizeof(ether->type) -- vlan->etype becomes ether->type
		memmove(pv_packet_data_start(packet) + PV_VLAN_HDR_LEN, pv_packet_data_start(packet), PV_ETH_HDR_LEN - sizeof(ether->type));

		packet->start += PV_VLAN_HDR_LEN;
	}
}

bool rx_offload_vlan_filter(const struct pv_nic* nic, struct pv_packet* const packet) {
	uint16_t vlan_id;
	if (packet->mbuf->ol_flags & PKT_RX_VLAN_STRIPPED) {
		// Already stripped by HW
		vlan_id = packet->mbuf->vlan_tci & 0x7f;
	} else {
		struct pv_ethernet* ether = (struct pv_ethernet*)pv_packet_data_start(packet);
		if(ether->type != PV_ETH_TYPE_VLAN) {
			return true;
		}
		struct pv_vlan* vlan = (struct pv_vlan*)PV_ETH_PAYLOAD(ether);
		vlan_id = vlan->tci.id;
	}

	struct pv_set* vlan_ids = nic->vlan_ids;
	return pv_set_contains(vlan_ids, &vlan_id);
}

void rx_offload_ipv4_checksum(const struct pv_nic* nic, struct pv_packet* const packet) {
	struct pv_ethernet * const ether = (struct pv_ethernet *)pv_packet_data_start(packet);
	struct rte_mbuf* const mbuf = packet->mbuf;
	
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

void tx_offload_vlan_insert(const struct pv_nic* nic, struct pv_packet* const packet) {

	if(pv_nic_is_tx_offload_supported(nic, DEV_TX_OFFLOAD_VLAN_INSERT)) {
		struct rte_mbuf *const mbuf = packet->mbuf;
		mbuf->ol_flags |= PKT_TX_VLAN;
		mbuf->vlan_tci = pv_vlan_tci_to_uint16(packet->vlan.tci);
	} else {
		void* start = pv_packet_data_start(packet);
		struct pv_ethernet* ether = (struct pv_ethernet*) start;
		memmove(start - PV_VLAN_HDR_LEN, start, PV_ETH_HDR_LEN - sizeof(ether->type));
		packet->start -= PV_VLAN_HDR_LEN;
		
		// Move to new header pos
		ether = (struct pv_ethernet*)pv_packet_data_start(packet);
		
		ether->type = PV_ETH_TYPE_VLAN;
		void* tci_pos = PV_ETH_PAYLOAD(ether);
		memcpy(tci_pos, &packet->vlan.tci, sizeof(packet->vlan.tci));
	}
}

void tx_offload_ipv4_checksum(const struct pv_nic* nic, struct pv_packet* const packet) {
	struct pv_ethernet* const ether = (struct pv_ethernet*)pv_packet_data_start(packet);
	struct pv_ipv4 * const ipv4 = (struct pv_ipv4 *)PV_ETH_PAYLOAD(ether);
	struct rte_mbuf* const mbuf = packet->mbuf;

	if(pv_nic_is_tx_offload_supported(nic, DEV_TX_OFFLOAD_IPV4_CKSUM)) {
		mbuf->ol_flags |= PKT_TX_IPV4 | PKT_TX_IP_CKSUM;
		mbuf->l2_len = sizeof(struct pv_ethernet);
		mbuf->l3_len = ipv4->hdr_len * 4;
	} else {
		ipv4->checksum = 0;
		ipv4->checksum = checksum(ipv4, ipv4->hdr_len * 4);
	}
}
