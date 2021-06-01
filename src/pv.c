#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include "zf_log.h"
#include <pv/config.h>
#include <pv/pv.h>

#include <cl/collection.h>
#include <cl/map.h>

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static const struct rte_eth_conf port_conf_default = {
    .rxmode =
        {
            .max_rx_pkt_len = RTE_ETHER_MAX_LEN,
        },
};

struct map* create_devmap();

void destroy_devmap(struct map* devmap);

static inline int port_init(uint16_t port, struct rte_mempool* mbuf_pool, struct pv_config_nic* nic_config) {
    struct rte_eth_conf port_conf = port_conf_default;
    // FIXME: Implement multiple rx/tx queue
    const uint16_t rx_rings = 1, tx_rings = 1;
    uint16_t nb_rxd = nic_config->rx_queue;
    uint16_t nb_txd = nic_config->tx_queue;
    int retval;
    uint16_t q;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_txconf txconf;

    if(!rte_eth_dev_is_valid_port(port)) {
        return 1;
    }

    retval = rte_eth_dev_info_get(port, &dev_info);
    if(retval != 0) {
        ZF_LOGE("Error during getting device (port %u) info: %s\n", port, strerror(-retval));
        return retval;
    }

    if(dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE) {
        port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;
    }

    retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
    if(retval != 0) {
        return retval;
    }

    retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
    if(retval != 0) {
        return retval;
    }

    for(q = 0; q < rx_rings; q += 1) {
        retval = rte_eth_rx_queue_setup(port, q, nb_rxd, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
        if(retval < 0) {
            return retval;
        }
    }

    txconf = dev_info.default_txconf;
    txconf.offloads = port_conf.txmode.offloads;

    for(q = 0; q < tx_rings; q += 1) {
        retval = rte_eth_tx_queue_setup(port, q, nb_txd, rte_eth_dev_socket_id(port), &txconf);
        if(retval < 0) {
            return retval;
        }
    }

    retval = rte_eth_dev_start(port);
    if(retval < 0) {
        return retval;
    }

    struct rte_ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    if(retval != 0) {
        return retval;
    }

    ZF_LOGI("Port %u MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", port, addr.addr_bytes[0], addr.addr_bytes[1],
            addr.addr_bytes[2], addr.addr_bytes[3], addr.addr_bytes[4], addr.addr_bytes[5]);

    retval = rte_eth_promiscuous_enable(port);
    if(retval != 0) {
        return retval;
    }

    return 0;
}

int pv_init() {
    struct rte_mempool* mbuf_mempool;
    unsigned int nb_ports;

    struct pv_config* config = pv_config_create();
    if(config == NULL) {
        ZF_LOGE("ERRORRERRORR");
    }

    char* argv[] = {
        "xxx",
    };
    int argc = sizeof(argv) / sizeof(argv[0]);

    int ret = rte_eal_init(argc, argv);
    if(ret < 0) {
        rte_exit(1, "Error with EAL initialization\n");
    }

    nb_ports = rte_eth_dev_count_avail();
    ZF_LOGI("Available ports: %d\n", nb_ports);
    ZF_LOGI("Available cores: %d\n", rte_lcore_count());

    mbuf_mempool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0,
                                           RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if(mbuf_mempool == NULL) {
        rte_exit(1, "Cannot create mbuf pool\n");
    }

    struct map* dev_map = create_devmap();

    for(int i = 0; i < config->nics_count; i += 1) {
        if(!map_has(dev_map, config->nics[i].dev)) {
            ZF_LOGE("Cannot found portid for dev %s", config->nics[i].dev);
            return -1;
        }

        uint16_t portid = to_i16(map_get(dev_map, config->nics[i].dev));
        port_init(portid, mbuf_mempool, &config->nics[i]);
    }

    destroy_devmap(dev_map);

    uint16_t port;

    RTE_ETH_FOREACH_DEV(port) {
        if(rte_eth_dev_socket_id(port) > 0 && rte_eth_dev_socket_id(port) != (int)rte_socket_id()) {
            ZF_LOGW("WARNING, port %u is on remote NUMA node to "
                    "polling thread.\n\tPerformance will "
                    "not be optimal.\n",
                    port);
        }
    }

    pv_config_destroy(config);
    return ret;
}

void pv_finalize() {
}

struct map* create_devmap() {
    struct map* map = map_create(10, string_hash, string_compare);
    assert(map != NULL);

    uint16_t portid;

    RTE_ETH_FOREACH_DEV(portid) {
        struct rte_eth_dev_info devinfo;
        if(rte_eth_dev_info_get(portid, &devinfo) != 0) {
            rte_exit(1, "Cannot get devinfo");
        }

        map_put(map, strdup(devinfo.device->name), from_u16(portid));
    }

    return map;
}

void destroy_devmap(struct map* devmap) {
    assert(devmap != NULL);

    struct map_iterator iter;

    map_iterator_init(&iter, devmap);
    while(map_iterator_has_next(&iter)) {
        struct map_entry* entry = map_iterator_next(&iter);
        free(entry->key);
    }

    map_destroy(devmap);
}
