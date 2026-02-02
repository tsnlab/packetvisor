/* SPDX-License-Identifier: GPLv3.0+ */

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
#include <xdp/parsing_helpers.h>

#include "packetvisor.bpf.h"

struct llc_hdr {
	__u8 dsap;
	__u8 ssap;
	__u8 ctrl;
};

struct snap_hdr {
	__u8 oui[3];
	__be16 ethertype;
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

static __always_inline bool match_l4_ipv4(__u8 proto, __u32 l4_flags)
{
	if (!l4_flags)
		return true;

	switch (proto) {
	case IPPROTO_TCP:
		bpf_printk("[%s / %d] TCP Packet Detected", __func__, __LINE__);
		bpf_printk("[%s / %d] Result: %d", __func__, __LINE__, l4_flags & L4_FLAGS_TCP);
		return (l4_flags & L4_FLAGS_TCP) != 0;
	case IPPROTO_UDP:
		bpf_printk("[%s / %d] UDP Packet Detected", __func__, __LINE__);
		bpf_printk("[%s / %d] Result: %d", __func__, __LINE__, l4_flags & L4_FLAGS_UDP);
		return (l4_flags & L4_FLAGS_UDP) != 0;
	case IPPROTO_ICMP:
		bpf_printk("[%s / %d] ICMP Packet Detected", __func__, __LINE__);
		bpf_printk("[%s / %d] Result: %d", __func__, __LINE__, l4_flags & L4_FLAGS_ICMP);
		return (l4_flags & L4_FLAGS_ICMP) != 0;
	default:
		return (l4_flags & L4_FLAGS_OTHER) != 0;
	}
}

static __always_inline bool match_l4_ipv6(__u8 proto, __u32 l4_flags)
{
	if (!l4_flags)
		return true;

	switch (proto) {
	case IPPROTO_TCP:
		bpf_printk("[%s / %d] TCP Packet Detected", __func__, __LINE__);
		bpf_printk("[%s / %d] Result: %d", __func__, __LINE__, l4_flags & L4_FLAGS_TCP);
		return (l4_flags & L4_FLAGS_TCP) != 0;
	case IPPROTO_UDP:
		bpf_printk("[%s / %d] UDP Packet Detected", __func__, __LINE__);
		bpf_printk("[%s / %d] Result: %d", __func__, __LINE__, l4_flags & L4_FLAGS_UDP);
		return (l4_flags & L4_FLAGS_UDP) != 0;
	case IPPROTO_ICMP:
		bpf_printk("[%s / %d] ICMP Packet Detected", __func__, __LINE__);
		bpf_printk("[%s / %d] Result: %d", __func__, __LINE__, l4_flags & L4_FLAGS_ICMPV6);
	default:
		return (l4_flags & L4_FLAGS_OTHER) != 0;
	}
}

static __always_inline bool match_l3_flag(__u16 h_proto, __u32 l3_flags)
{
	if (!l3_flags)
		return true;

	switch (h_proto) {
	case ETH_P_IP:
		return (l3_flags & L3_FLAGS_IPV4) != 0;
	case ETH_P_IPV6:
		return (l3_flags & L3_FLAGS_IPV6) != 0;
	default:
		return (l3_flags & L3_FLAGS_OTHER) != 0;
	}
}

static __always_inline bool match_ipv4_l4(void *nh, void *data_end, __u32 l4_flags)
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

	if (!match_l4_ipv4(ip->protocol, l4_flags))
		return false;

	if (!l4_flags)
		return true;

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

static __always_inline bool match_ipv6_l4(void *nh, void *data_end, __u32 l4_flags)
{
	struct ipv6hdr *ip6 = nh;
	void *l4;

	if ((void *)(ip6 + 1) > data_end)
		return false;

	if (!match_l4_ipv6(ip6->nexthdr, l4_flags))
		return false;

	if (!l4_flags)
		return true;

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
	int config_key = PACKETVISOR_CONFIG_KEY;
	__u16 h_proto;
	void *nh;
	bool vlan = false;
	bool matched = true;

	/* Program enabled check: if refcnt is zero, always pass to kernel */
	if (!refcnt)
		return XDP_PASS;

	/* Ensure Ethernet header is within bounds */
	if ((void *)(eth + 1) > data_end)
		return XDP_PASS;

	/* Read filter configuration from user space */
	rx_config = bpf_map_lookup_elem(&config_map, &config_key);
	if (!rx_config)
		return XDP_PASS;

	/*
	 * No filter flags set:
	 * - By design, treat as "no userspace filter"
	 * - Default to kernel path
	 */
	if (!(rx_config->l2_flags || rx_config->l3_flags || rx_config->l4_flags))
		return XDP_PASS;

	/* Start L2 parsing from Ethernet header */
	h_proto = bpf_ntohs(eth->h_proto);
	nh = eth + 1;
	/* VLAN tag present: advance to encapsulated EtherType */
	if (h_proto == ETH_P_8021Q) {
		struct vlan_hdr *vh = nh;

		if ((void *)(vh + 1) > data_end)
			return XDP_PASS;

		h_proto = bpf_ntohs(vh->h_vlan_encapsulated_proto);
		nh = vh + 1;
		vlan = true;
	}

	/*
	 * 802.3 length field case: EtherType is in LLC/SNAP
	 * This is required for EAPOL on some WiFi paths.
	 */
	if (h_proto <= ETH_P_802_3_MIN) {
		/* 802.3 length field: check LLC/SNAP for real EtherType */
		struct llc_hdr *llc = nh;
		struct snap_hdr *snap;

		if ((void *)(llc + 1) > data_end)
			return XDP_PASS;

		if (llc->dsap == 0xaa && llc->ssap == 0xaa && llc->ctrl == 0x03) {
			snap = (void *)(llc + 1);
			if ((void *)(snap + 1) > data_end)
				return XDP_PASS;
			if (snap->oui[0] == 0x00 && snap->oui[1] == 0x00 && snap->oui[2] == 0x00) {
				h_proto = bpf_ntohs(snap->ethertype);
				nh = snap + 1;
			}
		}
	}

	if (rx_config->l2_flags) {
		/*
		 * L2 filter:
		 * - ETH/VLAN selection
		 * - Optional ARP gating (ARP must be explicitly allowed)
		 */
		if (h_proto == ETH_P_ARP) {
			if (!(rx_config->l2_flags & L2_FLAGS_ARP))
				matched = false;
		}
		if (vlan) {
			if (!(rx_config->l2_flags & L2_FLAGS_VLAN))
				matched = false;
		} else {
			if (!(rx_config->l2_flags & L2_FLAGS_ETH))
				matched = false;
		}
	}

	if (matched && rx_config->l3_flags && h_proto != ETH_P_ARP) {
		/*
		 * L3 filter (non-ARP only):
		 * - IPv4 / IPv6 / EAPOL(ETH_P_PAE) / OTHER
		 * - ARP is handled entirely in L2
		 */
		if (!match_l3_flag(h_proto, rx_config->l3_flags))
			matched = false;
	}

	if (matched && rx_config->l4_flags) {
		/*
		 * L4 filter (IP only):
		 * - TCP/UDP/ICMP/ICMPv6 selection
		 * - Also validates L4 header bounds
		 */
		if (h_proto == ETH_P_IP) {
			if (!match_ipv4_l4(nh, data_end, rx_config->l4_flags))
				matched = false;
		} else if (h_proto == ETH_P_IPV6) {
			if (!match_ipv6_l4(nh, data_end, rx_config->l4_flags))
				matched = false;
		}
	}

	if (matched) {
		/*
		 * Matched filter:
		 * - Userspace processing requested for this packet
		 * - Redirect only if AF_XDP socket is bound to this queue
		 */
		int index = ctx->rx_queue_index;
		if (bpf_map_lookup_elem(&xsks_map, &index)) {
			bpf_printk("Send to userspace by Filter");
			return bpf_redirect_map(&xsks_map, index, 0);
		}

		return XDP_PASS;
	}

	/* Not matched: default to kernel path */
	return XDP_PASS;
}

char _license[] SEC("license") = "GPLv3.0+";
