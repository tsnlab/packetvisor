#include <stdio.h>
#include <iostream>
#include <arpa/inet.h>
#include <pugixml.hpp>

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

	if(argc == 5) {
		pugi::xml_document doc;
		pugi::xml_parse_result result = doc.load_file(argv[4]);
		if(result) {
			// Parse service
			for(pugi::xml_node node = doc.child("loadbalancer").child("service"); node; node = node.next_sibling("service")) {
				std::string address = node.child("address").text().as_string();
				std::string _protocol = node.child("protocol").text().as_string();
				std::string _port = node.child("port").text().as_string();
				std::string _schedule = node.child("schedule").text().as_string();

				LB_SERVICE_ALGORITHM algorithm;
				if(_schedule.compare("rr") == 0) {
					algorithm = LB_SERVICE_ALGORITHM_RR;
				} else {
					exit(1);
				}

				in_addr service_address;
				inet_pton(AF_INET, address.c_str(), &service_address.s_addr);

				uint8_t protocol;
				if(_protocol.compare("udp") == 0) {
					_protocol = IP_PROTOCOL_UDP;
				} else if(_protocol.compare("tcp") == 0) {
					protocol = IP_PROTOCOL_TCP;
				} else {
					std::cout << "Wrong ip protocol: " << _protocol << std::endl;
					exit(1);
				}

				uint16_t port = strtoul(_port.c_str(), NULL, 10);

				Service* service = LB::addService(algorithm, endian32(service_address.s_addr), protocol, port);
				if(service == NULL) {
					std::cout << "Can't parse service" << std::endl;
					exit(1);
				}

				// Parse server
				for(pugi::xml_node server_node = node.child("servers").child("server"); server_node; server_node = server_node.next_sibling("server")) {
					std::string _server_address = server_node.child("address").text().as_string();
					std::string _server_port = server_node.child("port").text().as_string();
					std::string _server_mode = server_node.child("mode").text().as_string();

					in_addr server_address;
					inet_pton(AF_INET, _server_address.c_str(), &server_address.s_addr);

					uint16_t server_port = strtoul(_server_port.c_str(), NULL, 10);

					LB_SERVER_MODE server_mode;
					if(_server_mode.compare("nat") == 0) {
						server_mode = LB_SERVER_MODE_NAT;
					} else {
						std::cout << "Wrong mode: " << _server_mode << std::endl;
						exit(1);
					}

					if(!service->addServer(endian32(server_address.s_addr), server_port, server_mode)) {
						exit(1);
					}
				}
			}
		} else {
			std::cout << "Can't parse loadbalancer configuration file" << std::endl;
			exit(1);
		}
	}

	return interface;
}
} // extern "C"