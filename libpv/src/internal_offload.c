#include "internal_offload.h"

#include <rte_mbuf.h>
#include <rte_ethdev.h>

#include <pv/net/ipv4.h>
#include <pv/checksum.h>


void rx_offload_ipv4_checksum(const struct pv_nic* nic, struct pv_ethernet* const ether, struct rte_mbuf* const mbuf) {
	struct pv_ipv4 * const ipv4 = (struct pv_ipv4 *)PV_ETH_PAYLOAD(ether);
	uint16_t pkt_checksum = ipv4->checksum;

	ipv4->checksum = 0;
	uint16_t calculated_checksum = checksum(ipv4, ipv4->hdr_len * 4);

	if (pkt_checksum == calculated_checksum) {
		printf("Checksum good\n");
		mbuf->ol_flags |= PKT_RX_IP_CKSUM_GOOD;
	} else {
		printf("Checksum bad\n");
		mbuf->ol_flags |= PKT_RX_IP_CKSUM_BAD;
	}
	
	ipv4->checksum = pkt_checksum;
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
