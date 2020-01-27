#include <stdlib.h>
#include <iostream>
#include <arpa/inet.h>
#include <pv/packet.h>
#include <pv/driver.h>
#include <pv/ethernet.h>
#include <pv/arp.h>
#include <pv/ipv4.h>
#include <pv/icmp.h>
#include <pv/udp.h>

using namespace pv;

class EchoPacketlet: public Packetlet {
public:
	uint32_t addr;
	EchoPacketlet(uint32_t addr);

	void init(Driver* driver);
	bool received(Packet* packet);
};

EchoPacketlet::EchoPacketlet(uint32_t addr) : Packetlet() {
	this->addr = addr;
}

void EchoPacketlet::init(Driver* driver) {
	Packetlet::init(driver);

	std::cout << "EchoPacketlet starting at " << 
		std::to_string((addr >> 24) & 0xff) << "." <<
		std::to_string((addr >> 16) & 0xff) << "." <<
		std::to_string((addr >> 8) & 0xff) << "." <<
		std::to_string((addr >> 0) & 0xff) << std::endl;
}

bool EchoPacketlet::received(Packet* packet) {
	std::cout << packet << std::endl;

	Ethernet* ether = new Ethernet(packet);
	std::cout << ether << std::endl;

	if(ether->getType() == ETHER_TYPE_ARP) {
		ARP* arp = new ARP(ether);
		std::cout << arp << std::endl;

		if(arp->getOpcode() == 1 && arp->getDstProto() == addr) {
			std::cout << "Sending ARP..." << std::endl;

			ether->setDst(ether->getSrc())
			     ->setSrc(driver->getMAC());
			
			arp->setOpcode(2)
			   ->setDstHw(arp->getSrcHw())
			   ->setDstProto(arp->getSrcProto())
			   ->setSrcHw(driver->getMAC())
			   ->setSrcProto(addr);

			if(!send(packet)) {
				std::cerr << "Cannot reply arp" << std::endl;
			}

			return true;
		}
	} else if(ether->getType() == ETHER_TYPE_IPv4) {
		IPv4* ipv4 = new IPv4(ether);
		std::cout << ipv4 << std::endl;

		if(ipv4->getProto() == IP_PROTOCOL_ICMP && ipv4->getDst() == addr) {
			ICMP* icmp = new ICMP(ipv4);
			std::cout << icmp << std::endl;

			if(icmp->getType() == 8) {
				std::cout << "Sending ICMP..." << std::endl;

				icmp->setType(0) // ICMP reply
					->setChecksum(0)
					->setChecksum(icmp->checksum(0, icmp->getEnd() - icmp->getOffset()));
				
				ipv4->setDst(ipv4->getSrc())
				    ->setSrc(addr)
				    ->setTtl(64)
				    ->setDf(false)
				    ->setId(ipv4->getId() + 1)
				    ->setChecksum(0)
					->checksum();

				ether->setDst(ether->getSrc())
				     ->setSrc(driver->getMAC());

				if(!send(packet)) {
					std::cerr << "Cannot reply icmp" << std::endl;
				}

				return true;
			}
		} else if(ipv4->getProto() == IP_PROTOCOL_UDP && ipv4->getDst() == addr) {
			UDP* udp = new UDP(ipv4);
			std::cout << udp << std::endl;

			if(udp->getDstport() == 7) {
				std::cout << "UDP echo..." << std::endl;

				udp->setDstport(udp->getSrcport())
				   ->setSrcport(7)
				   ->checksum();

				ether->setDst(ether->getSrc())
				     ->setSrc(driver->getMAC());

				if(!send(packet)) {
					std::cerr << "Cannot reply udp echo" << std::endl;
				}

				return true;
			}
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
		in_addr addr;
		inet_pton(AF_INET, argv[1], &addr.s_addr);
		return new EchoPacketlet(endian32(addr.s_addr));
	}
}

} // extern "C"
