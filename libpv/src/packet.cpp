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

void Packetlet::init(Driver* driver) {
	this->driver = driver;
}

void Packetlet::setId(int32_t id) {
	this->id = id;
}

int32_t Packetlet::getId() {
	return id;
}

void Packetlet::setHandle(void* handle) {
	this->handle = handle;
}

void* Packetlet::getHandle() {
	return handle;
}

Packet* Packetlet::alloc() {
	uint8_t* payload = driver->alloc();
	if(payload == nullptr)
		return nullptr;

	return new Packet(0, payload, 0, driver->getPayloadSize(), driver->getPayloadSize());
}

void Packetlet::drop(Packet* packet) {
	driver->free(packet->payload);
	delete packet;
}

bool Packetlet::received(Packet* packet) {
	return false;
}

bool Packetlet::send(Packet* packet) {
	bool result = driver->send(packet->queueId, packet->payload, packet->start, packet->end);

	delete packet;

	return result;
}

Packet::Packet(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size) {
	this->queueId = queueId;
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
	out << "Packet[queueId: " << std::to_string(obj->queueId) << ", payload: " << std::to_string((obj->end - obj->start)) << " bytes]";

	return out;
}

};
