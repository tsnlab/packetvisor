#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <dlfcn.h>
#include <sys/resource.h>
#include <pv/driver.h>
#include "config.h"
#include "driver.h"

#include <common/common_user_bpf_xdp.h>

static bool is_running = true;

static void on_interrupt(__attribute__((unused)) int signal) {
	is_running = false;
}

int main(int argc, char** argv) {
	if(pv::Config::parse(argc, argv) < 0) {
		pv::Config::usage();
		return 0;
	}

	// Load packetvisor
	void* handle = dlopen("libpv.so", RTLD_LAZY);
    if(!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }
    dlerror();    /* Clear any existing error */

	char* error;
	pv::Init init = (pv::Init)dlsym(handle, "pv_init");
    if((error = dlerror()) != NULL) {
        fprintf(stderr, "%s\n", error);
        exit(1);
    }

	pv::Destroy destroy = (pv::Destroy)dlsym(handle, "pv_destroy");
    if((error = dlerror()) != NULL) {
        fprintf(stderr, "%s\n", error);
        exit(1);
    }

	/* Global shutdown handler */
	signal(SIGINT, on_interrupt);

	/* Load custom program if configured */
	if(pv::Config::xdp_file[0] != 0) {
		struct bpf_object* bpf_obj = load_bpf_and_xdp_attach(pv::Config::xdp_flags, pv::Config::ifindex, pv::Config::xdp_file, sizeof(pv::Config::xdp_file), pv::Config::xdp_section, sizeof(pv::Config::xdp_section));
		if (!bpf_obj) {
			/* Error handling done in load_bpf_and_xdp_attach() */
			exit(EXIT_FAILURE);
		}

		/* We also need to load the xsks_map */
		struct bpf_map* map = bpf_object__find_map_by_name(bpf_obj, "xsks_map");
		int xsks_map_fd = bpf_map__fd(map);
		if (xsks_map_fd < 0) {
			fprintf(stderr, "ERROR: no xsks map found: %s\n",
				strerror(xsks_map_fd));
			exit(EXIT_FAILURE);
		}
	}

	/* Allow unlimited locking of memory, so all memory needed for packet
	 * buffers can be locked.
	 */
	struct rlimit rlim = { RLIM_INFINITY, RLIM_INFINITY };
	if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
		fprintf(stderr, "ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	pv::XDPDriver* driver = new pv::XDPDriver();
	pv::Callback* callback = init(driver);
	if(callback == NULL) {
		fprintf(stderr, "packetvisor init is failed\n");
		exit(1);
	}

	driver->setCallback(callback);

	/* Receive and count packets than drop them */
	printf("Packetvisor started on device %s, address is %02lx:%02lx:%02lx:%02lx:%02lx:%02lx\n", 
			pv::Config::ifname, 
			(pv::Config::mac >> 40) & 0xff,
			(pv::Config::mac >> 32) & 0xff,
			(pv::Config::mac >> 24) & 0xff,
			(pv::Config::mac >> 16) & 0xff,
			(pv::Config::mac >> 8) & 0xff,
			(pv::Config::mac >> 0) & 0xff);
	while(is_running) {
		if(!driver->loop())
			usleep(10);
	}

	delete driver;
	destroy(callback);

    dlclose(handle);

	return 0;
}
