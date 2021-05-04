#include <stdio.h>

#include <pv/pv.h>
#include <pv/nic.h>
#include <pv/net/ethernet.h>
#include <pv/net/ipv4.h>
#include <pv/net/arp.h>
#include <pv/net/icmp.h>

uint64_t my_mac;
uint32_t my_ipv4;

int process_icmp(struct pv_icmp* icmp);

int process_arp(struct pv_arp* arp) {
    if (arp->opcode != PV_ARP_OPCODE_ARP_REQUEST) {
        printf("ARP opcode %d is not supported\n", arp->opcode);
        return 0;
    }

    if (arp->dst_proto != my_ipv4) {
        // Not my IP, ignore
        printf("Not my ip. ignore\n");
        return 0;
    }

    printf("Reply to ARP request\n");

    arp->opcode = PV_ARP_OPCODE_ARP_REPLY;
    arp->dst_hw = arp->src_hw;
    arp->dst_proto = arp->src_proto;
    arp->src_hw = my_mac;
    arp->src_proto = my_ipv4;

    return 0;
}

int process_ipv4(struct pv_ipv4* ipv4) {
    void* payload = PV_IPv4_DATA(ipv4);

    ipv4->dst = ipv4->src;
    ipv4->src = my_ipv4;
    ipv4->checksum = 0;

    switch (ipv4->proto) {
    case PV_IP_PROTO_ICMP:
        process_icmp(payload);
    }
    return 0;
}

int process_icmp(struct pv_icmp* icmp) {

    printf("icmp type: %d\n", icmp->type);

    if (icmp->type == PV_ICMP_TYPE_ECHO_REQUEST) {
        icmp->type = PV_ICMP_TYPE_ECHO_REPLY;
    }

    icmp->checksum = 0;

    return 0;
}

int process(struct pv_packet* packet) {
    struct pv_ethernet* ether = (struct pv_ethernet*)packet->payload;
    ether->dmac = ether->smac;
    ether->smac = my_mac;

    printf("Got packet.\n");

    void* payload = PV_ETH_PAYLOAD(ether);

    switch (ether->type) {
    case PV_ETH_TYPE_ARP:
        process_arp(payload);
        break;
    case PV_ETH_TYPE_IPv4:
        process_ipv4(payload);
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

    pv_nic_get_mac(0, &my_mac);
    printf("mac: %012lx\n", my_mac);

    pv_nic_get_ipv4(0, &my_ipv4);
    printf("ipv4: %d.%d.%d.%d\n",
        my_ipv4 >> (8 * 3) & 0xff,
        my_ipv4 >> (8 * 2) & 0xff,
        my_ipv4 >> (8 * 1) & 0xff,
        my_ipv4 >> (8 * 0) & 0xff);

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
