#include <rte_ethdev.h>

#include <pv/nic.h>

#include "internal.h"

uint16_t pv_nic_count() {
    return rte_eth_dev_count_avail();
}

uint64_t pv_nic_get_mac(uint16_t nic_id) {
    struct rte_ether_addr addr;
    int ret = rte_eth_macaddr_get(nic_id, &addr);
    if(ret != 0) {
        return 0;
    }

    return ((uint64_t)addr.addr_bytes[0]) << 40 | ((uint64_t)addr.addr_bytes[1]) << 32 |
           ((uint64_t)addr.addr_bytes[2]) << 24 | ((uint64_t)addr.addr_bytes[3]) << 16 |
           ((uint64_t)addr.addr_bytes[4]) << 8 | ((uint64_t)addr.addr_bytes[5]) << 0;
}

bool pv_nic_is_promiscuous(uint16_t nic_id) {
    return rte_eth_promiscuous_enable(nic_id) == 1;
}

int pv_nic_set_promiscuous(uint16_t nic_id, bool mode) {
    if(mode) {
        return rte_eth_promiscuous_enable(nic_id);
    } else {
        return rte_eth_promiscuous_disable(nic_id);
    }
}

uint16_t pv_nic_rx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** array, uint16_t count) {
    struct rte_mbuf** mbufs = (struct rte_mbuf**)array;
    const uint16_t nb_rx = rte_eth_rx_burst(nic_id, queue_id, mbufs, count);

    for(uint16_t i = 0; i < nb_rx; i++) {
        array[i] = _pv_mbuf_to_packet(mbufs[i], nic_id, queue_id);
    }

    return nb_rx;
}

uint16_t pv_nic_tx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** array, uint16_t count) {
    struct rte_mbuf** mbufs = (struct rte_mbuf**)array;
    for(uint16_t i = 0; i < count; i += 1) {
        mbufs[i] = array[i]->priv;
    }

    const uint16_t nb_tx = rte_eth_tx_burst(nic_id, queue_id, mbufs, count);
    return nb_tx;
}

bool pv_nic_tx(uint16_t nic_id, uint16_t queue_id, struct pv_packet* packet) {
    struct pv_packet* array[1] = {packet};
    return pv_nic_tx_burst(nic_id, queue_id, array, 1) == 1;
}
