#pragma once

#include <stdexcept>
#include <byteswap.h>
#include <pv/packet.h>

namespace pv {

#define endian8(v)	(v)
#define endian16(v)	bswap_16((v))
#define endian32(v)	bswap_32((v))
#define endian48(v)	(bswap_64((v)) >> 16)
#define endian64(v)	bswap_64((v))

class Protocol {
protected:
	Packet*		packet;
	uint32_t	offset;

	void	 	CHECK(uint32_t pos, uint32_t size) const;
	uint8_t* 	OFFSET(uint32_t pos) const;

public:
	Protocol();
	Protocol(Packet* packet, uint32_t offset);
	virtual ~Protocol();

	Packet* getPacket();
	Protocol* setPacket(Packet* packet);

	uint32_t getOffset();
	Protocol* setOffset(uint32_t offset);
};

}; // namespace pv
