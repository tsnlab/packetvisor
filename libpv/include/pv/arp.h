#pragma once

#include <pv/protocol.h>

namespace pv {

#define ARP_LEN					28

class ARP : public Protocol {
public:
	ARP(Packet* packet, uint32_t offset);
	ARP(Protocol* parent);
	virtual ~ARP();

	uint16_t getHwType() const;
	ARP* setHwType(uint16_t type);

	uint16_t getProtoType() const;
	ARP* setProtoType(uint16_t type);

	uint8_t getHwSize() const;
	ARP* setHwSize(uint8_t size);

	uint8_t getProtoSize() const;
	ARP* setProtoSize(uint8_t size);

	uint16_t getOpcode() const;
	ARP* setOpcode(uint16_t opcode);

	uint64_t getSrcHw() const;
	ARP* setSrcHw(uint64_t addr);

	uint32_t getSrcProto() const;
	ARP* setSrcProto(uint32_t addr);

	uint64_t getDstHw() const;
	ARP* setDstHw(uint64_t addr);

	uint32_t getDstProto() const;
	ARP* setDstProto(uint32_t addr);

	friend std::ostream& operator<<(std::ostream& out, const ARP& obj);
	friend std::ostream& operator<<(std::ostream& out, const ARP* obj);
	friend std::ostream& operator<<(std::ostream& out, const ARP* obj);
};

}; // namespace pv

