#include <stdio.h>
#include <iostream>
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
	EchoPacketlet();
	bool received(Packet* packet);
};

EchoPacketlet::EchoPacketlet() : Packetlet() {
}

bool EchoPacketlet::received(Packet* packet) {
	std::cout << packet << std::endl;

	uint32_t myip = 0xc0a80001;

	Ethernet* ether = new Ethernet(packet);
	std::cout << ether << std::endl;

	if(ether->getType() == ETHER_TYPE_ARP) {
		ARP* arp = new ARP(ether);
		std::cout << arp << std::endl;

		if(arp->getOpcode() == 1 && arp->getDstProto() == myip) {
			printf("Sending ARP...\n");
			ether->setDst(ether->getSrc())
			     ->setSrc(driver->mac);
			
			arp->setOpcode(2)
			   ->setDstHw(arp->getSrcHw())
			   ->setDstProto(arp->getSrcProto())
			   ->setSrcHw(driver->mac)
			   ->setSrcProto(myip);

			if(!send(packet)) {
				fprintf(stderr, "Cannot reply arp\n");
				return false;
			} else {
				return true;
			}
		}

		return false;
	} else if(ether->getType() == ETHER_TYPE_IPv4) {
		IPv4* ipv4 = new IPv4(ether);
		std::cout << ipv4 << std::endl;

		if(ipv4->getProto() == IP_PROTOCOL_ICMP && ipv4->getDst() == myip) {
			ICMP* icmp = new ICMP(ipv4);
			std::cout << icmp << std::endl;

			if(icmp->getType() == 8) {
				printf("Sending ICMP...\n");
				icmp->setType(0) // ICMP reply
					->setChecksum(0)
					->setChecksum(icmp->checksum(0, icmp->getEnd() - icmp->getOffset()));
				
				ipv4->setDst(ipv4->getSrc())
				    ->setSrc(myip)
				    ->setTtl(64)
				    ->setDf(false)
				    ->setId(ipv4->getId() + 1)
				    ->setChecksum(0)
				    ->setChecksum(ipv4->checksum(0, ipv4->getHdrLen() * 4));

				ether->setDst(ether->getSrc())
				     ->setSrc(driver->mac);

				for(uint32_t i = packet->start; i < packet->end; i++) {
					printf("%02x ", packet->payload[i]);
				}
				printf("\n");

				if(!send(packet)) {
					fprintf(stderr, "Cannot reply icmp\n");
					return false;
				} else {
					return true;
				}
			}
		} else if(ipv4->getProto() == IP_PROTOCOL_UDP && ipv4->getDst() == myip) {
			UDP* udp = new UDP(ipv4);
			std::cout << udp << std::endl;

			if(udp->getDstport() == 7) {
				printf("UDP echo...\n");
				udp->setDstport(udp->getSrcport())
				   ->setSrcport(7)
				   ->checksum();

				ether->setDst(ether->getSrc())
				     ->setSrc(driver->mac);

				if(!send(packet)) {
					fprintf(stderr, "Cannot reply udp echo\n");
					return false;
				} else {
					return true;
				}
			}
		}
	}

	return false;
}

extern "C" {

Packetlet* pv_packetlet() {
	return new EchoPacketlet();
}

} // extern "C"
