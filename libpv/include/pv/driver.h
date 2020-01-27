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

	virtual bool received(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end);

	virtual int32_t load(const char* path, int argc, char** argv);
	virtual bool unload(int32_t id);
};

class Driver {
protected:
	uint64_t	mac;
	uint32_t	payload_size;

public:
	Driver();
	virtual ~Driver();

	virtual uint64_t getMAC();
	virtual uint64_t getMAC(uint32_t id);
	virtual uint32_t getPayloadSize();

	virtual uint8_t* alloc();
	virtual void free(uint8_t* payload);

	virtual bool send(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end);
};

typedef Callback* (*Init)(Driver* driver);
typedef void (*Destroy)(Callback* callback);

#ifdef __cplusplus
}; // namespace pv
#endif

#endif /* __PV_DRIVER_H__ */
