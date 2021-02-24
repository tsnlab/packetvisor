#ifndef __PV_INTERNAL_CONFIG_H__
#define __PV_INTERNAL_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

struct pv_config_memory {
	uint32_t packet_pool;
	uint32_t shared_memory;
};

struct pv_config_nic {
	bool is_mac_valid;
	bool is_ipv4_valid;
	bool is_ipv6_valid;

	char* dev;
	uint64_t mac;
	uint32_t ipv4;
	char* ipv6; // TODO

	uint32_t rx_queue;
	uint32_t tx_queue;
	bool promisc;

	uint32_t rx_offloads;
	uint32_t tx_offloads;
};

struct pv_config {
	uint32_t* cores;
	uint32_t cores_count;

	struct pv_config_memory memory;

	char* log_level;

	struct pv_config_nic* nics;
	int nics_count;

	char** eal_params;
	int eal_params_count;
};

struct pv_config* pv_config_create();

void pv_config_destroy(struct pv_config* config);

void pv_config_print(struct pv_config* config);

#endif /* __PV_INTERNAL_CONFIG_H__ */
