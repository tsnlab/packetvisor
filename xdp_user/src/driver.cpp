#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <net/if.h>
//#include <bpf/bpf.h>
#include "config.h"
#include "driver.h"

#include <common/common_user_bpf_xdp.h>

namespace pv {

typedef void (*__free)(void*);
__free _free = free;

uint64_t XDPDriver::alloc_frame() {
	uint64_t frame;
	if (xsk.umem_frame_free == 0)
		return INVALID_UMEM_FRAME;

	frame = xsk.umem_frame_addr[--xsk.umem_frame_free];
	xsk.umem_frame_addr[xsk.umem_frame_free] = INVALID_UMEM_FRAME;
	return frame;
}

void XDPDriver::free_frame(uint64_t frame) {
	assert(xsk.umem_frame_free < NUM_FRAMES);

	xsk.umem_frame_addr[xsk.umem_frame_free++] = frame;
}

XDPDriver::XDPDriver() {
	mac = pv::Config::mac;
	payload_size = FRAME_SIZE;

	bzero(&umem, sizeof(struct xsk_umem_info));
	bzero(&xsk, sizeof(struct xsk_socket_info));

	// Allocate packet buffer
	packet_buffer_size = NUM_FRAMES * FRAME_SIZE;
	if(posix_memalign(&packet_buffer, getpagesize(), packet_buffer_size)) { // page size aligned alloc
		throw "ERROR: Cannot allocate packet buffer" + std::to_string(packet_buffer_size) + " bytes.";
	}

	// Initialize user memory packet buffer
	int ret = xsk_umem__create(&umem.umem, packet_buffer, packet_buffer_size, &umem.fq, &umem.cq, NULL);
	if(ret != 0) {
		throw "ERROR: Cannot crate umem object: errno: " + std::to_string(ret) + ".";
	}

	umem.buffer = packet_buffer;

	// Initialize xsk socket
	xsk.umem = &umem;

	struct xsk_socket_config xsk_cfg;
	xsk_cfg.rx_size = XSK_RING_CONS__DEFAULT_NUM_DESCS;
	xsk_cfg.tx_size = XSK_RING_PROD__DEFAULT_NUM_DESCS;
	xsk_cfg.libbpf_flags = 0;
	xsk_cfg.xdp_flags = pv::Config::xdp_flags;
	xsk_cfg.bind_flags = pv::Config::xsk_flags;
	ret = xsk_socket__create(&xsk.xsk, pv::Config::ifname, pv::Config::queue, umem.umem, &xsk.rx, &xsk.tx, &xsk_cfg);
	if(ret != 0) {
		throw "ERROR: Cannot crate xsk object: errno: " + std::to_string(ret) + ".";
	}

	uint32_t prog_id = 0;
	ret = bpf_get_link_xdp_id(pv::Config::ifindex, &prog_id, pv::Config::xdp_flags);
	if(ret != 0) {
		throw "ERROR: Cannot get xdp id: errno: " + std::to_string(ret) + ".";
	}

	for(int i = 0; i < NUM_FRAMES; i++)
		xsk.umem_frame_addr[i] = i * FRAME_SIZE;

	xsk.umem_frame_free = NUM_FRAMES;

	uint32_t idx;
	ret = xsk_ring_prod__reserve(&xsk.umem->fq, XSK_RING_PROD__DEFAULT_NUM_DESCS, &idx);
	if(ret != XSK_RING_PROD__DEFAULT_NUM_DESCS) {
		throw "ERROR: Cannot reserve buffer: errno: " + std::to_string(ret) + ".";
	}

	for(int i = 0; i < XSK_RING_PROD__DEFAULT_NUM_DESCS; i ++)
		*xsk_ring_prod__fill_addr(&xsk.umem->fq, idx++) = alloc_frame();

	xsk_ring_prod__submit(&xsk.umem->fq, XSK_RING_PROD__DEFAULT_NUM_DESCS);

	// Initialize pcap
	if(pv::Config::pcap_path[0] != '\0') {
		try {
			pcap = new pv::Pcap(pv::Config::pcap_path);
		} catch(const std::string& msg) {
			fprintf(stderr, "WARN: %s\n", msg.c_str());
		}
	} else {
		pcap = nullptr;
	}

	// Initialize poolfd
	bzero(fds, sizeof(struct pollfd) * 2);
	fds[0].fd = xsk_socket__fd(xsk.xsk);
	fds[0].events = POLLIN;
}

XDPDriver::~XDPDriver() {
	if(pcap != nullptr)
		delete pcap;

	xsk_socket__delete(xsk.xsk);
	xsk_umem__delete(umem.umem);
	_free(packet_buffer);
	xdp_link_detach(pv::Config::ifindex, pv::Config::xdp_flags, 0);
}

void XDPDriver::setCallback(Callback* callback) {
	this->callback = callback;
}

uint8_t* XDPDriver::alloc() {
	uint64_t addr = alloc_frame();
	if(addr != INVALID_UMEM_FRAME) {
		return (uint8_t*)xsk_umem__get_data(xsk.umem->buffer, addr);
	} else {
		return nullptr;
	}
}

void XDPDriver::free(uint8_t* payload) {
	uint64_t addr = (uint64_t)payload - (uint64_t)umem.buffer;
	free_frame(addr);
}

bool XDPDriver::process(uint8_t* payload, uint32_t len) {
	try {
		if(callback != nullptr && callback->received(0, payload, 0, len, len)) {
			return true;
		}
	} catch(...) {
		fprintf(stderr, "Unexpected exception occurred while processing packet\n");
	}

	return false;
}

bool XDPDriver::received() {
	unsigned int rcvd, stock_frames, i;
	uint32_t idx_rx = 0, idx_fq = 0;
	size_t ret;

	rcvd = xsk_ring_cons__peek(&xsk.rx, RX_BATCH_SIZE, &idx_rx);
	if (!rcvd)
		return false;

	/* Stuff the ring with as much frames as possible */
	stock_frames = xsk_prod_nb_free(&xsk.umem->fq, xsk.umem_frame_free);

	if (stock_frames > 0) {

		ret = xsk_ring_prod__reserve(&xsk.umem->fq, stock_frames, &idx_fq);

		/* This should not happen, but just in case */
		while(ret != stock_frames)
			ret = xsk_ring_prod__reserve(&xsk.umem->fq, rcvd, &idx_fq);

		for (i = 0; i < stock_frames; i++)
			*xsk_ring_prod__fill_addr(&xsk.umem->fq, idx_fq++) = alloc_frame();

		xsk_ring_prod__submit(&xsk.umem->fq, stock_frames);
	}

	/* Process received packets */
	for (i = 0; i < rcvd; i++) {
		uint64_t addr = xsk_ring_cons__rx_desc(&xsk.rx, idx_rx)->addr;
		uint32_t len = xsk_ring_cons__rx_desc(&xsk.rx, idx_rx++)->len;

		// rx_bytes += len
		if(!process((uint8_t*)xsk_umem__get_data(xsk.umem->buffer, addr), len))
			free_frame(addr);
	}
	// rx_packets += recvd

	xsk_ring_cons__release(&xsk.rx, rcvd);

	/* Do we need to wake up the kernel for transmission */
	if(xsk.outstanding_tx > 0) {
		flush();
	}

	return true;
}

bool XDPDriver::send(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size) {
	if(pcap != NULL)
		pcap->send(payload + start, end - start);

	uint32_t tx_idx = 0;
	int ret = xsk_ring_prod__reserve(&xsk.tx, 1, &tx_idx);
	if (ret != 1) {
		/* No more transmit slots, drop the packet */
		return false;
	}

	uint64_t addr = (uint64_t)payload - (uint64_t)umem.buffer;
	xsk_ring_prod__tx_desc(&xsk.tx, tx_idx)->addr = addr;
	xsk_ring_prod__tx_desc(&xsk.tx, tx_idx)->len = end - start;
	xsk_ring_prod__submit(&xsk.tx, 1);
	xsk.outstanding_tx++;

	// tx_byees += end - start;
	// tx_packets += 1

	return true;
}

void XDPDriver::flush() {
	unsigned int completed;
	uint32_t idx_cq;

	sendto(xsk_socket__fd(xsk.xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);

	/* Collect/free completed TX buffers */
	completed = xsk_ring_cons__peek(&xsk.umem->cq,
					XSK_RING_CONS__DEFAULT_NUM_DESCS,
					&idx_cq);

	if(completed > 0) {
		for(unsigned int i = 0; i < completed; i++)
			free_frame(*xsk_ring_cons__comp_addr(&xsk.umem->cq, idx_cq++));

		xsk_ring_cons__release(&xsk.umem->cq, completed);
	}
}

bool XDPDriver::loop() {
	int nfds = 1;

	if(pv::Config::is_xsk_polling) {
		int ret = poll(fds, nfds, -1);
		if(ret <= 0 || ret > 1) {
			return false;
		}
	}

	return received();
}

}; // namespace pv
