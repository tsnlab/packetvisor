#include <stdio.h>

#include <pv/pv.h>
#include <pv/packet.h>
#include "internal_core.h"
#include "internal_nic.h"
#include "internal_config.h"

#include <rte_eal.h>
#include <rte_ethdev.h>

int pv_init(void) {
	struct pv_config* config = pv_config_create();
	if(config == NULL) {
		printf("Faild to parse config\n");
		return -1;
	}
	
	// call rte_eal_init. rte_eal_init reorder argv internally, use copy of eal_params
	void* argv = calloc(config->eal_params_count, sizeof(char*));
	if(argv == NULL) {
		printf("Failed to alloc memory for pv_argv\n");
		pv_config_destroy(config);
		return -1;
	}
	memcpy(argv, config->eal_params, sizeof(char*) * config->eal_params_count);

	int ret = rte_eal_init(config->eal_params_count, argv);
	if(ret < 0) {
		pv_config_destroy(config);
		free(argv);
		return ret;
	}
	free(argv);


	// init packet_pool
	uint16_t priv_size = sizeof(struct pv_packet) + 128 /* padding */;
	priv_size = RTE_ALIGN(priv_size, RTE_MBUF_PRIV_ALIGN);
	struct rte_mempool* mbuf_pool = rte_pktmbuf_pool_create("PV", config->memory.packet_pool, 250, priv_size, RTE_MBUF_DEFAULT_BUF_SIZE, SOCKET_ID_ANY);
	if(mbuf_pool == NULL) {
		printf("Failed to create packet pool\n");
		pv_config_destroy(config);
		return -1;
	}

	// TODO create global shared memory

	// TODO set log level

	// init cores
	if(pv_core_init(config->cores, config->cores_count) == false) {
		printf("Failed to init cores\n");
		pv_config_destroy(config);
		return ret;
	}

	// init nics
	ret = pv_nic_init(config->nics_count, mbuf_pool);
	if(ret != 0) {
		printf("Failed to init nics\n");
		pv_config_destroy(config);
		return ret;
	}

	// TODO multi rx/tx queue setup(3,4th argument)
	for(uint16_t nic_id = 0; nic_id < config->nics_count; nic_id++) {
		struct pv_config_nic* nic = &config->nics[nic_id];
		ret = pv_nic_add(nic_id, nic->dev, 1, 1, nic->rx_queue, nic->tx_queue, nic->rx_offloads, nic->tx_offloads);
		if(ret != 0) {
			printf("Failed to init nic(%d)\n", nic_id);
			pv_config_destroy(config);
			return ret;
		}

		if(nic->is_mac_valid)
			pv_nic_set_mac(nic_id, nic->mac);

		if(nic->is_ipv4_valid)
			pv_nic_set_ipv4(nic_id, nic->ipv4);

		// TODO ipv6

		if(pv_nic_set_promisc(nic_id, nic->promisc) == false) {
			printf("Failed to set promisc nic(%d)\n", nic_id);
			pv_config_destroy(config);
			return ret;
		}
	}

	for(uint16_t nic_id = 0; nic_id < config->nics_count; nic_id++) {
		ret = pv_nic_start(nic_id);
		if(ret != 0) {
			printf("Failed to start nic(%d)\n", nic_id);
			pv_config_destroy(config);
			return ret;
		}
	}

	pv_config_destroy(config);

	return 0;
}

void pv_finalize(void) {
	pv_nic_finalize();
	pv_core_finalize();
}
