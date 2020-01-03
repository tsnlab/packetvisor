#include <stdio.h>
#include <packetvisor/driver.h>

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

static struct packetvisor_Callback* callback;

static struct packetvisor_Driver driver = {
	.received = received
};

struct packetvisor_Driver* packetvisor_init(struct packetvisor_Callback* cb) {
	callback = cb;
	return &driver;
}

}
