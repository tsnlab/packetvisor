#pragma once

#include <pv/protocol.h>

namespace pv {

#define ICMP_LEN				4
#define ICMP_TYPE_ECHO_REPLY	0	///< ICMP type, echo reply
#define ICMP_TYPE_ECHO_REQUEST	8	///< ICMP type, echo request

class ICMP : public Protocol {
public:
	ICMP(Packet* packet, uint32_t offset);
	ICMP(Protocol* parent);
	virtual ~ICMP();

	uint8_t getType() const;
	ICMP* setType(uint8_t type);

	uint8_t getCode() const;
	ICMP* setCode(uint8_t code);

	uint16_t getChecksum() const;
	ICMP* setChecksum(uint16_t checksum);

	uint32_t getBodyOffset() const;

	friend std::ostream& operator<<(std::ostream& out, const ICMP& obj);
	friend std::ostream& operator<<(std::ostream& out, const ICMP* obj);
};

}; // namespace pv
