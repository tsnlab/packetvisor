#include <stdio.h>

#include <pv/pv.h>
#include <pv/packet.h>
#include "internal_nic.h"
#include "internal_config.h"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_lcore.h>

#define MAX_ARGC 128

int pv_init(int argc, char** argv) {
	struct pv_config* config = pv_config_create();
	if(config == NULL) {
		printf("Faild to parse config\n");
		return -1;
	}
	pv_config_print(config);

	int new_argc = argc;
	char* new_argv[MAX_ARGC] = {};
	memcpy(new_argv, argv, argc * sizeof(char*));

	// create core mask parameter 
	uint64_t core_mask[2] = {};
	char core_mask_buf[40] = {};

	for(int i = 0; i < config->cores_count; i++) {
		if(config->cores[i] < 0 || config->cores[i] > 127) {
			printf("Core index must be between 0~127.\n");
			pv_config_destroy(config);
			return -1;
		}
		core_mask[config->cores[i] / 128] |= (1 << (config->cores[i] % 128));
	}
	snprintf(core_mask_buf, sizeof(core_mask_buf), "0x%lx%lx", core_mask[1], core_mask[0]);
	printf("coremask : %s\n", core_mask_buf);

	new_argv[new_argc++] = "-c";
	new_argv[new_argc++] = core_mask_buf;
	for(int i = 0; i < new_argc; i++) {
	printf("argv[%d]: %s\n", i, new_argv[i]);
		}

	int ret = rte_eal_init(new_argc, new_argv);
	if(ret < 0) {
		pv_config_destroy(config);
		return ret;
	}

	// init packet_pool
	uint16_t priv_size = sizeof(struct pv_packet);
	priv_size = RTE_ALIGN(priv_size, RTE_MBUF_PRIV_ALIGN);
	struct rte_mempool* mbuf_pool = rte_pktmbuf_pool_create("PV", config->memory.packet_pool, 250, priv_size, RTE_MBUF_DEFAULT_BUF_SIZE, SOCKET_ID_ANY);
	if(mbuf_pool == NULL) {
		printf("Failed to create packet pool\n");
		pv_config_destroy(config);
		return -1;
	}

	// TODO create global shared memory

	// init nics
	ret = pv_nic_init(config->nics_count, mbuf_pool);
	if(ret != 0) {
		printf("Failed to init nics\n");
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
}

int pv_start_core(unsigned int core_id, pv_core_function_t* func, void* context) {
	return rte_eal_remote_launch(func, context, core_id);
}

int pv_wait_core(unsigned int core_id) {
	return rte_eal_wait_lcore(core_id);
}

void pv_debug() {
	printf("nic count: %d\n", rte_eth_dev_count_avail());
	printf("core count: %d\n", rte_lcore_count());
}
