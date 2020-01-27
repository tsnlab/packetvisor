#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <iostream>
#include <arpa/inet.h>
#include <pv/packet.h>
#include <pv/driver.h>
#include <pv/ethernet.h>
#include <pv/arp.h>
#include <pv/ipv4.h>
#include <pv/icmp.h>
#include <pv/udp.h>
#include <pv/tcp.h>
#include <sys/socket.h>

using namespace pv;

class ForwardPacketlet: public Packetlet {
protected:
	uint32_t addr;

	uint32_t id;
	uint64_t mac;

public:
	ForwardPacketlet(uint32_t addr, uint32_t id);
	bool received(Packet* packet);
	void init(Driver* driver);
};

ForwardPacketlet::ForwardPacketlet(uint32_t addr, uint32_t id) : Packetlet() {
	this->addr = addr;
	this->id = id;
}

void ForwardPacketlet::init(Driver* driver) {
	Packetlet::init(driver);

	mac = driver->getMAC(id);
	std::cout << "ForwardPacketlet passing NIC: " << id << std::endl;
}

bool ForwardPacketlet::received(Packet* packet) {
	std::cout << packet << std::endl;

	Ethernet* ether = new Ethernet(packet);
	std::cout << ether << std::endl;

	if(ether->getType() == ETHER_TYPE_ARP) {
		ARP* arp = new ARP(ether);
		std::cout << arp << std::endl;

		if(arp->getOpcode() == 1 && arp->getDstProto() == addr) {
			std::cout << "Sending ARP..." << std::endl;

			ether->setDst(ether->getSrc())
			     ->setSrc(mac);
			
			arp->setOpcode(2)
			   ->setDstHw(arp->getSrcHw())
			   ->setDstProto(arp->getSrcProto())
			   ->setSrcHw(mac)
			   ->setSrcProto(addr);

			if(!send(packet)) {
				std::cerr << "Cannot reply arp" << std::endl;
			}

			return true;
		}
	} else if(ether->getType() == ETHER_TYPE_IPv4) {
		IPv4* ipv4 = new IPv4(ether);
		std::cout << ipv4 << std::endl;

		if(ipv4->getDst() == addr) {
			if(ipv4->getProto() == IP_PROTOCOL_ICMP) {
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
						->checksum();

					ether->setDst(ether->getSrc())
						 ->setSrc(mac);

					if(!send(packet)) {
						std::cerr << "Cannot reply icmp" << std::endl;
					}

					return true;
				}
			} else if(ipv4->getProto() == IP_PROTOCOL_UDP) {
				UDP* udp = new UDP(ipv4);
				std::cout << udp << std::endl;

				char buf[5];
				sprintf(buf, "%04x", ipv4->getId());
				std::cout << "Forwarding UDP(" << buf << ") to NIC #" << std::to_string(id) << " " << std::to_string(ipv4->getLen() - ipv4->getHdrLen() * 4 - UDP_LEN) << " bytes" << std::endl;

				packet->queueId = id;
				udp->checksum();

				if(!send(packet)) {
					std::cerr << "Cannot forward udp packet" << std::endl;
				}

				return true;
			} else if(ipv4->getProto() == IP_PROTOCOL_TCP) {
				TCP* tcp = new TCP(ipv4);
				std::cout << tcp << std::endl;

				char buf[5];
				sprintf(buf, "%04x", ipv4->getId());
				std::cout << "Forwarding TCP(" << buf << ") to NIC #" << std::to_string(id) << " " << std::to_string(ipv4->getLen() - ipv4->getHdrLen() * 4 - TCP_LEN) << " bytes" << std::endl;

				packet->queueId = id;
				tcp->checksum();

				if(!send(packet)) {
					std::cerr << "Cannot forward udp packet" << std::endl;
				}

				return true;
			}
		}
	}

	return false;
}

extern "C" {

Packetlet* pv_packetlet(int argc, char** argv) {
	if(argc < 3) {
		std::cerr << "argv[1] must be an ip address and argv[2] must be a NIC ID, argc is " << std::to_string(argc) << std::endl;
		return nullptr;
	} else {
		in_addr addr;
		inet_pton(AF_INET, argv[1], &addr.s_addr);
		uint32_t id = atoi(argv[2]);
		return new ForwardPacketlet(endian32(addr.s_addr), id);
	}
}

} // extern "C"
