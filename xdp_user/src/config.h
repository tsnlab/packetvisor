#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <net/if.h>

namespace pv {

class Config {
public:
	static uint64_t	mac;

	static uint32_t	xdp_flags;
	static uint16_t	xsk_flags;

	static int		xsk_if_queue;
	static bool		is_xsk_polling;
	static int		queue;

	static int		ifindex;
	static char		ifname[IF_NAMESIZE];

	static char		xdp_file[256];
	static char		xdp_section[32];
	static char		pcap_path[256];

	static int parse(int argc, char** argv);
	static void usage();
};

}; // namespace pv
