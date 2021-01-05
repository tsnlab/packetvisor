#pragma once

#include <pv/protocol.h>

namespace pv {

#define UDP_LEN					8

class UDP : public Protocol {
public:
	UDP(Packet* packet, uint32_t offset);
	UDP(Protocol* parent);
	virtual ~UDP();

	uint16_t getSrcport() const;
	UDP* setSrcport(uint16_t port);

	uint16_t getDstport() const;
	UDP* setDstport(uint16_t port);

	uint16_t getLength() const;
	UDP* setLength(uint16_t length);

	uint16_t getChecksum() const;
	UDP* setChecksum(uint16_t checksum);
	UDP* checksumWithPseudo(uint16_t pseudo_checksum);
	UDP* checksum();

	uint32_t getBodyOffset() const;

	friend std::ostream& operator<<(std::ostream& out, const UDP& obj);
	friend std::ostream& operator<<(std::ostream& out, const UDP* obj);
};

}; // namespace pv
