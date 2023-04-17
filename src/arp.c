#include <string.h>
#include "arp.h"

#define MAC_ADDRESS_LEN     6   // 6 bytes

// Change ARP request packet with ARP response packet.
void gen_arp_response_packet(struct pv_packet* packet, const struct ether_addr smac) {
    /* make packet data from the buffer headroom */

    // copy dest MAC
    memcpy(packet->buffer + packet->start, (packet->buffer + packet->start) + 6, MAC_ADDRESS_LEN);

    // copy src MAC
    memcpy(packet->buffer + packet->start + 6, smac.ether_addr_octet, MAC_ADDRESS_LEN);

    // Ethertype - ARP
    packet->buffer[packet->start + 12] = 0x08;
    packet->buffer[packet->start + 13] = 0x06;

    // HW type
    packet->buffer[packet->start + 14] = 0x00;
    packet->buffer[packet->start + 15] = 0x01;

    // protocol type
    packet->buffer[packet->start + 16] = 0x08;
    packet->buffer[packet->start + 17] = 0x00;

    // HW size
    packet->buffer[packet->start + 18] = 0x06;

    // protocol size
    packet->buffer[packet->start + 19] = 0x04;

    // op code - ARP_REPLY
    packet->buffer[packet->start + 20] = 0x00;
    packet->buffer[packet->start + 21] = 0x02;

    // sender MAC
    packet->buffer[packet->start + 22] = packet->buffer[packet->start + 6];
    packet->buffer[packet->start + 23] = packet->buffer[packet->start + 7];
    packet->buffer[packet->start + 24] = packet->buffer[packet->start + 8];
    packet->buffer[packet->start + 25] = packet->buffer[packet->start + 9];
    packet->buffer[packet->start + 26] = packet->buffer[packet->start + 10];
    packet->buffer[packet->start + 27] = packet->buffer[packet->start + 11];

    // sender IP (10.0.0.4)
    packet->buffer[packet->start + 28] = 0x0a;
    packet->buffer[packet->start + 29] = 0x00;
    packet->buffer[packet->start + 30] = 0x00;
    packet->buffer[packet->start + 31] = 0x04;

    // target MAC
    packet->buffer[packet->start + 32] = packet->buffer[packet->start + 0];
    packet->buffer[packet->start + 33] = packet->buffer[packet->start + 1];
    packet->buffer[packet->start + 34] = packet->buffer[packet->start + 2];
    packet->buffer[packet->start + 35] = packet->buffer[packet->start + 3];
    packet->buffer[packet->start + 36] = packet->buffer[packet->start + 4];
    packet->buffer[packet->start + 37] = packet->buffer[packet->start + 5];

    // dest IP (10.0.0.5)
    packet->buffer[packet->start + 38] = 0x0a;
    packet->buffer[packet->start + 39] = 0x00;
    packet->buffer[packet->start + 40] = 0x00;
    packet->buffer[packet->start + 41] = 0x05;

    // set packet length of ARP response as 42 bytes
    packet->end = packet->start + ARP_RSPN_PACKET_SIZE;
}

void gen_fixed_packet(struct pv_packet* packet) {
    /* make packet data from the buffer headroom */

    // dest MAC
    packet->buffer[packet->start + 0] = 0xB2;
    packet->buffer[packet->start + 1] = 0xF1;
    packet->buffer[packet->start + 2] = 0x19;
    packet->buffer[packet->start + 3] = 0x5A;
    packet->buffer[packet->start + 4] = 0x98;
    packet->buffer[packet->start + 5] = 0xCA;

    // src MAC
    packet->buffer[packet->start + 6] = 0xAE;
    packet->buffer[packet->start + 7] = 0x3A;
    packet->buffer[packet->start + 8] = 0x63;
    packet->buffer[packet->start + 9] = 0x2D;
    packet->buffer[packet->start + 10] = 0x7C;
    packet->buffer[packet->start + 11] = 0xED;

    // Ethertype - ARP
    packet->buffer[packet->start + 12] = 0x08;
    packet->buffer[packet->start + 13] = 0x06;

    // HW type
    packet->buffer[packet->start + 14] = 0x00;
    packet->buffer[packet->start + 15] = 0x01;

    // protocol type
    packet->buffer[packet->start + 16] = 0x08;
    packet->buffer[packet->start + 17] = 0x00;

    // HW size
    packet->buffer[packet->start + 18] = 0x06;

    // protocol size
    packet->buffer[packet->start + 19] = 0x04;

    // op code - ARP_REPLY
    packet->buffer[packet->start + 20] = 0x00;
    packet->buffer[packet->start + 21] = 0x02;

    // sender MAC
    packet->buffer[packet->start + 22] = packet->buffer[packet->start + 6];
    packet->buffer[packet->start + 23] = packet->buffer[packet->start + 7];
    packet->buffer[packet->start + 24] = packet->buffer[packet->start + 8];
    packet->buffer[packet->start + 25] = packet->buffer[packet->start + 9];
    packet->buffer[packet->start + 26] = packet->buffer[packet->start + 10];
    packet->buffer[packet->start + 27] = packet->buffer[packet->start + 11];

    // sender IP (10.0.0.4)
    packet->buffer[packet->start + 28] = 0x0a;
    packet->buffer[packet->start + 29] = 0x00;
    packet->buffer[packet->start + 30] = 0x00;
    packet->buffer[packet->start + 31] = 0x04;

    // target MAC
    packet->buffer[packet->start + 32] = packet->buffer[packet->start + 0];
    packet->buffer[packet->start + 33] = packet->buffer[packet->start + 1];
    packet->buffer[packet->start + 34] = packet->buffer[packet->start + 2];
    packet->buffer[packet->start + 35] = packet->buffer[packet->start + 3];
    packet->buffer[packet->start + 36] = packet->buffer[packet->start + 4];
    packet->buffer[packet->start + 37] = packet->buffer[packet->start + 5];

    // dest IP (10.0.0.5)
    packet->buffer[packet->start + 38] = 0x0a;
    packet->buffer[packet->start + 39] = 0x00;
    packet->buffer[packet->start + 40] = 0x00;
    packet->buffer[packet->start + 41] = 0x05;

    packet->end = packet->start + ARP_RSPN_PACKET_SIZE;
}
