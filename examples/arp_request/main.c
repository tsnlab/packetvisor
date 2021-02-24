#include <stdio.h>
#include <unistd.h>

#include <pv/pv.h>
#include <pv/core.h>
#include <pv/nic.h>
#include <pv/net/ethernet.h>
#include <pv/net/arp.h>
#include <pv/net/ipv4.h>

int process(void* ctx) {
	while(1) {
		sleep(1);
		struct pv_packet* packet = pv_packet_alloc();
		if(packet == NULL) {
			printf("Failed to alloc packet\n");
			break;
		}

		if(pv_packet_add_tail_paylen(packet, sizeof(struct pv_ethernet) + sizeof(struct pv_arp)) == false) {
			printf("Failed to add paylen\n");
			pv_packet_free(packet);
			break;
		}

		struct pv_ethernet* ether = (struct pv_ethernet*)packet->payload;
		ether->dmac = 0xffffffffffff;
		uint64_t tmp_mac;
		pv_nic_get_mac(0, &tmp_mac);
		ether->smac = tmp_mac;
		ether->type = PV_ETH_TYPE_ARP;

		struct pv_arp* arp = (struct pv_arp*)PV_ETH_PAYLOAD(ether);
		arp->hw_type = 1;
		arp->proto_type = PV_ETH_TYPE_IPv4;
		arp->hw_size = 6;
		arp->proto_size = 4;
		arp->opcode = 1;	// arp request
		arp->src_hw = ether->smac;
		uint32_t tmp_ip;
		pv_nic_get_ipv4(0, &tmp_ip);
		arp->src_proto = tmp_ip;
		arp->dst_proto = 0xc0a80002;

		if(pv_nic_tx(0, 0, packet) == false) {
			printf("Failed to send packet\n");
		}
		printf("core[%u] sends packet\n", (uint32_t)(uintptr_t)ctx);

	}
}

int main(int argc, char** argv) {
	int ret = pv_init();
	if(ret != 0) {
		printf("Failed to init pv\n");
		return ret;
	}

	printf("Core count: %u\n", pv_core_count());

	for(int i = 1; i < pv_core_count(); i++) {
		pv_core_start(i, process, (void*)(uintptr_t)i);
	}

	process((void*)0);

	for(int i = 1; i < pv_core_count(); i++) {
		pv_core_wait(i);
	}

	return 0;
}
