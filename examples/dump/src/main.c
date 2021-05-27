#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <pv/net/ethernet.h>
#include <pv/nic.h>
#include <pv/packet.h>
#include <pv/pv.h>

int main(int argc, char** argv) {
    if(pv_init() != 0) {
        printf("Cannot initialize packetvisor\n");
    }

    struct pv_packet* packets[64];

    while(true) {
        uint16_t count = pv_nic_rx_burst(0, 0, packets, 64);

        if(count == 0) {
            usleep(100);
            continue;
        }

        for(uint16_t i = 0; i < count; i++) {
            struct pv_ethernet* ether = (struct pv_ethernet*)(packets[i]->buffer + packets[i]->start);

            printf("%012lx %012lx %04x (%u bytes)\n", (uint64_t)ether->dmac, (uint64_t)ether->smac, ether->type,
                   packets[i]->end - packets[i]->start);
            pv_packet_free(packets[i]);
        }
    }

    pv_finalize();

    return 0;
}
