#ifndef __PV_INTERNAL_NIC_H__
#define __PV_INTERNAL_NIC_H__

#include <stdint.h>
#include <stdbool.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>

struct pv_offload_type {
    char* name;
    uint32_t mask;
};

int pv_nic_init(uint16_t nic_count, struct rte_mempool* mbuf_pool);

void pv_nic_finalize(void);

int pv_nic_add(uint16_t nic_id, char* dev_name, uint16_t nb_rx_queue, uint16_t nb_tx_queue, uint16_t rx_queue_size, uint16_t tx_queue_size, uint32_t rx_offloads, uint32_t tx_offloads);

bool pv_nic_set_mac(uint16_t nic_id, uint64_t mac_addr);

bool pv_nic_set_ipv4(uint16_t nic_id, uint32_t ipv4_addr);

bool pv_nic_set_promisc(uint16_t nic_id, bool enable);

int pv_nic_start(uint16_t nic_id);

#endif /* __PV_INTERNAL_NIC_H__ */
