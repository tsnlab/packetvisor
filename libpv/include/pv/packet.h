#pragma once

#include <iostream>
#include <stdbool.h>
#include <stdint.h>
#include <pv/driver.h>

namespace pv {

class Packet;

class Packetlet {
protected:
	Driver*	driver;

public:
	Packetlet();
	virtual ~Packetlet();

	void setDriver(Driver* driver);

	Packet* alloc(uint32_t size);
	void drop(Packet* packet);

	virtual bool received(Packet* packet);
	bool send(Packet* packet);
};

class Protocol;

class Packet {
public:
	int32_t		queueId;
	uint64_t	addr;
	uint8_t*	payload;
	uint32_t	start;
	uint32_t	end;
	uint32_t	size;
	Protocol*	protocol;

	Packet(int32_t queueId, uint64_t addr, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
	virtual ~Packet();

	friend std::ostream& operator<<(std::ostream& out, const Packet& obj);
	friend std::ostream& operator<<(std::ostream& out, const Packet* obj);
};

}; // namespace pv
