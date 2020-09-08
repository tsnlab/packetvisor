#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include <pv/packet.h>
#include <pv/driver.h>
#include <pv/ethernet.h>
#include <pv/arp.h>
#include <pv/ipv6.h>
#include <pv/icmp6.h>
#include <pv/udp.h>

#include <stdio.h>

using namespace pv;

class EchoPacketlet: public Packetlet {
public:
	struct pv::in6_addr addr;
	EchoPacketlet(struct pv::in6_addr addr);

	void init(Driver* driver);
	bool received(Packet* packet);
};

EchoPacketlet::EchoPacketlet(struct pv::in6_addr addr) : Packetlet() {
	this->addr = addr;
}

void EchoPacketlet::init(Driver* driver) {
	Packetlet::init(driver);

	char buf[40] = {};
	sprintf(buf, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", 
			endian16(addr.u.addr16[0]), endian16(addr.u.addr16[1]), 
			endian16(addr.u.addr16[2]), endian16(addr.u.addr16[3]),
			endian16(addr.u.addr16[4]), endian16(addr.u.addr16[5]),
			endian16(addr.u.addr16[6]), endian16(addr.u.addr16[7]));

	std::cout << "EchoPacketlet starting at " << buf << std::endl;
}

bool EchoPacketlet::received(Packet* packet) {
	std::cout << packet << std::endl;

	Ethernet* ether = new Ethernet(packet);
	std::cout << ether << std::endl;

	if(ether->getType() != ETHER_TYPE_IPv6)
		return false;

	IPv6* ipv6 = new IPv6(ether);
	std::cout << ipv6 << std::endl;

	if(ipv6->getNextHdr() == IP_PROTOCOL_ICMP6) {
		if(memcmp(&ipv6->getDst().u.addr8[13], &addr.u.addr8[13], 3) != 0)
			return false;

		ICMP6* icmp6 = new ICMP6(ipv6);
		std::cout << icmp6 << std::endl;

		if(icmp6->getType() == ICMP6_TYPE_ECHO_REQUEST) {
			icmp6->setType(ICMP6_TYPE_ECHO_REPLY)
				 ->setChecksum(0);

			ipv6->setTrafficClass(0)
				->setFlowLabel(0)
				->setNextHdr(IP_PROTOCOL_ICMP6)
				->setHopLimit(64)
				->setDst(ipv6->getSrc())
				->setSrc(addr);

			ether->setDst(ether->getSrc())
				 ->setSrc(driver->getMAC());
			
			icmp6->checksum();

			if(!send(packet)) {
				std::cerr << "Cannot reply icmp6 echo request" << std::endl;
			}
			return true;
		
		} else if(icmp6->getType() == ICMP6_TYPE_NS) {
			uint64_t smac = icmp6->getOptLinkAddr();

			icmp6->setType(ICMP6_TYPE_NA)
				 ->setChecksum(0)
				 ->setRFlag(0)
				 ->setSFlag(1)
				 ->setOFlag(1)
				 ->setOptType(ICMP6_OPT_TYPE_TARGET_LINK_ADDR)
				 ->setOptLinkAddr(driver->getMAC());

			ipv6->setTrafficClass(0)
				->setFlowLabel(0)
				->setPayLen(32)
				->setNextHdr(IP_PROTOCOL_ICMP6)
				->setHopLimit(255)
				->setDst(ipv6->getSrc())
				->setSrc(addr);

			ether->setDst(smac)
				 ->setSrc(driver->getMAC());
			
			icmp6->checksum();

			if(!send(packet)) {
				std::cerr << "Cannot reply icmp6 neighbor solicitation" << std::endl;
			}
			return true;
		}
	}

	return false;
}

extern "C" {

Packetlet* pv_packetlet(int argc, char** argv) {
	if(argc < 2) {
		std::cerr << "argv[1] must be an ip address, argc is " << std::to_string(argc) << std::endl;
		return nullptr;
	} else {
		struct pv::in6_addr addr = {};
		inet_pton(AF_INET6, argv[1], &addr);
		return new EchoPacketlet(addr);
	}
}

} // extern "C"
