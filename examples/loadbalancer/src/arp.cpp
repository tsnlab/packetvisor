#include "arp.hpp"

ARPEntry::ARPEntry(uint64_t mac, uint32_t address): m_mac(mac), m_address(address) {
}

ARPEntry::~ARPEntry() {

}

void ARPEntry::refresh() {
    lifetime = std::chrono::system_clock::now();
}

bool ARPEntry::isTimeout() {
    auto current = std::chrono::system_clock::now();
    std::chrono::duration<double> diff = current - lifetime;

    if(diff.count() > LB_ARP_CACHE_LIFETIME)
        return true;
    else
        return false;
}

uint64_t ARPEntry::getMAC() {
    return m_mac;
}

uint32_t ARPEntry::getAddress() {
    return m_address;
}