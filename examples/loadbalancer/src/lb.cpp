#include <stdlib.h>
#include <sys/shm.h>

#include <tbb/concurrent_unordered_map.h>

#include <pv/ethernet.h>
#include <pv/ipv4.h>
#include <pv/icmp.h>
#include <pv/udp.h>
#include <pv/tcp.h>

#include "lb.hpp"
#include "router.hpp"

#define LB_SHARED_MEMORY_KEY        0127

using namespace pv;

static tbb::concurrent_unordered_map<uint64_t, Service*> service_table;

LB::LB() {
}

LB::~LB() {
}

Service* LB::addService(enum LB_SERVICE_ALGORITHM algo, uint32_t address, uint8_t protocol, uint16_t port) {
    Service* lb_service = new Service(algo, address, protocol, port);

    uint64_t hash_key = ((uint64_t)address << 32) | ((uint64_t)protocol << 16) | (uint64_t)port; // [    address(32)    ][(8)][protocol(8)][port(16)]

    //add service to table
    auto ret = service_table.insert({hash_key, lb_service});
    if(!ret.second) {
        delete lb_service;
        return NULL;
    }

    return lb_service;
}

Service* LB::getService(uint32_t address, uint8_t protocol, uint16_t port) {
    uint64_t hash_key = ((uint64_t)address << 32) | ((uint64_t)protocol << 16) | (uint64_t)port;

    tbb::concurrent_unordered_map<uint64_t, Service*>::iterator it = service_table.find(hash_key);
    if(it == service_table.end())
        return NULL;

    return it->second;
}

Service* LB::findServiceBySession(uint32_t address, uint8_t protocol, uint16_t port) {
  tbb::concurrent_unordered_map<uint64_t, Service*>::iterator it;
  for(it = service_table.begin(); it != service_table.end(); ++it) {
      Service* service = it->second;
      Session* session = service->getSession(address, protocol, port);

      if(session != NULL)
        return service;
  }

  return NULL;
}

bool LB::process(Packet* packet) {
	Ethernet* ether = new Ethernet(packet);
	IPv4* ipv4 = new IPv4(ether);

    uint32_t dst_address = ipv4->getDst();
    uint32_t src_address = ipv4->getSrc();
    uint8_t protocol = 0;
    uint16_t dst_port = 0;
    uint16_t src_port = 0;
	if(ipv4->getProto() == IP_PROTOCOL_UDP) {
        protocol = IP_PROTOCOL_UDP;

        UDP* udp = new UDP(ipv4);
        std::cout << udp << std::endl;

        dst_port = udp->getDstport();
        src_port = udp->getSrcport();
    } else if(ipv4->getProto() == IP_PROTOCOL_TCP) {
        protocol = IP_PROTOCOL_TCP;

        TCP* tcp = new TCP(ipv4);
        std::cout << tcp << std::endl;

        dst_port = tcp->getDstport();
        src_port = tcp->getSrcport();
    } else {
        return false;
    }

    Service* service = getService(dst_address, protocol, dst_port);
    if(service != NULL) { // external request
        // get session
        Session* session = service->getSession(src_address, protocol, src_port);
        if(session == NULL) { // create new session

            session = service->createSession(src_address, protocol, src_port);
            if(session == NULL) {
                return false;
            }
        } else {
            session->refresh();
        }

        // translate(NAT)
        ipv4->setDst(session->getServerAddr());
        ipv4->checksum();
        std::cout << ipv4 << std::endl;

        if(ipv4->getProto() == IP_PROTOCOL_UDP) {
            UDP* udp = new UDP(ipv4);
            udp->setDstport(session->getServerPort());
            udp->checksum();

            std::cout << udp << std::endl;
        } else if(ipv4->getProto() == IP_PROTOCOL_TCP) {
            TCP* tcp = new TCP(ipv4);
            tcp->setDstport(session->getServerPort());
            tcp->checksum();

            std::cout << tcp << std::endl;
        }
    } else {
        // find session(NAT)
        Service* service = findServiceBySession(dst_address, protocol, dst_port);
        if(service == NULL) {
            return false;
        }
        Session* session = service->getSession(dst_address, protocol, dst_port);
        session->refresh();

        // translate(NAT)
        ipv4->setSrc(service->getAddress())
        ->checksum();

        std::cout << ipv4 << std::endl;

        if(ipv4->getProto() == IP_PROTOCOL_UDP) {
            UDP* udp = new UDP(ipv4);
            udp->setSrcport(service->getPort())
                ->checksum();

            std::cout << udp << std::endl;
        } else if(ipv4->getProto() == IP_PROTOCOL_TCP) {
            TCP* tcp = new TCP(ipv4);
            tcp->setSrcport(service->getPort())
                ->checksum();

            std::cout << tcp << std::endl;
        }
    }

    return Router::route(packet);
}