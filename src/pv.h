#pragma once

#include <stdint.h>
#include <xdp/xsk.h>

// Packet metadata
struct pv_packet {
    uint32_t start;     // payload offset pointing the start of payload. ex. 256
    uint32_t end;       // payload offset point the end of payload. ex. 1000
    uint32_t size;      // total size of buffer. ex. 2048
    uint8_t* buffer;    // buffer address.
    void* priv;         // DO NOT touch this.
};

struct pv_nic {
    struct xsk_socket* xsk;     // XDP socket

    struct xsk_umem* umem;      // UMEM
    void* buffer;               // UMEM data address
    uint32_t chunk_size;        // UMEM chunk size

    uint64_t* chunk_pool;       // UMEM chunk array
    int32_t chunk_pool_idx;     // UMEM chunk index (offset)

    /* XSK rings */
    struct xsk_ring_prod fq;    // Filling ring (filling queue)
    struct xsk_ring_cons rx;    // RX ring
    struct xsk_ring_cons cq;    // Completion ring (completion queue)
    struct xsk_ring_prod tx;    // TX ring
};


/* users can make packet metadata(pv_packet) manually by using this function. */
struct pv_packet* pv_alloc(struct pv_nic* nic);

/* users can free packet metadata(pv_packet) manually by using this function. */
void pv_free(struct pv_nic* nic, struct pv_packet* packet);

/* this function opens packetvisor using XDP socket which is attached to if_name */
struct pv_nic* pv_open(const char* if_name, uint32_t chunk_size, uint32_t chunk_count, uint32_t rx_size,
                        uint32_t tx_size, uint32_t fill_size, uint32_t complete_size);

/* this function closes packetvisor from pv_nic.
it returns -1 when failed, and 0 when success. */
int32_t pv_close(struct pv_nic* nic);

/* this function receives packets from pv_nic which XDP program is attached to and push packet data
into packet metadata(pv_packet). it returns the number of received packets */
uint32_t pv_receive(struct pv_nic* nic, struct pv_packet** packets, uint32_t batch_size);

/* this function tries to send packets as much as batch_size or don't send packets if packets to be sent is not ready up to batch_size.
the function returns the number of sent packets */
uint32_t pv_send(struct pv_nic* nic, struct pv_packet** packets, uint32_t batch_size);
