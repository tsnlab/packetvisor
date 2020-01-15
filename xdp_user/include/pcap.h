#pragma once

#include <stdint.h>

namespace pv {

class Pcap {
protected:
	int			fd;
	char		path[256];
	uint8_t		buf[2048];
	uint32_t	start;
	uint32_t	end;

public:
	Pcap(const char* path);
	virtual ~Pcap();

	int32_t received(uint8_t* payload, uint32_t len);
	int32_t send(uint8_t* payload, uint32_t len);
};

}; // namespace pv
