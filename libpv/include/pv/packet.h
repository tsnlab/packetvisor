#pragma once

#include <stdbool.h>
#include <stdint.h>

class Packet {
public:
	uint32_t	queueId;
	uint8_t*	payload;
	uint32_t	start;
	uint32_t	end;
	uint32_t	size;

	Packet(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
	virtual ~Packet();
};
