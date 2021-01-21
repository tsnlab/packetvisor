#include <stdio.h>

#include <pv/pv.h>
#include <pv/nic.h>
#include <pv/net/ethernet.h>
#include <pv/net/ip.h>
#include <pv/net/udp.h>

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

	if(ip->proto != PV_IP_PROTO_UDP)
		return 0;

	struct pv_udp* udp = (struct pv_udp*)PV_IPv4_DATA(ip);
	uint16_t src_port = udp->srcport;
	udp->srcport = udp->dstport;
	udp->dstport = src_port;
	udp->checksum = 0;

	return 0;
}

int main(int argc, char** argv) {
	int ret = pv_init(argc, argv);
	printf("pv_init(): %d\n", ret);

	mac = pv_nic_get_mac(0);
	printf("mac : %lx\n", mac);

	struct pv_packet* pkt_buf[100] = {};

	while(1) {
		uint16_t nrecv = pv_nic_rx_burst(0, 0, pkt_buf, 100);
		if(nrecv == 0) {
			continue;
		}

		for(uint16_t i = 0; i < nrecv; i++) {
			process(pkt_buf[i]);
		}

		uint16_t nsent = pv_nic_tx_burst(0, 0, pkt_buf, nrecv);
		if(nsent == 0) {
			printf("nsent is 0\n");
			return 1;
		}

	}

	return 0;
}
