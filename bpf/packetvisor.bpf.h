/* SPDX-License-Identifier: GPLv3.0+ */

#ifndef AF_XDP_KERN_H
#define AF_XDP_KERN_H

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <xdp/xdp_helpers.h>

/* Packetvisor configuration key */
#define PACKETVISOR_CONFIG_KEY 0


/* L2 flags */
#define L2_FLAGS_ETH (1 << 0)
#define L2_FLAGS_VLAN (1 << 1)
#define L2_FLAGS_ARP (1 << 2)
#define L2_FLAGS_OTHER (1 << 3)
#define L2_FLAGS_RESERVED (1 << 4)

/* L3 flags */
#define L3_FLAGS_IPV4 (1 << 0)
#define L3_FLAGS_IPV6 (1 << 1)
#define L3_FLAGS_OTHER (1 << 2)
#define L3_FLAGS_RESERVED (1 << 3)

/* L4 flags */
#define L4_FLAGS_TCP (1 << 0)
#define L4_FLAGS_UDP (1 << 1)
#define L4_FLAGS_ICMP (1 << 2)
#define L4_FLAGS_ICMPV6 (1 << 3)
#define L4_FLAGS_OTHER (1 << 4)
#define L4_FLAGS_RESERVED (1 << 5)

/* AF_XDP rx configuration */
struct af_xdp_rx_config{
    __u32 l2_flags;
    __u32 l3_flags;
    __u32 l4_flags;

    // TODO: match ipv4 address & TCP, UDP port number
};

#endif
