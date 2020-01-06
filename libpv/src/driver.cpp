#include <stdio.h>
#include <pv/driver.h>

extern "C" {

static void received(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size) {
	printf("received in c++ ");
	for(uint32_t i = start; i < end; i++) {
		printf("%02x ", payload[i]);
		if((i + 1) % 16 == 0)
			printf("\n");
	}
	printf("\n");
}

static struct pv_Callback* callback;

static struct pv_Driver driver = {
	.received = received
};

struct pv_Driver* pv_init(struct pv_Callback* cb) {
	callback = cb;
	return &driver;
}

}
