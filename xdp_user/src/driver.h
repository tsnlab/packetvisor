#pragma once

#include <string>
#include <poll.h>
#include <bpf/xsk.h>
#include <pv/driver.h>
#include <pthread.h>
#include "config.h"
#include "pcap.h"

namespace pv {

#define FRAME_SIZE         XSK_UMEM__DEFAULT_FRAME_SIZE
#define RX_BATCH_SIZE      64
#define INVALID_UMEM_FRAME UINT64_MAX

struct xsk_umem_info {
	struct xsk_ring_prod fq;
	struct xsk_ring_cons cq;
	struct xsk_umem *umem;
	void *buffer;

	pthread_spinlock_t	lock;

	uint64_t* umem_frame_addr;
	uint32_t umem_frame_free;
};

struct xsk_socket_info {
	struct xsk_ring_cons rx;
	struct xsk_ring_prod tx;
	struct xsk_umem_info *umem;
	struct xsk_socket *xsk;

	uint32_t outstanding_tx;
};

class XDPDriver : public pv::Driver {
private:
	struct pollfd				fds[2];
	int							ifindex;
	uint32_t					xdp_flags;
	bool						is_polling;

protected:
	static XDPDriver*			xdp_drivers[16];
	static uint32_t				xdp_driver_count;

	struct xsk_umem_info		umem;
	void*						packet_buffer;
	uint64_t					packet_buffer_size;

	struct xsk_socket_info		xsk;
	Pcap*						pcap;
	Callback*					callback;

	uint64_t alloc_frame();
	void free_frame(uint64_t frame);
	void _free_frame(uint64_t frame);
	
public:
	uint32_t		id;
	std::string		ifname;

	XDPDriver(XDPConfig* config);
	virtual ~XDPDriver();

	void setCallback(Callback* callback);

	uint64_t getMAC(uint32_t id);
	uint32_t getPayloadSize();

	uint8_t* alloc();
	void free(uint8_t* payload);

	bool process(uint8_t* payload, uint32_t len);
	bool received();
	bool send(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end);
	void flush();

	bool loop();
};

}; // namespace pv
