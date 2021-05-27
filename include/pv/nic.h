#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <pv/packet.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t pv_nic_count();
uint64_t pv_nic_get_mac(uint16_t nic_id);
bool pv_nic_get_promiscuous(uint16_t nic_id);
int pv_nic_set_promiscuous(uint16_t nic_id, bool mode);
uint16_t pv_nic_rx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** array, uint16_t count);
uint16_t pv_nic_tx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** array, uint16_t count);

#ifdef __cplusplus
}
#endif
