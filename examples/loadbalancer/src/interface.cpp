#include <pv/ethernet.h>
#include <pv/arp.h>
#include <pv/ipv4.h>
#include <pv/icmp.h>

#include "interface.hpp"
#include "lb.hpp"

using namespace pv;

Interface::Interface(uint32_t address, uint32_t netmask, uint32_t gateway): Packetlet(), m_address(address), m_netmask(netmask), m_gateway(gateway) {
	std::cout << "New Interface" << std::endl;
	std::cout << "  addr: " << address << std::endl;
	std::cout << "  netmask: " << netmask << std::endl;
	std::cout << "  gw: "<< gateway << std::endl;
}

void Interface::init(Driver* driver) {
	Packetlet::init(driver);
}

uint32_t Interface::getAddress() {
    return m_address;
}

uint32_t Interface::getNetmask() {
    return m_netmask;
}

uint32_t Interface::getGateway() {
    return m_gateway;
}

uint64_t Interface::getMAC() {
    return driver->getMAC();
}

bool Interface::checkDestination(uint32_t address) {
	if(address == m_address)
		return true;
	else
		return false;
}

bool Interface::ARPProcess(Packet* packet) {
	Ethernet* ether = new Ethernet(packet);
	ARP* arp = new ARP(ether);
	std::cout << arp << std::endl;

	uint32_t dstAddress = arp->getDstProto();
	if(arp->getOpcode() == 1) {
		ether->setDst(ether->getSrc())
				->setSrc(driver->getMAC());
		
		arp->setOpcode(2)
			->setDstHw(arp->getSrcHw())
			->setDstProto(arp->getSrcProto())
			->setSrcHw(driver->getMAC())
			->setSrcProto(dstAddress);

		if(!send(packet)) {
			fprintf(stderr, "Cannot reply arp\n");
			return false;
		} else {
			return true;
		}
	} else if(arp->getOpcode() == 2) {
		std::cout << "Update ARP Table: " << std::endl;
		// update arp table
		uint64_t smac = arp->getSrcHw();
		uint32_t sip = arp->getSrcProto();

		//TODO: check insert result
		arp_table.insert({sip, smac});
		return true;
	}

	return false;
}

void Interface::sendARPRequest(uint32_t address) {
	printf("Send ARP Request...\n");
	Packet* packet = alloc();
	if(packet == NULL)
		return;

	Ethernet* ether = new Ethernet(packet);
	ether->setDst(0xffffffffffff);
	ether->setSrc(driver->getMAC());
	ether->setType(ETHER_TYPE_ARP);

	ARP* arp = new ARP(ether);
	arp->setHwType(1);
	arp->setProtoType(0x0800);
	arp->setHwSize(6);
	arp->setProtoSize(4);
	arp->setOpcode(1);
	arp->setSrcHw(driver->getMAC());
	arp->setSrcProto(m_address);
	arp->setDstHw(0);
	arp->setDstProto(address);

	packet->end = ETHER_LEN + ARP_LEN;
	// packet->queueId = id;

	send(packet);
}

uint64_t Interface::getDestHWAddr(uint32_t address) {
	// TODO: garbage collection

	auto it = arp_table.find(address);
	if(it == arp_table.end()) {
		// send arp request
		sendARPRequest(address);

		return 0xffffffffffff;
	} else {
		return it->second;
	}
}

bool Interface::ICMPProcess(Packet* packet) {
	Ethernet* ether = new Ethernet(packet);
	IPv4* ipv4 = new IPv4(ether);
	ICMP* icmp = new ICMP(ipv4);
	std::cout << icmp << std::endl;

	uint32_t dstAddress = ipv4->getDst();
	if(icmp->getType() == 8 && checkDestination(dstAddress)) {
		icmp->setType(0) // ICMP reply
			->setChecksum(0)
			->setChecksum(icmp->checksum(0, icmp->getEnd() - icmp->getOffset()));
		
		ipv4->setDst(ipv4->getSrc())
			->setSrc(dstAddress)
			->setTtl(64)
			->setDf(false)
			->setId(ipv4->getId() + 1)
			->setChecksum(0)
			->checksum();

		ether->setDst(ether->getSrc())
				->setSrc(driver->getMAC());

		if(!send(packet)) {
			fprintf(stderr, "Cannot reply icmp\n");
			return false;
		} else {
			return true;
		}
	}

	return false;
}

bool Interface::received(Packet* packet) {
	std::cout << "recieved: "<< std::endl;
	std::cout << packet << std::endl;

	Ethernet* ether = new Ethernet(packet);
	std::cout << ether << std::endl;

	if(ether->getType() == ETHER_TYPE_ARP) {
		return ARPProcess(packet);
	} else if(ether->getType() == ETHER_TYPE_IPv4) {
		IPv4* ipv4 = new IPv4(ether);
		std::cout << ipv4 << std::endl;

		if(ipv4->getProto() == IP_PROTOCOL_ICMP && checkDestination(ipv4->getDst())) {
			return ICMPProcess(packet);
		} else {
			if(LB::process(packet))
				return send(packet);
			else
				return false;
		}
	}

	return false;
}