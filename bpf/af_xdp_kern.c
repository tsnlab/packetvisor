/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <xdp/xdp_helpers.h>

#define DEFAULT_QUEUE_IDS 64

struct {
	__uint(type, BPF_MAP_TYPE_XSKMAP);
	__uint(key_size, sizeof(int));
	__uint(value_size, sizeof(int));
	__uint(max_entries, DEFAULT_QUEUE_IDS);
} xsks_map SEC(".maps");

struct {
	__uint(priority, 5);
	__uint(XDP_PASS, 1);
} XDP_RUN_CONFIG(xsk_packetvisor_prog);

/* Program refcount, in order to work properly,
 * must be declared before any other global variables
 * and initialized with '1'.
 */
 volatile int refcnt = 1;

/* This is the program for post 5.3 kernels. */
SEC("xdp")
int xsk_packetvisor_prog(struct xdp_md *ctx)
{
    bpf_printk("%s: Hello, World!", __func__);

	/* Make sure refcount is referenced by the program */
	if (!refcnt)
		return XDP_PASS;

	int index = ctx->rx_queue_index;

	/* A set entry here means that the corresponding queue_id
	 * has an active AF_XDP socket bound to it.
	 */
	if (bpf_map_lookup_elem(&xsks_map, &index))
		return bpf_redirect_map(&xsks_map, index, 0);

	return XDP_PASS;
}

char _license[] SEC("license") = "GPL";