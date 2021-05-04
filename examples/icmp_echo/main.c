#include <stdio.h>

#include <pv/pv.h>
#include <pv/nic.h>
#include <pv/net/ethernet.h>
#include <pv/net/ipv4.h>
#include <pv/net/arp.h>

uint64_t mac;
uint32_t ipv4;

int process_arp(struct pv_ethernet* ether) {
    // Set dst
    ether->dmac = ether->smac;
    ether->smac = mac;

    struct pv_arp* arp = (struct pv_arp*)PV_ETH_PAYLOAD(ether);
	if (arp->opcode != PV_ARP_OPCODE_ARP_REQUEST) {
		printf("ARP opcode %d is not supported\n", arp->opcode);
		return 0;
	}

	if (arp->dst_proto != ipv4) {
		// Not my IP, ignore
		printf("Not my ip. ignore\n");
		return 0;
	}

	printf("Reply to ARP request\n");

	arp->opcode = PV_ARP_OPCODE_ARP_REPLY;
	arp->dst_hw = arp->src_hw;
	arp->dst_proto = arp->src_proto;
	arp->src_hw = mac;
	arp->src_proto = ipv4;
}

int process_ipv4(struct pv_ethernet* ether) {
	//TODO
	return 0;
}

int process(struct pv_packet* packet) {
    struct pv_ethernet* ether = (struct pv_ethernet*)packet->payload;
    // ether->dmac = ether->smac;
    // ether->smac = mac;

	printf("Got packet.\n");

    switch (ether->type) {
    case PV_ETH_TYPE_ARP:
        process_arp(ether);
        break;
    case PV_ETH_TYPE_IPv4:
        process_ipv4(ether);
        break;
    }

    return 0;
}

int main(int argc, char** argv) {
    int ret = pv_init();
    if(ret != 0) {
        printf("Failed to init pv\n");
        return ret;
    }

    pv_nic_get_mac(0, &mac);
    printf("mac: %012lx\n", mac);

	pv_nic_get_ipv4(0, &ipv4);
	printf("ipv4: %d.%d.%d.%d\n",
	    ipv4 >> (8 * 3) & 0xff,
		ipv4 >> (8 * 2) & 0xff,
		ipv4 >> (8 * 1) & 0xff,
		ipv4 >> (8 * 0) & 0xff);

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
