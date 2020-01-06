#ifndef __PV_DRIVER_H__
#define __PV_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

// APIs for XDP
#ifdef __cplusplus
extern "C" {
#endif

struct pv_Driver {
	void (*received)(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
};

struct pv_Callback {
	uint8_t* (*alloc)(uint32_t size);
	void (*free)(uint8_t* payload);
	bool (*send)(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
};

typedef struct pv_Driver* (*pv_Init)(struct pv_Callback* callback);
typedef void (*pv_Received)(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);

#ifdef __cplusplus
} // extern "C"
#endif

// APIs for libpv
#ifdef __cplusplus
namespace pv {
#endif

int32_t pv_driver_add_packetlet(void* packetlet);
bool pv_driver_remove_packetlet(void* packetlet);

uint8_t* pv_driver_alloc(uint32_t size);
void pv_driver_free(uint8_t* payload);
bool pv_driver_send(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);

#ifdef __cplusplus
}; // namespace pv
#endif

#endif /* __PV_DRIVER_H__ */
