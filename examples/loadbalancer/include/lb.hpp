#ifndef __LB_HPP__
#define __LB_HPP__

#include <cstdio>
#include <stdint.h>

#include <pv/packet.h>

#include "service.hpp"

using namespace std;
using namespace __gnu_cxx;

class LB {
public:
    LB();
    ~LB();

    static Service* addService(enum LB_SERVICE_ALGORITHM algo, uint32_t address, uint8_t protocol, uint16_t port); // TODO: check protocol
    static Service* getService(uint32_t address, uint8_t protocol, uint16_t port);
    static Service* findServiceBySession(uint32_t address, uint8_t protocol, uint16_t port);
    static bool process(pv::Packet* packet);

private:
};

#endif /*__LB_HPP__*/