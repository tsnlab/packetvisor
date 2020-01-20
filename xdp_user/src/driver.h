#pragma once

#include <poll.h>
#include <bpf/xsk.h>
#include <pv/driver.h>
#include "pcap.h"

namespace pv {

#define NUM_FRAMES         4096
#define FRAME_SIZE         XSK_UMEM__DEFAULT_FRAME_SIZE
#define RX_BATCH_SIZE      64
#define INVALID_UMEM_FRAME UINT64_MAX

struct xsk_umem_info {
	struct xsk_ring_prod fq;
	struct xsk_ring_cons cq;
	struct xsk_umem *umem;
	void *buffer;
};

struct xsk_socket_info {
	struct xsk_ring_cons rx;
	struct xsk_ring_prod tx;
	struct xsk_umem_info *umem;
	struct xsk_socket *xsk;

	uint64_t umem_frame_addr[NUM_FRAMES];
	uint32_t umem_frame_free;

	uint32_t outstanding_tx;
};

class XDPDriver : public pv::Driver {
private:
	struct pollfd				fds[2];

protected:
	void*						packet_buffer;
	uint64_t					packet_buffer_size;
	struct xsk_umem_info		umem;
	struct xsk_socket_info		xsk;
	Pcap*						pcap;
	Callback*					callback;

	uint64_t alloc_frame();
	void free_frame(uint64_t frame);
	
public:
	XDPDriver();
	virtual ~XDPDriver();

	void setCallback(Callback* callback);

	uint8_t* alloc();
	void free(uint8_t* payload);

	bool process(uint8_t* payload, uint32_t len);
	bool received();
	bool send(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
	void flush();

	bool loop();
};

}; // namespace pv
