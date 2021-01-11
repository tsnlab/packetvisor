#include <stdio.h>

#include <pv/pv.h>
#include <pv/nic.h>
#include <pv/net/ethernet.h>

int process(struct pv_packet* packet) {
	struct pv_ethernet* ether = (struct pv_ethernet*)packet->payload;
	ether->dmac = ether->smac;
	printf("after ether\n");
	uint64_t smac = pv_nic_get_mac(packet->nic_id);
	printf("after ether2\n");
	ether->smac = pv_nic_get_mac(packet->nic_id);

	return 0;
}

int main(int argc, char** argv) {
	int ret = pv_init(argc, argv);
	printf("pv_init(): %d\n", ret);

	uint64_t mac = pv_nic_get_mac(0);
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

		uint16_t nsent = pv_nic_tx_burst(0, 0, pkt_buf, 100);
		if(nsent == 0) {
			printf("nsent is 0\n");
			return 1;
			}

	}

	return 0;
}
