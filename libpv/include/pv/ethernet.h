#pragma once

#include <pv/protocol.h>

namespace pv {

#define ETHER_LEN			14
#define ETHER_TYPE_IPv4		0x0800		///< Ether type of IPv4
#define ETHER_TYPE_ARP		0x0806		///< Ether type of ARP
#define ETHER_TYPE_IPv6		0x86dd		///< Ether type of IPv6
#define ETHER_TYPE_LLDP		0x88cc		///< Ether type of LLDP
#define ETHER_TYPE_8021Q	0x8100		///< Ether type of 802.1q
#define ETHER_TYPE_8021AD	0x88a8		///< Ether type of 802.1ad

class Ethernet : public Protocol {
public:
	Ethernet(Packet* packet);
	virtual ~Ethernet();

	uint64_t getDst() const;
	Ethernet* setDst(uint64_t mac);

	uint64_t getSrc() const;
	Ethernet* setSrc(uint64_t mac);

	uint16_t getType() const;
	Ethernet* setType(uint16_t type);

	uint32_t getPayloadOffset() const;

	friend std::ostream& operator<<(std::ostream& out, const Ethernet& obj);
	friend std::ostream& operator<<(std::ostream& out, const Ethernet* obj);
};

}; // namespace pv
