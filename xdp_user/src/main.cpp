#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>
#include <time.h>
#include <pthread.h>
#include <sys/resource.h>
#include <pv/driver.h>
#include "config.h"
#include "driver.h"

#include <common/common_user_bpf_xdp.h>

static uint64_t get_us() {
	struct timespec ts;
	clock_gettime(0, &ts);

	return 1000000 * ts.tv_sec  + ts.tv_nsec / 1000;
}

static bool is_running = true;

static void on_interrupt(__attribute__((unused)) int signal) {
	is_running = false;
}

static void* run(void* ctx) {
	uint64_t sleep_delay = 2000000;	// 2 secs
	uint64_t sleep_steps[] = { 0, 10, 50, 100, 500 };	// 0 ~ 500us
	int sleep_step_count = sizeof(sleep_steps) / sizeof(uint64_t);
	int sleep_step = 0;

	pv::XDPDriver* driver = (pv::XDPDriver*)ctx;
	printf("XDPDriver for dev %s is started...\n", driver->ifname.c_str());

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

	printf("XDPDriver for dev %s is stopped...\n", driver->ifname.c_str());

	return nullptr;
}

int main(int argc, char** argv) {
	int ret = pv::Config::parse(argc, argv);
	if(ret <= 0)
		return ret;

	argc -= ret;
	argv += ret;

	// Load libpv
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

	/* Allow unlimited locking of memory, so all memory needed for packet
	 * buffers can be locked.
	 */
	struct rlimit rlim = { RLIM_INFINITY, RLIM_INFINITY };
	if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
		fprintf(stderr, "ERROR: setrlimit(RLIMIT_MEMLOCK) \"%s\"\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	}

	uint32_t idx = 0;
	uint32_t size = pv::Config::xdp.size();
	pv::XDPDriver* drivers[size];
	pv::Callback* callbacks[size];
	pthread_t threads[size];
	for(auto it = pv::Config::xdp.begin(); it != pv::Config::xdp.end(); it++) {
		std::string dev = it->first;
		pv::XDPConfig* config = it->second;

		printf("Loading xdp driver for %s\n", config->ifname.c_str());

		// Load xdp program into kernel
		printf("\tLoading xdp program into kernel: %s\n", config->ifname.c_str());
		char sec[32];
		snprintf(sec, 32, "%s", config->xdp_section.c_str());
		struct bpf_object* bpf_obj = load_bpf_and_xdp_attach(
				config->xdp_flags, config->ifindex, 
				config->xdp_file.c_str(),
				sec, 32);
		if(bpf_obj == nullptr) {
			/* Error handling done in load_bpf_and_xdp_attach() */
			exit(EXIT_FAILURE);
		}
		printf("\txdp program loaded into a section named: %s\n", sec);

		// Load xsks_map into kernel
		printf("\tLoading xsks_map into kernel\n");
		struct bpf_map* map = bpf_object__find_map_by_name(bpf_obj, "xsks_map");
		int xsks_map_fd = bpf_map__fd(map);
		if (xsks_map_fd < 0) {
			fprintf(stderr, "ERROR: no xsks map found: %s\n",
				strerror(xsks_map_fd));
			exit(EXIT_FAILURE);
		}

		// Create driver
		printf("\tCreating XDP driver\n");
		pv::XDPDriver* driver;
		try {
			driver = new pv::XDPDriver(config);
			driver->id = idx + 1;
			drivers[idx] = driver;
		} catch(const std::string& msg) {
			fprintf(stderr, "ERROR: Cannot create XDPDriver: %s\n", msg.c_str());
			exit(1);
		}

		// Register driver and get callback
		printf("\tRegister callback to XDP driver\n");
		pv::Callback* callback = init(driver);
		if(callback == NULL) {
			fprintf(stderr, "packetvisor init is failed\n");
			exit(1);
		}
		driver->setCallback(callback);
		callbacks[idx] = callback;

		printf("\tMAC address is %02lx:%02lx:%02lx:%02lx:%02lx:%02lx\n", 
				(config->mac >> 40) & 0xff,
				(config->mac >> 32) & 0xff,
				(config->mac >> 24) & 0xff,
				(config->mac >> 16) & 0xff,
				(config->mac >> 8) & 0xff,
				(config->mac >> 0) & 0xff);

		idx++;
	}

	idx = 0;
	for(auto it = pv::Config::xdp.begin(); it != pv::Config::xdp.end(); it++) {
		std::string dev = it->first;
		pv::XDPConfig* config = it->second;
		pv::Callback* callback = callbacks[idx];

		printf("Loading packetlet for %s(count=%lu)\n", dev.c_str(), config->packetlets.size());
		for(pv::PacketletInfo* info: config->packetlets) {
			printf("\tLoading: %s ", info->path.c_str());
			char* argv[info->args.size() + 1];
			argv[0] = strdup(info->path.c_str());
			for(unsigned int i = 0; i < info->args.size(); i++) {
				argv[i + 1] = strdup(info->args[i].c_str());
				printf("%s ", argv[i + 1]);
			}
			printf("\n");

			callback->load(info->path.c_str(), (int)info->args.size() + 1, argv);

			for(unsigned int i = 0; i < info->args.size() + 1; i++) {
				free(argv[i]);
			}
		}

		idx++;
	}

	/* Global shutdown handler */
	signal(SIGINT, on_interrupt);

	for(uint32_t i = 0; i < size; i++) {
		pthread_create(&threads[i], NULL, run, drivers[i]);
	}

	// Destroy driver and callback
	for(uint32_t i = 0; i < size; i++) {
		pthread_join(threads[i], NULL);

		destroy(callbacks[i]);
		delete drivers[i];
	}

    dlclose(handle);

	pv::Config::destroy();

	return 0;
}
