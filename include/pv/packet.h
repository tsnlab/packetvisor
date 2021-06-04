#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * Get start pointer of payload from the packet data structure.
 *
 * @param packet    a packet data structure.
 * @return payload start pointer of payload in void* type.
 */
#define PV_PACKET_PAYLOAD(packet) (void*)(packet->buffer + packet->start)

/**
 * Get payload length in bytes.
 *
 * @param packet    a packet data structure.
 * @return payload length in bytes.
 */
#define PV_PACKET_PAYLOAD_LEN(packet) (packet->end - packet->start)

/**
 * This data structure represents a packet.
 */
struct pv_packet {
    /**
     * payload start index from buffer. This index is inclusive.
     */
    uint32_t start;
    /**
     * payload end index from buffer. This index is not inclusive so payload is stored
     * from start to end - 1. In other words, [start, end). In another words,
     * buffer[start] contains the first byte of the payload and buffer[end - 1] contains
     * the last bytr of the payload. You can calculate the pyalod length by (end - start).
     */
    uint32_t end;
    /**
     * buffer size in bytes.
     */
    uint32_t size;
    /**
     * buffer can be separated in 3 parts. Head padding is a padding before payload.
     * Usually about 100 bytes. Payload is packet payload. Tail padding is a padding after payload.
     * If you want to prepend a header (such as VLAN header), you can use header padding.
     * If you want to append some data, you can use tail padding.
     */
    uint8_t* buffer;
    /**
     * network interface controller ID. nic_id is used to distinguish NICs.
     * Every NIC has its own MAC address.
     */
    uint16_t nic_id;
    /**
     * queue_id is used to distinguish RSS core. Every RSS core has its own ID.
     */
    uint16_t queue_id;
    /**
     * private area to store something internal use.
     */
    void* priv;
};

/**
 * Allocate a packet data structure. If there is no more memory(packet pool), it will return NULL.
 * This function guarantees the buffer size is more than 1514 bytes. So all of the packets can be
 * stored in the buffer except jumbo frame. Packetvisor doesn't support jumbo frame yet.
 *
 * @return  A packet data structure. When there is no more memory(packetpool), it will return NULL.
 */
struct pv_packet* pv_packet_alloc();

/**
 * Deallocate a packet data structure. In other words drop a packet. If a packet is transmitted to a NIC,
 * the packet is freed internally. So you MUST NOT free the packet when it is transmitted.
 */
void pv_packet_free(struct pv_packet* packet);

#ifdef __cplusplus
}
#endif
