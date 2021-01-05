#ifndef __PV_INTERNAL_NIC_H__
#define __PV_INTERNAL_NIC_H__

#include <stdint.h>
#include <stdbool.h>

bool pv_nic_set_id_by_name(char* dev_name, uint16_t nic_id);

int pv_nic_init_rx(uint16_t nic_id, uint16_t nb_rx_queue, uint16_t rx_queue_size);
int pv_nic_init_tx(uint16_t nic_id, uint16_t nb_tx_queue, uint16_t tx_queue_size);

int pv_nic_set_mac(uint16_t nic_id, uint64_t mac);
bool pv_nic_set_promisc(uint16_t nic_id, bool enable)

int pv_nic_start(uint16_t nic_id);

#endif /* __PV_INTERNAL_NIC_H__ */
