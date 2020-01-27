#pragma once

#include <iostream>
#include <stdbool.h>
#include <stdint.h>
#include <pv/driver.h>

namespace pv {

class Packet;

class Packetlet {
protected:
	Driver*		driver;
	int32_t		id;
	void*		handle;

public:
	Packetlet();
	virtual ~Packetlet();

	virtual void init(Driver* driver);

	void setId(int32_t id);
	int32_t getId();

	void setHandle(void* handle);
	void* getHandle();

	Packet* alloc();
	void drop(Packet* packet);

	virtual bool received(Packet* packet);
	bool send(Packet* packet);
};

class Protocol;

class Packet {
public:
	uint32_t	queueId;
	uint8_t*	payload;
	uint32_t	start;
	uint32_t	end;
	uint32_t	size;
	Protocol*	protocol;

	Packet(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);
	virtual ~Packet();

	friend std::ostream& operator<<(std::ostream& out, const Packet& obj);
	friend std::ostream& operator<<(std::ostream& out, const Packet* obj);
};

typedef Packetlet* (*packetlet)(int argc, char** argv);	// Packetlet* pv_packetlet();

}; // namespace pv
