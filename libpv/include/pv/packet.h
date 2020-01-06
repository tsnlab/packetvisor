#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <byteswap.h>

namespace pv {

#define endian8(v)	(v)
#define endian16(v)	bswap_16((v))
#define endian32(v)	bswap_32((v))
#define endian48(v)	(bswap_64((v)) >> 16)
#define endian64(v)	bswap_64((v))

class Packet;

class Packetlet {
protected:
	int32_t id;

public:
	Packetlet();
	virtual ~Packetlet();

	virtual bool received(Packet* packet);
	void send(Packet* packet);
};

class Packet {
public:
	int32_t		queueId;
	uint8_t*	payload;
	uint32_t	start;
	uint32_t	end;
	uint32_t	size;

	Packet(uint32_t size);
	Packet(int32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
	virtual ~Packet();

	friend std::ostream& operator<<(std::ostream& out, const Packet& obj);
	friend std::ostream& operator<<(std::ostream& out, const Packet* obj);
};

}; // namespace pv
