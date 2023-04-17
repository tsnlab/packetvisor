#include <stdlib.h>
#include <sys/mman.h>
#include <net/if_arp.h>
#include <linux/if_link.h>
#include "pv.h"

// XXX: Change values to see dumping RX and TX packets
// #define _DEBUG
#define DEFAULT_HEADROOM 256
#define INVALID_CHUNK_INDEX ((uint64_t)-1)

#ifdef _DEBUG
static void hex_dump(void *packet, size_t length, uint64_t addr) {
    const unsigned char *address = (unsigned char *)packet;
    const unsigned char *line = address;
    size_t line_size = 32;
    unsigned char c;
    char buf[32];
    int32_t i = 0;

    sprintf(buf, "addr=%lu", addr);
    // printf("addr=%u ", addr);
    printf("length = %zu\n", length);
    printf("%s | ", buf);
    while (length-- > 0)
    {
        printf("%02X ", *address++);
        if (!(++i % line_size) || (length == 0 && i % line_size))
        {
            if (length == 0)
            {
                while (i++ % line_size)
                    printf("__ ");
            }
            printf(" | "); /* right close */
            while (line < address)
            {
                c = *line++;
                printf("%c", (c < 33 || c == 255) ? 0x2E : c);
            }
            printf("\n");
            if (length > 0)
                printf("%s | ", buf);
        }
    }
    printf("\n");
}
#endif

static uint64_t _pv_alloc(struct pv_nic* nic) {
    if (nic->chunk_pool_idx > 0) {
        return nic->chunk_pool[--nic->chunk_pool_idx];
    } else {
        return INVALID_CHUNK_INDEX;
    }
}

static void _pv_free(struct pv_nic* nic, uint64_t chunk_addr) {
    // align **chunk_addr with chunk size
    uint64_t remainder = chunk_addr % nic->chunk_size;
    chunk_addr = chunk_addr - remainder;
    nic->chunk_pool[nic->chunk_pool_idx++] = chunk_addr;
}

struct pv_packet* pv_alloc(struct pv_nic* nic) {
    uint64_t idx = _pv_alloc(nic);
    if (idx == INVALID_CHUNK_INDEX) {
        return NULL;
    }

    struct pv_packet* packet = calloc(1, sizeof(struct pv_packet));
    if (packet == NULL) {
        return NULL;
    }

    packet->start = DEFAULT_HEADROOM;//테스트 필요
    packet->end = DEFAULT_HEADROOM;
    packet->size = nic->chunk_size;
    packet->buffer = xsk_umem__get_data(nic->buffer, idx);
    packet->priv = (void*)idx;

    return packet;
}

void pv_free(struct pv_nic* nic, struct pv_packet* packet) {
    _pv_free(nic, (uint64_t)packet->priv);
    free(packet);
}

static int32_t configure_umem(struct pv_nic* nic, uint32_t chunk_size, uint32_t chunk_count, uint32_t fill_size, uint32_t complete_size) {
    uint32_t mmap_buffer_size = chunk_count * chunk_size; // chunk_count is UMEM size.

    /* Reserve memory for the UMEM. */
    void* mmap_address = mmap(NULL, mmap_buffer_size,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mmap_address == MAP_FAILED) {
        return -1;
    }

    /* create UMEM, filling ring, completion ring for xsk */
    struct xsk_umem_config umem_cfg = {
        // ring sizes aren't usually over 1024
        .fill_size = fill_size,
        .comp_size = complete_size,
        .frame_size = chunk_size,
        .frame_headroom = XSK_UMEM__DEFAULT_FRAME_HEADROOM,
        .flags = XSK_UMEM__DEFAULT_FLAGS};

    int32_t ret = xsk_umem__create(&nic->umem, mmap_address, mmap_buffer_size, &nic->fq, &nic->cq, &umem_cfg);
    if (ret != 0) {
        munmap(mmap_address, mmap_buffer_size);
        return -2;
    }

    nic->buffer = mmap_address;

    return 0;
}

struct pv_nic* pv_open(const char* if_name, uint32_t chunk_size, uint32_t chunk_count, uint32_t rx_ring_size,
                       uint32_t tx_ring_size, uint32_t filling_ring_size, uint32_t completion_ring_size) {
    /* create pv_nic */
    struct pv_nic* nic = calloc(1, sizeof(struct pv_nic));
    if (nic == NULL) {
        return NULL;
    }

    /* initialize UMEM chunk information */
    nic->chunk_size = chunk_size;
    nic->chunk_pool_idx = chunk_count;   // **chunk_count is UMEM size
    nic->chunk_pool = (uint64_t*)calloc(chunk_count, sizeof(uint64_t));
    for (uint32_t i = 0; i < chunk_count; i++) {
        nic->chunk_pool[i] = (uint64_t)i * chunk_size; // put chunk address
    }

    /* configure UMEM */;
    int32_t umem_ret = configure_umem(nic, chunk_size, chunk_count, filling_ring_size, completion_ring_size);
    if (umem_ret < 0) {
        free(nic->chunk_pool);
        free(nic);
        return NULL;
    }

    /* pre-allocate UMEM chunks into fq. */
    uint32_t fq_idx;
    uint32_t reserved = xsk_ring_prod__reserve(&nic->fq, filling_ring_size, &fq_idx);

    for (int i = 0; i < reserved; i++) {
        *xsk_ring_prod__fill_addr(&nic->fq, fq_idx++) = _pv_alloc(nic); // allocation of UMEM chunks into fq.
    }
    xsk_ring_prod__submit(&nic->fq, reserved); // notify kernel of allocating UMEM chunks into fq as much as **reserved.

    /* setting xsk, RX ring, TX ring configuration */
    struct xsk_socket_config xsk_cfg = {
        .rx_size = rx_ring_size,
        .tx_size = tx_ring_size,
        /* zero means loading default XDP program.
        if you need to load other XDP program, set 1 on this flag and use xdp_program__open_file(), xdp_program__attach() in libxdp. */
        .libxdp_flags = 0,
        .xdp_flags = XDP_FLAGS_DRV_MODE,     // driver mode (Native mode)
        .bind_flags = XDP_USE_NEED_WAKEUP
    };

    /* create xsk socket */
    int32_t xsk_ret = xsk_socket__create(&nic->xsk, if_name, 0, nic->umem, &nic->rx, &nic->tx, &xsk_cfg);
    if (xsk_ret != 0) {
        free(nic->chunk_pool);
        free(nic);
        return NULL;
    }

    return nic;
}

int32_t pv_close(struct pv_nic* nic) {
    /* xsk delete */
    xsk_socket__delete(nic->xsk);

    /* UMEM free */
    int32_t ret = xsk_umem__delete(nic->umem);

    return ret;
}

uint32_t pv_receive(struct pv_nic* nic, struct pv_packet** packets, uint32_t batch_size) {
    /* pre-allocate UMEM chunks into fq as much as **batch_size to receive packets */
    uint32_t fq_idx;
    uint32_t reserved = xsk_ring_prod__reserve(&nic->fq, batch_size, &fq_idx); // reserve slots in fq as much as **batch_size.

    for (uint32_t i = 0; i < reserved; i++) {
        *xsk_ring_prod__fill_addr(&nic->fq, fq_idx++) = _pv_alloc(nic); // allocate UMEM chunks into fq.
    }
    xsk_ring_prod__submit(&nic->fq, reserved); // notify kernel of allocating UMEM chunks into fq as much as **reserved.

    /* save packet metadata from received packets in RX ring. */
    uint32_t rx_idx;
    uint32_t received = xsk_ring_cons__peek(&nic->rx, batch_size, &rx_idx); // fetch the number of received packets in RX ring.

    const struct xdp_desc* rx_desc;
    if (received > 0) {
        uint32_t metadata_count = 0;
        for (metadata_count = 0; metadata_count < received; metadata_count++) {
            /* create packet metadata */
            packets[metadata_count] = calloc(1, sizeof(struct pv_packet));
            if (packets[metadata_count] == NULL) {
                break;
            }

            rx_desc = xsk_ring_cons__rx_desc(&nic->rx, rx_idx + metadata_count); // bringing information(packet address, packet length) of received packets through descriptors in RX ring
            /* save metadata */
            packets[metadata_count]->start = DEFAULT_HEADROOM;
            packets[metadata_count]->end = DEFAULT_HEADROOM + rx_desc->len;
            packets[metadata_count]->size = nic->chunk_size;
            packets[metadata_count]->buffer = xsk_umem__get_data(nic->buffer, rx_desc->addr - DEFAULT_HEADROOM);
            packets[metadata_count]->priv = (void*)(rx_desc->addr - DEFAULT_HEADROOM);
            #ifdef _DEBUG
            printf("addr: %llu, len: %u\n", rx_desc->addr, rx_desc->len);
            hex_dump(packets[metadata_count]->buffer, packets[metadata_count]->size, (uint64_t)packets[metadata_count]->priv);
            #endif
        }

        xsk_ring_cons__release(&nic->rx, received); // notify kernel of using filled slots in RX ring as much as **received

        if (metadata_count != received) {
            received = metadata_count;
        }
    }

    if (xsk_ring_prod__needs_wakeup(&nic->fq)) {
        recvfrom(xsk_socket__fd(nic->xsk), NULL, 0, MSG_DONTWAIT, NULL, NULL);
    }

    return received;
}

uint32_t pv_send(struct pv_nic* nic, struct pv_packet** packets, uint32_t batch_size) {
    /* free packet metadata and UMEM chunks as much as the number of filled slots in cq. */
    uint32_t cq_idx;
    uint32_t filled = xsk_ring_cons__peek(&nic->cq, batch_size, &cq_idx); // fetch the number of filled slots(the number of packets completely sent) in cq

    if (filled > 0) {
        for (uint32_t i = 0; i < filled; i++) {
            _pv_free(nic, *xsk_ring_cons__comp_addr(&nic->cq, cq_idx++));   // free UMEM chunks as much as the number of sent packets (same as **filled)
        }
        xsk_ring_cons__release(&nic->cq, filled); // notify kernel that cq has empty slots with **filled (Dequeue)
    }

    /* reserve TX ring as much as batch_size before sending packets. */
    uint32_t tx_idx = 0;
    uint32_t reserved = xsk_ring_prod__reserve(&nic->tx, batch_size, &tx_idx);
    /* send packets if TX ring has been reserved with **batch_size. if not, don't send packets and free them */
    if (reserved < batch_size) {
        /* in case that kernel lost interrupt signal in the previous sending packets procedure,
        repeat to interrupt kernel to send packets which ketnel could have still held.
        (this procedure is for clearing filled slots in cq, so that cq can be reserved as much as **batch_size in the next execution of pv_send(). */
        if (xsk_ring_prod__needs_wakeup(&nic->tx)) {
            sendto(xsk_socket__fd(nic->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
        }
        return 0;
    }

    /* send packets */
    struct xdp_desc* tx_desc;
    for (uint32_t i = 0; i < reserved; i++) {
        /* insert packets to be send into TX ring (Enqueue) */
        struct pv_packet* packet = packets[i];
        tx_desc = xsk_ring_prod__tx_desc(&nic->tx, tx_idx + i);
        tx_desc->addr = (uint64_t)(packet->priv + packet->start);
        tx_desc->len = packet->end - packet->start;

        #ifdef _DEBUG
        printf("addr: %llu, len: %u\n", tx_desc->addr, tx_desc->len);
        hex_dump(packet->buffer, packet->size, (uint64_t)packet->priv);
        #endif
        free(packet);   // free packet metadata of sent packets.
    }

    xsk_ring_prod__submit(&nic->tx, reserved);  // notify kernel of enqueuing TX ring as much as reserved.

    if (xsk_ring_prod__needs_wakeup(&nic->tx)) {
        sendto(xsk_socket__fd(nic->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);   // interrupt kernel to send packets.
    }

    return reserved;
}
