#ifndef __PV_DRIVER_H__
#define __PV_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pv_Driver {
	void (*received)(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
};

struct pv_Callback {
	uint8_t* (*alloc)(uint32_t queueId, uint32_t size);
	void (*free)(uint32_t queueId, uint8_t* payload);
	void (*send)(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
};

typedef struct pv_Driver* (*pv_Init)(struct pv_Callback* callback);
typedef void (*pv_Received)(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __PV_DRIVER_H__ */
