#pragma once

#include <iostream>
#include <stdbool.h>
#include <stdint.h>

namespace pv {

class Packet;

class Packetlet {
protected:
	int32_t id;

public:
	Packetlet();
	virtual ~Packetlet();

	virtual bool received(Packet* packet);
	bool send(Packet* packet);
};

class Packet {
public:
	int32_t		queueId;
	uint64_t	addr;
	uint8_t*	payload;
	uint32_t	start;
	uint32_t	end;
	uint32_t	size;

	Packet(uint64_t addr, uint32_t size);
	Packet(int32_t queueId, uint64_t addr, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
	virtual ~Packet();

	friend std::ostream& operator<<(std::ostream& out, const Packet& obj);
	friend std::ostream& operator<<(std::ostream& out, const Packet* obj);
};

}; // namespace pv
