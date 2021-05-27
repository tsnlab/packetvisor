#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct pv_packet {
    uint32_t start;
    uint32_t end;
    uint32_t size;
    uint8_t* buffer;
    uint16_t nic_id;
    uint16_t queue_id;
    void* priv;
};

struct pv_packet* pv_packet_alloc();
void pv_packet_free(struct pv_packet* packet);

#ifdef __cplusplus
}
#endif
