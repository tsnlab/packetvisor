#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <net/if.h>
#include "pv.h"
#include "arp.h"

// XXX: Change this value with veth0's MAC address
static const struct ether_addr opt_txsmac = {{ 0xae, 0x3a, 0x63, 0x2d, 0x7c, 0xed }};    // Source MAC address

static bool is_pv_done = false;

static void int_exit(int sig) {
    is_pv_done = true;
    printf("PV end\n");
}

static uint32_t process_packets(struct pv_nic* nic, struct pv_packet** packets, uint32_t batch_size) {
    uint32_t processed = 0;

    for (uint32_t i = 0; i < batch_size; i++) {
        uint8_t* rx_packet = packets[i]->buffer + packets[i]->start;

        // analyze packet and create packet
        if (rx_packet[12] == 0x08 && rx_packet[13] == 0x06 && rx_packet[20] == 0x00 && rx_packet[21] == 0x01) {     // ARP request packet
            packets[processed] = packets[i];
            gen_arp_response_packet(packets[processed], opt_txsmac);
            processed++;
        } else {
            pv_free(nic, packets[i]);
        }
    }
    return processed;
}

int main(int argc, char** argv) {
    const char* if_name = "";
    uint32_t chunk_size, chunk_count, rx_ring_size, tx_ring_size, filling_ring_size, completion_ring_size;

    /* option */
    if (argc == 8) {
        uint32_t if_index;
        if_name = argv[1];
        if_index = if_nametoindex(if_name);
        if (!if_index) {
         printf("ERROR: interface \"%s\" does not exist\n", if_name);
         exit(EXIT_FAILURE);
        }

        chunk_size = atoi(argv[2]);
        if (chunk_size < 2048 || chunk_size > 4096) {
            printf("ERROR: chunk size should be 2048 ~ 4096\n");
            exit(EXIT_FAILURE);
        }

        // UMEM chunk count(UMEM size) should be in the following range: (rx ring size + tx ring size) x1.5 ~ x2 (from experience)
        chunk_count = atoi(argv[3]);
        if (chunk_count < 0) {
            printf("ERROR: check the number of UMEM size\n");
            exit(EXIT_FAILURE);
        }

        filling_ring_size = atoi(argv[4]);
        if (filling_ring_size < 16 || filling_ring_size > 1024) {
            printf("ERROR: Filling ring size should be 16 ~ 1024 (power of 2)\n");
            exit(EXIT_FAILURE);
        }

        rx_ring_size = atoi(argv[5]);
        if (rx_ring_size < 16 || rx_ring_size > 1024) {
            printf("ERROR: RX ring size should be 16 ~ 1024 (power of 2)\n");
            exit(EXIT_FAILURE);
        }

        completion_ring_size = atoi(argv[6]);
        if (completion_ring_size < 16 || completion_ring_size > 1024) {
            printf("ERROR: Completion ring size should be 16 ~ 1024 (power of 2\n");
            exit(EXIT_FAILURE);
        }

        tx_ring_size = atoi(argv[7]);
        if (tx_ring_size < 16 || tx_ring_size > 1024) {
            printf("ERROR: TX ring size should be 16 ~ 1024 (power of 2)\n");
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Usage: %s <inferface name> <chunk size> <chunk count> <Filling ring size> <RX ring size> <Completion ring size> <TX ring size>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    uint32_t rx_batch_size = 64;
    struct pv_packet* packets[rx_batch_size];

    struct pv_nic* nic = pv_open(if_name, chunk_size, chunk_count, rx_ring_size, tx_ring_size, filling_ring_size, completion_ring_size);
    if (nic == NULL) {
        printf("ERROR: Packetvisor couldn't be opened\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, int_exit);
    signal(SIGTERM, int_exit);
    signal(SIGABRT, int_exit);

    do {
        uint32_t received = pv_receive(nic, packets, rx_batch_size);
        if (received > 0) {
            uint32_t processed = process_packets(nic, packets, received);
            uint32_t sent = pv_send(nic, packets, processed);

            if (sent == 0) {
                for (uint32_t i = 0; i < processed; i++) {
                    pv_free(nic, packets[i]);
                }
            }
        }
    } while (!is_pv_done);

    int32_t ret = pv_close(nic);
    if (ret < 0) {
        exit(EXIT_FAILURE);
    }

    return 0;
}
