#include <stdlib.h>

#include <pv/packet.h>
#include <pv/ethernet.h>
#include <pv/ipv4.h>

#include "router.hpp"

using namespace pv;
using namespace std;

static std::list<Interface*> interface_list; // TODO: fix tbb
static Interface* default_interface{NULL};

Router::Router() {
}

Router::~Router() {
}

bool Router::addInterface(Interface* interface, bool is_default) {
    if(is_default)
        default_interface = interface;

    interface_list.push_back(interface);
    return true;
}

bool Router::addInterface(Interface* interface) {
    return addInterface(interface, false);
}

bool Router::route(pv::Packet* packet) {
	Ethernet* ether = new Ethernet(packet);
	IPv4* ipv4 = new IPv4(ether);

    uint32_t dst_address = ipv4->getDst();

    for(list<Interface*>::iterator it = interface_list.begin(); it != interface_list.end(); ++it) {
        Interface* interface = *it;

        if((dst_address & interface->getNetmask()) == (interface->getAddress() & interface->getNetmask())) {
            ether->setDst(interface->getDestHWAddr(dst_address))
                    ->setSrc(interface->getMAC());

            packet->queueId = std::distance(interface_list.begin(), it) + 1;

            return true;
        }
    }

    if(default_interface != NULL) {
        uint32_t queueId = 0;
        for(list<Interface*>::iterator it = interface_list.begin(); it != interface_list.end(); ++it) {
            Interface* interface = *it;

            if(interface == default_interface) {
                queueId = std::distance(interface_list.begin(), it) + 1;

                break;
            }
        }

        ether->setDst(default_interface->getDestHWAddr(default_interface->getGateway()))
                ->setSrc(default_interface->getMAC());

        packet->queueId = queueId;

        return true;
    }

    return false;
}