#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pv/packet.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get number of NICs. Or get maximum nic_id - 1.
 *
 * @return number of NICs
 */
uint16_t pv_nic_count();

/**
 * Get MAC address from a NIC.
 *
 * @param nic_id    NIC ID.
 * @return MAC address in uint64_t representation. You don't need to care about the endianness.
 *         MAC address 00:11:22:33:44:55 will be represented in 0x1122334455ULL.
 *         If there is error 0 will be returned.
 */
uint64_t pv_nic_get_mac(uint16_t nic_id);

/**
 * Get IPv4 address for a NIC.
 * @param nic_id NIC ID.
 * @return IPv4 address that is specified in configuration.
 */
struct in_addr pv_nic_get_ipv4(uint16_t nic_id);

/**
 * Get IPv4 address for a NIC.
 * @param nic_id NIC ID.
 * @return IPv4 address that is specified in configuration.
 */
struct in6_addr pv_nic_get_ipv6(uint16_t nic_id);

/**
 * Check the NIC is in promiscuous mode or not.
 *
 * @param nic_id    NIC ID.
 * @return true if the NIC is in promiscuous mode. If there is some error, it will return false.
 */
bool pv_nic_is_promiscuous(uint16_t nic_id);

/**
 * Set the NIC's promiscuous mode.
 *
 * @param nic_id    NIC ID.
 * @param mode      true if promiscuous mode is on.
 * @return 0 is returned when there is no error. Error code is not specified yet.
 */
int pv_nic_set_promiscuous(uint16_t nic_id, bool mode);

/**
 * Receive chunk of packets from the NIC.
 *
 * @param nic_id    NIC ID.
 * @param queue_id  queue ID.
 * @param array     chunk of packets.
 * @param count     number of packets to store the packets.
 * @return number of packets received.
 */
uint16_t pv_nic_rx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** array, uint16_t count);

/**
 * Transmit chunk of packets to the NIC.
 *
 * @param nic_id    NIC ID.
 * @param queue_id  queue ID.
 * @param array     chunk of packets.
 * @param count     number of packets which the array contains.
 * @return number of packets transmitted from the first. If not all the packets are transmitted,
 *         you must retransmit or free the remain packets.
 */
uint16_t pv_nic_tx_burst(uint16_t nic_id, uint16_t queue_id, struct pv_packet** array, uint16_t count);

/**
 * Transmit a packet to the NIC. This function is a wrapper function to #pv_nic_tx_burst
 *
 * @param nic_id    NIC ID.
 * @param queue_id  queue ID.
 * @param packet    a packet data structure to transmits.
 * @return true if the packet is transmitted. If it is not transmitted,
 *         you must retransmit or free the packet.
 */
bool pv_nic_tx(uint16_t nic_id, uint16_t queue_id, struct pv_packet* packet);

/**
 * Get RX timestamp used for IEEE1588
 * @param portid port id to get timestamp from
 * @param timestamp timespec struct to store timestamp
 * @param timesync_flag timesync_flag @see pv_packet_get_timesync_flag
 * @return true if success
 */
bool pv_nic_get_rx_timestamp(uint16_t nic_id, struct timespec* timestamp);

/**
 * Get TX timestamp used for IEEE1588
 * @param portid port id to get timestamp from
 * @param timestamp timespec struct to store timestamp
 * @return true if success
 */
bool pv_nic_get_tx_timestamp(uint16_t nic_id, struct timespec* timestamp);

/**
 * Get current NIC's timestamp
 * @param portid port id to get timestamp from
 * @param timestamp timespec struct to store timestamp
 * @return true if success
 */
bool pv_nic_get_timestamp(uint16_t nic_id, struct timespec* timestamp);

/**
 * Adjust NIC's time with delta
 * @param nic_id port id to adjust time for
 * @param delta delta in ns
 * @return true if success
 */
bool pv_nic_timesync_adjust_time(uint16_t nic_id, int64_t delta);

#ifdef __cplusplus
}
#endif
