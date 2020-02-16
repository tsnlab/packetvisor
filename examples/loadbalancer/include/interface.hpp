#ifndef __INTERFACE_HPP__
#define __INTERFACE_HPP__

#include <stdint.h>
#include <tbb/concurrent_unordered_map.h>

#include <pv/driver.h>
#include <pv/packet.h>

#include "arp.hpp"

class Interface: public pv::Packetlet {
public:
	Interface(uint32_t address, uint32_t netmask, uint32_t gateway);
	bool received(pv::Packet* packet);

	void init(pv::Driver* driver);

    uint32_t getAddress();
    uint32_t getNetmask();
    uint32_t getGateway();
    uint64_t getMAC();
	uint64_t getDestHWAddr(uint32_t address);

private:
	uint32_t m_address{0};
	uint32_t m_netmask{0};
	uint32_t m_gateway{0};

	bool checkDestination(uint32_t address);
	bool ARPProcess(pv::Packet* packet);
	bool ICMPProcess(pv::Packet* packet);
	void sendARPRequest(uint32_t address);

	tbb::concurrent_unordered_map<uint32_t, ARPEntry> arp_table;
};

#endif /*__INTERFACE_HPP__*/