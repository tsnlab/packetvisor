#include <stdio.h>
#include <iostream>
#include <pv/driver.h>
#include <pv/packet.h>
#include <pv/protocol.h>

namespace pv {

Packetlet::Packetlet() {
}

Packetlet::~Packetlet() {
}

void Packetlet::setDriver(Driver* driver) {
	printf("packetlet %p set driver %p\n", this, driver);
	this->driver = driver;
}

Packet* Packetlet::alloc(uint32_t size) {
	uint64_t addr;
	uint8_t* payload;

	if(!driver->alloc(&addr, &payload, size)) {
		return nullptr;
	}

	return new Packet(-1, addr, payload, 0, size, size);
}

void Packetlet::drop(Packet* packet) {
	driver->free(packet->addr);
	delete packet;
}

bool Packetlet::received(Packet* packet) {
	return false;
}

bool Packetlet::send(Packet* packet) {
	bool result = driver->send(packet->queueId, packet->addr, packet->payload, packet->start, packet->end, packet->size);

	delete packet;

	return result;
}

Packet::Packet(int32_t queueId, uint64_t addr, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size) {
	this->queueId = queueId;
	this->addr = addr;
	this->payload = payload;
	this->start = start;
	this->end = end;
	this->size = size;
	protocol = nullptr;
}

Packet::~Packet() {
	if(protocol != nullptr)
		delete protocol;
}

std::ostream& operator<<(std::ostream& out, const Packet& obj) {
	out << &obj;

	return out;
}

std::ostream& operator<<(std::ostream& out, const Packet* obj) {
	out << "Packet[payload: " << std::to_string((obj->end - obj->start)) << " bytes]";

	return out;
}

};
