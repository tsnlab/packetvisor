#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <pv/pv.h>
#include <pv/nic.h>
#include <pv/packet.h>
#include <pv/net/ethernet.h>
#include <pv/net/arp.h>
#include <pv/net/ipv4.h>
#include <pv/net/icmp.h>
#include <pv/net/udp.h>

#include "util.h"

#define ECHO_PORT 7

void print_info();

void process(struct pv_packet * pkt);
void process_arp(struct pv_packet * pkt);
void process_ipv4(struct pv_packet * pkt);
void process_icmp(struct pv_packet * pkt);
void process_udp(struct pv_packet * pkt);

struct pv_ethernet * get_ether(struct pv_packet * pkt) {
    return (struct pv_ethernet *)(pkt->buffer + pkt->start);
}

uint32_t pkt_length(struct pv_packet * pkt) {
    return pkt->end - pkt->start;
}

uint64_t mymac;
uint32_t myip;

int main(int argc, char * argv[]) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <ipv4>\n", argv[0]);
        exit(1);
    }

    if(pv_init() != 0) {
        fprintf(stderr, "Cannot initialize packetvisor\n");
        exit(1);
    }

    mymac = pv_nic_get_mac(0);
    myip = ntohl(inet_addr(argv[1]));

    print_info();

    struct pv_packet * pkts[512];
    const int max_pkts = sizeof(pkts) / sizeof(pkts[0]);

    while(1) {
        uint16_t count = pv_nic_rx_burst(0, 0, pkts, max_pkts);

        if(count == 0) {
            usleep(100);
            continue;
        }

        for(uint16_t i = 0; i < count; i++) {
            process(pkts[i]);
        }
    }

    pv_finalize();
    return 0;
}

void print_info() {
    printf(
        "MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
        (uint8_t)(mymac >> (8 * 5)) & 0xff,
        (uint8_t)(mymac >> (8 * 4)) & 0xff,
        (uint8_t)(mymac >> (8 * 3)) & 0xff,
        (uint8_t)(mymac >> (8 * 2)) & 0xff,
        (uint8_t)(mymac >> (8 * 1)) & 0xff,
        (uint8_t)(mymac >> (8 * 0)) & 0xff);

    printf(
        "IP: %d.%d.%d.%d\n",
        myip >> (8 * 3) & 0xff,
        myip >> (8 * 2) & 0xff,
        myip >> (8 * 1) & 0xff,
        myip >> (8 * 0) & 0xff);
}

void process(struct pv_packet * pkt) {
    struct pv_ethernet * ether = get_ether(pkt);

    if(ether->dmac != mymac && ether->dmac == 0xffffffffffff) {
        pv_packet_free(pkt);
    }

    switch(ether->type) {
    case PV_ETH_TYPE_ARP:
        process_arp(pkt);
        break;
    case PV_ETH_TYPE_IPv4:
        process_ipv4(pkt);
        break;
    default:
        printf("Drop\n");
        pv_packet_free(pkt);
    }
}

void process_arp(struct pv_packet * pkt) {
    struct pv_ethernet * ether = get_ether(pkt);
    struct pv_arp * arp = (struct pv_arp *)PV_ETH_PAYLOAD(ether);

    if(arp->opcode != PV_ARP_OPCODE_ARP_REQUEST || arp->dst_proto != myip) {
        pv_packet_free(pkt);
        return;
    }

    ether->dmac = ether->smac;
    ether->smac = mymac;

    arp->opcode = PV_ARP_OPCODE_ARP_REPLY;
    arp->dst_hw = arp->src_hw;
    arp->dst_proto = arp->src_proto;
    arp->src_hw = mymac;
    arp->src_proto = myip;

    char dstip[16];
    ip_to_str(arp->dst_proto, dstip);
    printf("Replying ARP request from %s\n", dstip);

    pv_nic_tx(0, 0, pkt);
}

void process_ipv4(struct pv_packet * pkt) {
    struct pv_ethernet * ether = get_ether(pkt);
    struct pv_ipv4 * ipv4 = (struct pv_ipv4 *)PV_ETH_PAYLOAD(ether);

    if(ipv4->dst != myip) {
        pv_packet_free(pkt);
        return;
    }

    switch(ipv4->proto) {
    case PV_IP_PROTO_ICMP:
        process_icmp(pkt);
        break;
    case PV_IP_PROTO_UDP:
        process_udp(pkt);
        break;
    default:
        pv_packet_free(pkt);
    }
}

void process_icmp(struct pv_packet * pkt) {
    struct pv_ethernet * ether = get_ether(pkt);
    struct pv_ipv4 * ipv4 = (struct pv_ipv4 *)PV_ETH_PAYLOAD(ether);
    struct pv_icmp * icmp = (struct pv_icmp *)PV_IPv4_PAYLOAD(ipv4);

    if(icmp->type != PV_ICMP_TYPE_ECHO_REQUEST) {
        pv_packet_free(pkt);
        return;
    }

    ether->dmac = ether->smac;
    ether->smac = mymac;

    ipv4->dst = ipv4->src;
    ipv4->src = myip;

    icmp->type = PV_ICMP_TYPE_ECHO_REPLY;

    pv_icmp_checksum(icmp, PV_IPv4_PAYLOAD_LEN(ipv4));
    pv_ipv4_checksum(ipv4);
    pv_nic_tx(0, 0, pkt);
}

void process_udp(struct pv_packet * pkt) {
    struct pv_ethernet * ether = get_ether(pkt);
    struct pv_ipv4 * ipv4 = (struct pv_ipv4 *)PV_ETH_PAYLOAD(ether);
    struct pv_udp * udp = (struct pv_udp *)PV_IPv4_PAYLOAD(ipv4);

    if (udp->dstport != ECHO_PORT) {
        pv_packet_free(pkt);
        return;
    }

    printf("Echo UDP\n");

    ether->dmac = ether->smac;
    ether->smac = mymac;

    ipv4->dst = ipv4->src;
    ipv4->src = myip;

    uint16_t port = udp->dstport;
    udp->dstport = udp->srcport;
    udp->srcport = port;

    pv_udp_checksum_ipv4(udp, ipv4);
    pv_ipv4_checksum(ipv4);

    pv_nic_tx(0, 0, pkt);
}
