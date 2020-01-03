#ifndef __PACKETVISOR_DRIVER_H__
#define __PACKETVISOR_DRIVER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct packetvisor_Driver {
	void (*received)(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
};

struct packetvisor_Callback {
	uint8_t* (*alloc)(uint32_t queueId, uint32_t size);
	void (*free)(uint32_t queueId, uint8_t* payload);
	void (*send)(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
};

typedef struct packetvisor_Driver* (*packetvisor_Init)(struct packetvisor_Callback* callback);
typedef void (*packetvisor_Received)(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* __PACKETVISOR_DRIVER_H__ */
