#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <cl/map.h>

struct pv_config_memory {
    uint32_t packet_pool;   // Count
    uint32_t shared_memory; // Bytes
};

struct pv_config_nic {
    char* dev;     // *required* pci port num 0000:00:08.0
    uint64_t mac;  // c0:ff:ee:c0:ff:ee or NULL
    uint32_t ipv4; // 192.168.0.1, NULL if auto
    // char* ipv6;  // ::1, NULL if auto
    uint16_t rx_queue;
    uint16_t tx_queue;
    uint32_t rx_offloads;
    uint32_t tx_offloads;
    bool promisc;
};

enum pv_config_loglevel {
    PV_LOGLEVEL_VERBOSE,
    PV_LOGLEVEL_DEBUG,
    PV_LOGLEVEL_INFO,
    PV_LOGLEVEL_WARNING,
    PV_LOGLEVEL_ERROR,
};

struct pv_config {
    uint16_t* cores;
    uint16_t cores_count;

    struct pv_config_memory memory;

    struct pv_config_nic* nics;
    uint16_t nics_count;

    uint8_t loglevel; // See pv_config_loglevel

    struct map* config_map;
};

struct pv_config* pv_config_create();
void pv_config_destroy(struct pv_config* config);
