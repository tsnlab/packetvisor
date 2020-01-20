#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>
#include <time.h>
#include <sys/resource.h>
#include <pv/driver.h>
#include "config.h"
#include "driver.h"

#include <common/common_user_bpf_xdp.h>

uint64_t sleep_delay = 2000000;	// 2 secs
uint64_t sleep_steps[] = { 0, 10, 50, 100, 500 };	// 0 ~ 500us
int sleep_step_count = sizeof(sleep_steps) / sizeof(uint64_t);
int sleep_step = 0;

static bool is_running = true;

static void on_interrupt(__attribute__((unused)) int signal) {
	is_running = false;
}

static uint64_t get_us() {
	struct timespec ts;
	clock_gettime(0, &ts);

	return 1000000 * ts.tv_sec  + ts.tv_nsec / 1000;
}

int main(int argc, char** argv) {
	int count = pv::Config::parse(argc, argv);
	if(count < 0) {
		pv::Config::usage();
		return 0;
	}

	argc -= count;
	argv += count;

	// Load packetvisor
	void* handle = dlopen("libpv.so", RTLD_LAZY);
    if(!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    dlerror();
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

	// Load packetets
	for(int i = 0; i < argc; i++) {
		printf("Loading packetlet: %s\n", argv[i]);
		callback->load(argv[i]);
	}

	/* Receive and count packets than drop them */
	printf("Packetvisor started on device %s, address is %02lx:%02lx:%02lx:%02lx:%02lx:%02lx\n", 
			pv::Config::ifname, 
			(pv::Config::mac >> 40) & 0xff,
			(pv::Config::mac >> 32) & 0xff,
			(pv::Config::mac >> 24) & 0xff,
			(pv::Config::mac >> 16) & 0xff,
			(pv::Config::mac >> 8) & 0xff,
			(pv::Config::mac >> 0) & 0xff);

	uint64_t base = get_us();
	bool isInSleep = false;
	while(is_running) {
		if(driver->loop()) {
			isInSleep = false;
		} else {
			if(!isInSleep) {
				isInSleep = true;
				base = get_us();
				sleep_step = 0;
			} else if(isInSleep) {
				uint64_t current = get_us();

				// change sleep step
				if(current - base > sleep_delay) {
					base = current;

					if(++sleep_step >= sleep_step_count)
						sleep_step--;
				}

				if(sleep_step == 0) {
					asm volatile ("nop"::);
					asm volatile ("nop"::);
					asm volatile ("nop"::);
					asm volatile ("nop"::);
					asm volatile ("nop"::);
				} else {
					usleep(sleep_steps[sleep_step]);
				}
			}
		}
	}

	delete driver;
	destroy(callback);

    dlclose(handle);

	return 0;
}
