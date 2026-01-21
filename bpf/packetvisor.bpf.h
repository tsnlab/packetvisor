#ifndef AF_XDP_KERN_H
#define AF_XDP_KERN_H

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <xdp/xdp_helpers.h>

/* Packervisor configuration key */
#define PACKERVISOR_CONFIG_KEY 0

/* L2 flags */
#define L2_FLAGS_ETH (1 << 0)
#define L2_FLAGS_VLAN (1 << 1)
#define L2_FLAGS_RESERVED (1 << 2)

/* L3 flags */
#define L3_FLAGS_IPV4 (1 << 0)
#define L3_FLAGS_RESERVED (1 << 1)

/* Ethernet type mask */
#define ETH_TYPE_MASK_IPV4 (1 << 0)
#define ETH_TYPE_MASK_IPV6 (1 << 1)
#define ETH_TYPE_MASK_ARP (1 << 2)
#define ETH_TYPE_MASK_OTHER (1 << 3)
#define ETH_TYPE_MASK_RESERVED (1 << 4)

/* IPv4 protocol mask */
#define IPV4_PROTO_MASK_ICMP (1 << 0)
#define IPV4_PROTO_MASK_UDP (1 << 1)
#define IPV4_PROTO_MASK_TCP (1 << 2)
#define IPV4_PROTO_MASK_OTHER (1 << 3)
#define IPV4_PROTO_MASK_RESERVED (1 << 4)

/* AF_XDP rx configuration */
struct af_xdp_rx_config{
    __u32 l2_flags;
    __u32 l3_flags;

    __u32 eth_type_mask;
    __u32 ipv4_proto_mask;
};

#endif