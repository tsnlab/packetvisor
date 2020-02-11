#ifndef __SERVICE_HPP__
#define __SERVICE_HPP__

#include <stdint.h>
#include <list>
#include <hash_map>

#include "session.hpp"

using namespace std;
using namespace __gnu_cxx;

enum LB_SERVICE_ALGORITHM {
    LB_SERVICE_ALGORITHM_RR = 1,
};

enum LB_SERVER_MODE {
    LB_SERVER_MODE_NAT = 1,
};

struct LBServer {
    uint32_t address;
    uint8_t protocol;
    uint16_t port;
    enum LB_SERVER_MODE mode;
};

class Service {
public:
    Service(enum LB_SERVICE_ALGORITHM algo, uint32_t address, uint8_t protocol, uint16_t port);
    ~Service();
    bool addServer(uint32_t address, uint16_t port, enum LB_SERVER_MODE mode);
    Session* getSession(uint32_t address, uint8_t protocol, uint16_t port);
    Session* createSession(uint32_t address, uint8_t protocol, uint16_t port);
    uint32_t getAddress();
    uint16_t getPort();

private:
    struct LBServer* getNextServer();

private:
    enum LB_SERVICE_ALGORITHM m_algo = {LB_SERVICE_ALGORITHM_RR};
    uint32_t m_address = {0};
    uint8_t m_protocol = {0};
    uint16_t m_port = {0};
    std::list<struct LBServer*> server_list;
    hash_map<uint64_t, Session*> session_table;
    hash_map<uint64_t, Session*> session_table2; //service reply
    
    uint16_t rr_counter = {0};
};

#endif /*__SERVICE_HPP__*/