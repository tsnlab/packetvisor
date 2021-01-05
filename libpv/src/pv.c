#include <pv/pv.h>
#include "internal_nic.h"

#include <rte_eal.h>

int pv_init(int argc, char** argv) {
	int ret = rte_eal_init(argc, argv);
	if(ret < 0)
		return ret;

	

}

void pv_finalize(void) {

}
