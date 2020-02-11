#include <stdio.h>
#include <iostream>
#include <arpa/inet.h>

#include <pv/packet.h>
#include <pv/protocol.h>
#include <pv/ipv4.h>

#include "interface.hpp"
#include "router.hpp"
#include "lb.hpp"

static LB* lb;

extern "C" {
pv::Packetlet* pv_packetlet(int argc, char** argv) {
	// Parsing configuration
    lb = new LB();
	if(lb == NULL)
		exit(1);

	in_addr address;
	inet_pton(AF_INET, argv[1], &address.s_addr);

	in_addr netmask;
	inet_pton(AF_INET, argv[2], &netmask.s_addr);

	in_addr gateway;
	inet_pton(AF_INET, argv[3], &gateway.s_addr);

	Interface* interface = new Interface(endian32(address.s_addr), endian32(netmask.s_addr), endian32(gateway.s_addr));
	Router::addInterface(interface);

	if(argc >= 5 && !strcmp(argv[4], "1")) { // TestCode
		in_addr service_address;
		inet_pton(AF_INET, "192.168.0.200", &service_address.s_addr);

		Service* service = LB::addService(LB_SERVICE_ALGORITHM_RR, endian32(service_address.s_addr), IP_PROTOCOL_UDP, 7);
		if(service == NULL) {
			exit(1);
		}

		in_addr server_addr;
		inet_pton(AF_INET, "10.0.0.200", &server_addr.s_addr);
		
		if(!service->addServer(endian32(server_addr.s_addr), 8080, LB_SERVER_MODE_NAT)) {
			exit(1);
		}

		std::cout << "done" << std::endl;
	}

	return interface;
}
} // extern "C"