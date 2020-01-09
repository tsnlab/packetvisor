#pragma once

#include <iostream>
#include <pv/packet.h>

namespace pv {

#define IP_PROTOCOL_ICMP	0x01	///< IP protocol number for ICMP
#define IP_PROTOCOL_IP		0x04	///< IP protocol number for IP
#define IP_PROTOCOL_UDP		0x11	///< IP protocol number for UDP
#define IP_PROTOCOL_TCP		0x06	///< IP protocol number for TCP
#define IP_PROTOCOL_ESP		0x32	///< IP protocol number for ESP
#define IP_PROTOCOL_AH		0x33	///< IP protocol number for AH

class IPv4 {
protected:
	Packet*		packet;
	uint32_t	offset;

public:
	IPv4(Packet* packet, uint32_t offset);
	virtual ~IPv4();

	uint8_t getVersion() const;
	IPv4* setVersion(uint8_t version);

	uint8_t getHdrLen() const;
	IPv4* setHdrLen(uint8_t hdr_len);

	uint8_t getDscp() const;
	IPv4* setDscp(uint8_t dscp);

	uint8_t getEcn() const;
	IPv4* setEcn(uint8_t ecn);

	uint16_t getLen() const;
	IPv4* setLen(uint16_t len);

	uint16_t getId() const;
	IPv4* setId(uint16_t id);

	uint8_t getFlags() const;
	IPv4* setFlags(uint8_t flags);

	bool isRb() const;
	IPv4* setRb(bool rb);

	bool isDf() const;
	IPv4* setDf(bool df);

	bool isMf() const;
	IPv4* setMf(bool mf);

	uint16_t getFragOffset() const;
	IPv4* setFragOffset(uint16_t frag_offset);

	uint8_t getTtl() const;
	IPv4* setTtl(uint8_t ttl);

	uint8_t getProto() const;
	IPv4* setProto(uint8_t proto);

	uint16_t getChecksum() const;
	IPv4* setChecksum(uint16_t checksum);

	uint32_t getSrc() const;
	IPv4* setSrc(uint32_t src);

	uint32_t getDst() const;
	IPv4* setDst(uint32_t dst);
	
	uint32_t getBodyOffset() const;

	friend std::ostream& operator<<(std::ostream& out, const IPv4& obj);
	friend std::ostream& operator<<(std::ostream& out, const IPv4* obj);
};

}; // namespace pv
