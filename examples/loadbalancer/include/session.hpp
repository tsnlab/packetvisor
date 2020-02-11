#ifndef __SESSION_HPP__
#define __SESSION_HPP__

#include <chrono>

#define LB_SESSION_LIFETIME 60 * 60 // 1hour

class Session {
public:
    Session(uint32_t serverAddr, uint16_t serverPort, uint32_t clientAddr, uint16_t clientPort);
    ~Session();

    void refresh();
    bool isTimeout();

    uint32_t getServerAddr();
    uint16_t getServerPort();
    uint32_t getClientAddr();
    uint16_t getClientPort();

private:
    uint32_t m_serverAddr{0};
    uint16_t m_serverPort{0};
    uint32_t m_clientAddr{0};
    uint16_t m_clientPort{0};

    std::chrono::time_point<std::chrono::system_clock> lifetime;
};

#endif /*__SESSION_HPP__*/