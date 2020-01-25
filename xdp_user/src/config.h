#pragma once

#include <stdint.h>
#include <stdbool.h>
#undef max
#undef min 
#include <map>
#include <vector>
#include <net/if.h>

namespace pv {

class UmemConfig {
public:
	UmemConfig();
	virtual ~UmemConfig();

	uint32_t num_frames;
};

struct PacketletInfo {
	std::string					path;
	std::vector<std::string>	args;
};

class XDPConfig {
public:
	XDPConfig(const std::string& dev);
	virtual ~XDPConfig();

	uint64_t	mac;

	uint32_t	xdp_flags;
	uint16_t	xsk_flags;

	int			xsk_if_queue;
	bool		is_xsk_polling;
	int			queue;

	int			ifindex;
	std::string	ifname;			// max size: IF_NAME_SIZE in net/if.h

	std::string	xdp_file;		// max size: 256
	std::string	xdp_section;	// max size: 32
	std::string	pcap_path;		// max size: 256

	std::vector<PacketletInfo*>	packetlets;
};

class Config {
private:
public:
	static UmemConfig* umem;
	static std::map<std::string, XDPConfig*> xdp;

	static bool parse();
	static void destroy();
};

}; // namespace pv
