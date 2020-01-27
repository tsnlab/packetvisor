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
#include <sys/socket.h>

using namespace pv;

class PassPacketlet: public Packetlet {
protected:
	uint32_t addr;

	uint32_t id;
	uint64_t mac;

public:
	PassPacketlet(uint32_t addr, uint32_t id);
	bool received(Packet* packet);
	void init(Driver* driver);
};

PassPacketlet::PassPacketlet(uint32_t addr, uint32_t id) : Packetlet() {
	this->addr = addr;
	this->id = id;
}

void PassPacketlet::init(Driver* driver) {
	Packetlet::init(driver);

	mac = driver->getMAC(id);
	std::cout << "PassPacketlet passing NIC: " << id << std::endl;
}

bool PassPacketlet::received(Packet* packet) {
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
			     //->setSrc(driver->getMAC());
			
			arp->setOpcode(2)
			   ->setDstHw(arp->getSrcHw())
			   ->setDstProto(arp->getSrcProto())
			   ->setSrcHw(mac)
			   //->setSrcHw(driver->getMAC())
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

				static int counter = 0;
				Packet* packet2 = new Packet(0, driver->alloc(), packet->start, packet->end, packet->size);
				packet2->queueId = 2;
				memcpy(packet2->payload + packet2->start, packet->payload + packet->start, packet->end - packet->start);
				Ethernet* ether2 = new Ethernet(packet2);
				IPv4* ip2 = new IPv4(ether2);
				ip2->setProto(17);
				ip2->checksum();
				UDP* udp = new UDP(ip2);
				udp->setSrcport(2000);
				udp->setDstport(2000);
				udp->setChecksum(0);
				udp->setLength(56);
				bzero(packet2->payload + udp->getBodyOffset(), packet2->end - udp->getBodyOffset());
				sprintf((char*)packet2->payload + udp->getBodyOffset(), "\nHello World to 2 %d\n\n", counter++);
				std::cout << "packet2" << std::endl;
				std::cout << packet2 << std::endl;
				std::cout << ether2 << std::endl;
				std::cout << ip2 << std::endl;
				std::cout << udp << std::endl;
				for(unsigned int i = packet2->start; i < packet2->end; i++) {
					printf("%02x ", packet2->payload[packet2->start + i]);
				}
				printf("\n");

				printf("packet2: %p\n", packet2);
				if(!send(packet2)) {
					std::cerr << "Cannot send dup packet" << std::endl;
				}

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
				     //->setSrc(driver->getMAC());

				/*
				Packet* packet3 = new Packet(0, driver->alloc(), packet->start, packet->end, packet->size);
				packet3->queueId = 1;
				memcpy(packet3->payload + packet3->start, packet->payload + packet->start, packet->end - packet->start);
				Ethernet* ether3 = new Ethernet(packet3);
				IPv4* ip3 = new IPv4(ether3);
				ip3->setProto(17);
				ip3->checksum();
				UDP* udp3 = new UDP(ip3);
				udp3->setSrcport(2000);
				udp3->setDstport(2000);
				udp3->setChecksum(0);
				udp3->setLength(56);
				bzero(packet3->payload + udp3->getBodyOffset(), packet3->end - udp3->getBodyOffset());
				sprintf((char*)packet3->payload + udp3->getBodyOffset(), "\nHello World to 1 %d\n\n", counter++);
				std::cout << "packet3" << std::endl;
				std::cout << ether3 << std::endl;
				std::cout << ip3 << std::endl;
				std::cout << udp3 << std::endl;

				printf("packet3: %p\n", packet3);
				if(!send(packet3)) {
					std::cerr << "Cannot send dup packet" << std::endl;
				}
				*/

				printf("packet: %p\n", packet);
				if(!send(packet)) {
					std::cerr << "Cannot reply icmp" << std::endl;
				}

				return true;
			}
		} else if(ipv4->getProto() == IP_PROTOCOL_UDP && ipv4->getDst() == addr) {
			UDP* udp = new UDP(ipv4);
			std::cout << udp << std::endl;
		}
	}

	//ether->setSrc(mac);
	std::cout << "Forwarding..." << std::endl;
	std::cout << ether << std::endl;
	IPv4* ipv4 = new IPv4(ether);
	std::cout << ipv4 << std::endl;
	UDP* udp = new UDP(ipv4);
	udp->setChecksum(0);
	std::cout << udp << std::endl;

	Packet* packet2 = new Packet(0, driver->alloc(), packet->start, packet->end, packet->size);
	packet2->queueId = id;
	memcpy(packet2->payload + packet2->start, packet->payload + packet->start, packet->end - packet->start);

	std::cout << "forwarding copy\n";
	std::cout << packet2 << std::endl;

	for(unsigned int i = packet2->start; i < packet2->end; i++) {
		printf("%02x ", packet2->payload[packet2->start + i]);
	}
	printf("\n");

	if(!send(packet2)) {
		std::cerr << "Cannot forward packet" << std::endl;
	}

	packet->queueId = id;

	std::cout << "forwarding original\n";
	std::cout << packet << std::endl;

	for(unsigned int i = packet->start; i < packet->end; i++) {
		printf("%02x ", packet->payload[packet->start + i]);
	}
	printf("\n");

	if(!send(packet)) {
		std::cerr << "Cannot forward packet" << std::endl;
	}

	return true;
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
		return new PassPacketlet(endian32(addr.s_addr), id);
	}
}

} // extern "C"
