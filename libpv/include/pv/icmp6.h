#pragma once

#include <pv/protocol.h>
#include <pv/ipv6.h>

namespace pv {

#define ICMP6_LEN	4

#define ICMP6_TYPE_ECHO_REQUEST	128	///< ICMP6 type, echo request
#define ICMP6_TYPE_ECHO_REPLY	129	///< ICMP6 type, echo reply
#define ICMP6_TYPE_NS	135	///< ICMP6 type, neighbor solicitation
#define ICMP6_TYPE_NA	136	///< ICMP6 type, neighbor advertisement

#define ICMP6_OPT_TYPE_SOURCE_LINK_ADDR	1	///< ICMP6 option type, source link-layer address
#define ICMP6_OPT_TYPE_TARGET_LINK_ADDR	2	///< ICMP6 option type, target link-layer address

class ICMP6 : public Protocol {
public:
	ICMP6(Packet* packet, uint32_t offset);
	ICMP6(Protocol* protocol);
	virtual ~ICMP6();

	uint8_t getType() const;
	ICMP6* setType(uint8_t type);

	uint8_t getCode() const;
	ICMP6* setCode(uint8_t code);

	uint16_t getChecksum() const;
	ICMP6* setChecksum(uint16_t checksum);
	ICMP6* checksum();

	uint32_t getBodyOffset() const;

	uint8_t getRFlag() const;	///< for TYPE_NA
	ICMP6* setRFlag(uint8_t r);

	uint8_t getSFlag() const;	///< for TYPE_NA
	ICMP6* setSFlag(uint8_t s);

	uint8_t getOFlag() const;	///< for TYPE_NA
	ICMP6* setOFlag(uint8_t o);

	struct in6_addr getTarget() const;	///< for TYPE_NA, NS
	ICMP6* setTarget(struct in6_addr target);

	uint8_t getOptLen() const;	///< for TYPE_NA, NS
	ICMP6* setOptLen(uint8_t len);

	uint8_t getOptType() const;	///< for TYPE_NA, NS
	ICMP6* setOptType(uint8_t type);

	uint64_t getOptLinkAddr() const;	///< for TYPE_NA, NS
	ICMP6* setOptLinkAddr(uint64_t link_addr);

	uint16_t getId() const;	///< for TYPE_ECHO_REQ, RES
	ICMP6* setId(uint16_t id);

	uint16_t getSeq() const;	///< for TYPE_ECHO_REQ, RES
	ICMP6* setSeq(uint16_t seq);

	friend std::ostream& operator<<(std::ostream& out, const ICMP6& obj);
	friend std::ostream& operator<<(std::ostream& out, const ICMP6* obj);
};

}; // namespace pv
