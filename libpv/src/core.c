#include <pv/core.h>
#include "internal_core.h"

#include <rte_lcore.h>

uint32_t* cores;
uint32_t cores_count;

bool pv_core_init(uint32_t* config_cores, uint32_t config_cnt) {
	uint32_t ncore = rte_lcore_count();
	if(ncore < 1)
		return false;

	if(ncore < config_cnt) {
		printf("Too many user config cores. user_config: %u, detected: %u\n", config_cnt, ncore);
		return false;
	}

	for(int i = 0; i < ncore; i++) {
		if(config_cores[i] > ncore) {
			printf("Invalid core index '%u'\n", config_cores[i]);
			return false;
		}
	}
	
	cores = calloc(config_cnt, sizeof(uint32_t));
	if(cores == NULL)
		return false;

	memcpy(cores, config_cores, config_cnt * sizeof(uint32_t));
	cores_count = config_cnt;

	return true;
}

void pv_core_finalize() {
	free(cores);
	cores_count = 0;
}

uint32_t pv_core_count() {
	return cores_count;
}

int pv_core_start(uint32_t core_id, pv_core_function_t* func, void* context) {
	return rte_eal_remote_launch(func, context, cores[core_id]);
}

int pv_core_wait(uint32_t core_id) {
	return rte_eal_wait_lcore(cores[core_id]);
}
