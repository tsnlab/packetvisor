#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <pv/config.h>
#include <pv/net/ethernet.h>
#include <pv/nic.h>
#include <pv/packet.h>
#include <pv/pv.h>

int main(int argc, char** argv) {
    if(pv_init() != 0) {
        printf("Cannot initialize packetvisor\n");
    }

    struct pv_packet* packets[64];

    printf("Getting cores\n");
    int cores[16];
    size_t cores_count = pv_config_get_cores(cores, 16);
    printf("There are %lu cores\n", cores_count);

    for (int i = 0; i < cores_count; i += 1) {
        printf("Core: %d\n", cores[i]);
    }

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
