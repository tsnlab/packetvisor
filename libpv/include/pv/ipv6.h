#pragma once

#include <pv/protocol.h>

namespace pv {

#define IP6_LEN	40

#define IP_PROTOCOL_ICMP6	0x3A	///< IPv6 next header number for ICMPv6

struct ip6_addr {
	union {
		uint8_t addr8[16];
		uint16_t addr16[8];
		uint32_t addr32[4];
		uint64_t addr64[2];
	} u;
};

struct IPv6_Pseudo {
	struct ip6_addr source;
	struct ip6_addr destination;
	uint32_t upper_len;
	uint8_t zero[3];	// Reserved, fill zero
	uint8_t next_hdr;
} __attribute__((packed));

class IPv6 : public Protocol {
public:
	IPv6(Packet* packet, uint32_t offset);
	IPv6(Protocol* parent);
	virtual ~IPv6();

	uint8_t getVersion() const;
	IPv6* setVersion(uint8_t version);

	uint8_t getTrafficClass() const;
	IPv6* setTrafficClass(uint8_t traffic_class);

	uint32_t getFlowLabel() const;
	IPv6* setFlowLabel(uint32_t flow_label);

	uint16_t getPayLen() const;
	IPv6* setPayLen(uint16_t pay_len);

	uint8_t getNextHdr() const;
	IPv6* setNextHdr(uint8_t next_hdr);

	uint8_t getHopLimit() const;
	IPv6* setHopLimit(uint8_t hop_limit);

	struct ip6_addr getSrc() const;
	IPv6* setSrc(struct ip6_addr src);

	struct ip6_addr getDst() const;
	IPv6* setDst(struct ip6_addr dst);
	
	uint16_t getPseudoChecksum() const;

	virtual uint32_t getBodyOffset() const;

	friend std::ostream& operator<<(std::ostream& out, const IPv6& obj);
	friend std::ostream& operator<<(std::ostream& out, const IPv6* obj);
};

}; // namespace pv
