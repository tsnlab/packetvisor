/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <stdbool.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <xdp/xdp_helpers.h>

#include "packetvisor.bpf.h"

struct vlan_hdr {
	__be16 h_vlan_TCI;
	__be16 h_vlan_encapsulated_proto;
};

#define DEFAULT_QUEUE_IDS 64

struct {
	__uint(type, BPF_MAP_TYPE_XSKMAP);
	__uint(key_size, sizeof(int));
	__uint(value_size, sizeof(int));
	__uint(max_entries, DEFAULT_QUEUE_IDS);
} xsks_map SEC(".maps");

/* Configuration map to receive single key-value from user space */
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(key_size, sizeof(int));
	__uint(value_size, sizeof(struct af_xdp_rx_config));
	__uint(max_entries, 1);
} config_map SEC(".maps");

struct {
	__uint(priority, 5);
	__uint(XDP_PASS, 1);
} XDP_RUN_CONFIG(xsk_packetvisor_prog);

/* Program refcount, in order to work properly,
 * must be declared before any other global variables
 * and initialized with '1'.
 */
 volatile int refcnt = 1;


static __always_inline bool match_l3_flag(__u16 h_proto, __u32 l3_flags)
{
	if (!l3_flags)
		return true;

	switch (h_proto) {
	case ETH_P_IP:
		return (l3_flags & L3_FLAGS_IPV4) != 0;
	case ETH_P_IPV6:
		return (l3_flags & L3_FLAGS_IPV6) != 0;
	case ETH_P_ARP:
		return (l3_flags & L3_FLAGS_ARP) != 0;
	case ETH_P_PAE:
		bpf_printk("Proto is EAPOL");
		bpf_printk("Result: %d\n", l3_flags & L3_FLAGS_EAPOL);
		return (l3_flags & L3_FLAGS_EAPOL) != 0;
	default:
		return (l3_flags & L3_FLAGS_OTHER) != 0;
	}
}

static __always_inline bool match_ipv4_l4(void *nh, void *data_end)
{
	struct iphdr *ip = nh;
	__u32 ihl;
	void *l4;

	if ((void *)(ip + 1) > data_end)
		return false;

	ihl = ip->ihl * 4;
	if (ihl < sizeof(*ip))
		return false;

	if ((void *)ip + ihl > data_end)
		return false;

	l4 = (void *)ip + ihl;
	switch (ip->protocol) {
	case IPPROTO_TCP:
		return (void *)(l4 + sizeof(struct tcphdr)) <= data_end;
	case IPPROTO_UDP:
		return (void *)(l4 + sizeof(struct udphdr)) <= data_end;
	case IPPROTO_ICMP:
		return (void *)(l4 + sizeof(struct icmphdr)) <= data_end;
	default:
		return true;
	}
}

static __always_inline bool match_ipv6_l4(void *nh, void *data_end)
{
	struct ipv6hdr *ip6 = nh;
	void *l4;

	if ((void *)(ip6 + 1) > data_end)
		return false;

	l4 = (void *)(ip6 + 1);
	switch (ip6->nexthdr) {
	case IPPROTO_TCP:
		return (void *)(l4 + sizeof(struct tcphdr)) <= data_end;
	case IPPROTO_UDP:
		return (void *)(l4 + sizeof(struct udphdr)) <= data_end;
	case IPPROTO_ICMPV6:
		return (void *)(l4 + sizeof(struct icmp6hdr)) <= data_end;
	default:
		return true;
	}
}


/* This is the program for post 5.3 kernels. */
SEC("xdp")
int xsk_packetvisor_prog(struct xdp_md *ctx)
{
	void *data = (void *)(long)ctx->data;
	void *data_end = (void *)(long)ctx->data_end;
	struct ethhdr *eth = data;
	struct af_xdp_rx_config *rx_config;
	int config_key = PACKERVISOR_CONFIG_KEY;
	__u16 h_proto;
	void *nh;
	bool vlan = false;
	bool matched = true;

	/* Make sure refcount is referenced by the program */
	if (!refcnt)
		return XDP_PASS;

	if ((void *)(eth + 1) > data_end)
		return XDP_PASS;

	rx_config = bpf_map_lookup_elem(&config_map, &config_key);
	if (!rx_config)
		return XDP_PASS;

	if (!(rx_config->l2_flags || rx_config->l3_flags)) {
		int index = ctx->rx_queue_index;

		if (bpf_map_lookup_elem(&xsks_map, &index))
			return bpf_redirect_map(&xsks_map, index, 0);

		return XDP_PASS;
	}

	h_proto = bpf_ntohs(eth->h_proto);
	nh = eth + 1;
	if (h_proto == ETH_P_8021Q || h_proto == ETH_P_8021AD) {
		struct vlan_hdr *vh = nh;

		if ((void *)(vh + 1) > data_end)
			return XDP_PASS;

		h_proto = bpf_ntohs(vh->h_vlan_encapsulated_proto);
		nh = vh + 1;
		vlan = true;
	}

	if (rx_config->l2_flags) {
		if (vlan) {
			if (!(rx_config->l2_flags & L2_FLAGS_VLAN))
				matched = false;
		} else {
			if (!(rx_config->l2_flags & L2_FLAGS_ETH))
				matched = false;
		}
	}

	if (matched && rx_config->l3_flags) {
		if (!match_l3_flag(h_proto, rx_config->l3_flags))
			matched = false;
	}

	if (matched) {
		if (h_proto == ETH_P_IP) {
			if (!match_ipv4_l4(nh, data_end))
				matched = false;
		} else if (h_proto == ETH_P_IPV6) {
			if (!match_ipv6_l4(nh, data_end))
				matched = false;
		}
	}

	if (matched)
		return XDP_PASS;

	/* Packet did not match config, redirect to user space if socket is bound */
	int index = ctx->rx_queue_index;
	if (bpf_map_lookup_elem(&xsks_map, &index))
		return bpf_redirect_map(&xsks_map, index, 0);

	return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
