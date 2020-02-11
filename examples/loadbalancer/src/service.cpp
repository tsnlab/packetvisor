#include <iostream>
#include <stdlib.h>
#include <iterator>

#include "service.hpp"

Service::Service(enum LB_SERVICE_ALGORITHM algo, uint32_t address, uint8_t protocol, uint16_t port): m_algo(algo), m_address(address), m_protocol(protocol), m_port(port) {
}

Service::~Service() {
    //TODO
}

bool Service::addServer(uint32_t address, uint16_t port, enum LB_SERVER_MODE mode) {
    struct LBServer* lb_server = (struct LBServer*)calloc(1, sizeof(struct LBServer));
    if(lb_server == NULL)
        return false;

    lb_server->address = address;
    lb_server->port = port;
    lb_server->mode = mode;

    // add to server table
    server_list.push_back(lb_server);

    return true;
}

struct LBServer* Service::getNextServer() {
    if(server_list.size() == 0)
        return NULL;

    switch(m_algo) {
        case LB_SERVICE_ALGORITHM_RR:;
            auto it = std::next(server_list.begin(), (rr_counter++ % server_list.size()));
            return *it;
    }

    return NULL;
}

Session* Service::getSession(uint32_t address, uint8_t protocol, uint16_t port) {
    uint64_t hash_key = ((uint64_t)address << 32) | ((uint64_t)protocol << 16) | (uint64_t)port; // [    address(32)    ][(8)][protocol(8)][port(16)]
    auto it = session_table.find(hash_key);
    if(it == session_table.end())
        return NULL;

    return it->second;
}

Session* Service::createSession(uint32_t address, uint8_t protocol, uint16_t port) {
    LBServer* server = getNextServer();
    if(server == NULL) {
        return NULL;
    }

    Session* session = new Session(server->address, server->port, address, port);
    if(session == NULL) {
        return NULL;
    }

    uint64_t hash_key = ((uint64_t)address << 32) | ((uint64_t)protocol << 16) | (uint64_t)port; // [    address(32)    ][(8)][protocol(8)][port(16)]
    auto ret = session_table.insert(hash_map<uint64_t, Session*>::value_type(hash_key, session));
    if(!ret.second) {
        std::cout << "duplicated hash_key: " << hash_key << std::endl;
        delete session;
        return NULL;
    }

    return session;
}

uint32_t Service::getAddress() {
    return m_address;
}

uint16_t Service::getPort() {
    return m_port;
}

