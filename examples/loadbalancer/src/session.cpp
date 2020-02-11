#include <stdint.h>

#include "session.hpp"

Session::Session(uint32_t serverAddr, uint16_t serverPort, uint32_t clientAddr, uint16_t clientPort) : m_serverAddr(serverAddr), m_serverPort(serverPort), m_clientAddr(clientAddr), m_clientPort(clientPort) {
    lifetime = std::chrono::system_clock::now();
}

void Session::refresh() {
    lifetime = std::chrono::system_clock::now();
}

bool Session::isTimeout() {
    auto current = std::chrono::system_clock::now();
    std::chrono::duration<double> diff = current - lifetime;

    if(diff.count() > LB_SESSION_LIFETIME)
        return true;
    else
        return false;
}

uint32_t Session::getServerAddr() {
    return m_serverAddr;
}

uint16_t Session::getServerPort() {
    return m_serverPort;
}

uint32_t Session::getClientAddr() {
    return m_clientAddr;
}

uint16_t Session::getClientPort() {
    return m_clientPort;
}