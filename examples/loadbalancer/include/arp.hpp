#ifndef __ARP_HPP__
#define __ARP_HPP__

#include <stdint.h>
#include <chrono>

#define LB_ARP_CACHE_LIFETIME     60 // 60 seconds

class ARPEntry {
public:
    ARPEntry(uint64_t mac, uint32_t address);
    ~ARPEntry();

    bool isTimeout();
    void refresh();
    uint64_t getMAC();
    uint32_t getAddress();

private:
    uint64_t m_mac{0};
    uint32_t m_address{0};
    std::chrono::time_point<std::chrono::system_clock> lifetime;
};

#endif /*__ARP_HPP__*/