#ifndef __PV_DRIVER_H__
#define __PV_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

// APIs for libpv
#ifdef __cplusplus
namespace pv {
#endif

class Callback {
public:
	Callback();
	virtual ~Callback();

	virtual bool received(uint32_t queueId, uint64_t addr, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
};

class Driver {
public:
	uint64_t	mac;

	Driver();
	virtual ~Driver();

	virtual bool alloc(uint64_t* addr, uint8_t** payload, uint32_t size);
	virtual void free(uint64_t addr);

	virtual bool send(uint32_t queueId, uint64_t addr, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
};

typedef Callback* (*Init)(Driver* driver);
typedef void (*Destroy)(Callback* callback);

#ifdef __cplusplus
}; // namespace pv
#endif

#endif /* __PV_DRIVER_H__ */
