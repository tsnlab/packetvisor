#include <stdio.h>

#include <pv/pv.h>
#include <pv/nic.h>
#include <pv/net/ethernet.h>
#include <pv/net/ipv4.h>

uint64_t mac;

int process(struct pv_packet* packet) {
	struct pv_ethernet* ether = (struct pv_ethernet*)packet->payload;
	ether->dmac = ether->smac;
	ether->smac = mac;

	if(ether->type != PV_ETH_TYPE_IPv4)
		return 0;

	struct pv_ipv4* ip = (struct pv_ipv4*)PV_ETH_PAYLOAD(ether);
	uint32_t src_ip = ip->src;
	ip->src = ip->dst;
	ip->dst = src_ip;
	ip->checksum = 0;

	return 0;
}

int main(int argc, char** argv) {
	int ret = pv_init(argc, argv);
	if(ret != 0) {
		printf("Failed to init pv\n");
		return ret;
	}

	pv_debug();

	pv_nic_get_mac(0, &mac);
	printf("mac: %lx\n", mac);

	struct pv_packet* pkt_buf[1024] = {};

	while(1) {
		uint16_t nrecv = pv_nic_rx_burst(0, 0, pkt_buf, 1024);
		if(nrecv == 0) {
			continue;
		}

		for(uint16_t i = 0; i < nrecv; i++) {
			process(pkt_buf[i]);
		}

		uint16_t nsent = pv_nic_tx_burst(0, 0, pkt_buf, nrecv);
		if(nsent == 0) {
			printf("nsent is 0\n");
		}

	}

	return 0;
}
