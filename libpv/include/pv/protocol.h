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
	Protocol*	parent;
	Packet*		packet;
	uint32_t	offset;

	void	 	CHECK(uint32_t pos, uint32_t size) const;
	uint8_t* 	OFFSET(uint32_t pos) const;

public:
	Protocol(Packet* packet, uint32_t offset);
	Protocol(Protocol* parent);
	virtual ~Protocol();

	Packet* getPacket() const;
	Protocol* setPacket(Packet* packet);

	uint32_t getStart() const;
	Protocol* setStart(uint32_t start);

	uint32_t getEnd() const;
	Protocol* setEnd(uint32_t end);

	uint32_t getSize() const;
	Protocol* setSize(uint32_t size);

	uint32_t getOffset() const;
	Protocol* setOffset(uint32_t offset);

	Protocol* getParent() const;

	virtual uint32_t getBodyOffset() const;

	static uint16_t checksum(uint8_t* payload, uint32_t offset, uint32_t size);
	uint16_t checksum(uint32_t offset, uint32_t size);
};

}; // namespace pv
