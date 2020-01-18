#pragma once

#include <bpf/xsk.h>

namespace pv {

class XDPEnv {
protected:
	struct xsk_umem*		umem;
	struct xsk_ring_prod	fill;
	struct xsk_ring_cons	complete;
	void*					buffer;

public:
	XDPEnv();
	virtual ~XDPEnv();

	uint8_t* alloc();
	void free(uint8_t* payload);
};

}; // namespace pv
