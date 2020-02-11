#ifndef __ROUTER_HPP__
#define __ROUTER_HPP__

#include <stdint.h>
#include <stdbool.h>
#include <list>

#include <pv/packet.h>

#include "interface.hpp"

class Router {
public:
    Router();
    ~Router();

    static bool addInterface(Interface* interface, bool is_default);
    static bool addInterface(Interface* interface);
    static bool route(pv::Packet* packet);

private:
};

#endif /*__ROUTER_HPP__*/