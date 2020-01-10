#include <stdio.h>
#include <iostream>
#include <pv/driver.h>
#include <pv/packet.h>

namespace pv {

Packetlet::Packetlet() {
	id = pv_driver_add_packetlet((void*)this);
}

Packetlet::~Packetlet() {
	pv_driver_remove_packetlet((void*)this);
}

bool Packetlet::received(Packet* packet) {
	return false;
}

bool Packetlet::send(Packet* packet) {
	bool result = pv_driver_send(packet->queueId, packet->addr, packet->payload, packet->start, packet->end, packet->size);

	packet->payload = nullptr;
	delete packet;

	return result;
}

Packet::Packet(uint64_t addr, uint32_t size) {
	if(!pv_driver_alloc(&addr, &payload, size)) {
		throw "cannot allocate payload"; // TODO: change it to exception
	}

	queueId = -1;
	addr = addr;
	start = 0;
	end = size;
	this->size = size;
}

Packet::Packet(int32_t queueId, uint64_t addr, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size) {
	this->queueId = queueId;
	this->addr = addr;
	this->payload = payload;
	this->start = start;
	this->end = end;
	this->size = size;
}

Packet::~Packet() {
	if(payload != nullptr)
		pv_driver_free(addr);
}

std::ostream& operator<<(std::ostream& out, const Packet& obj) {
	out << &obj;

	return out;
}

std::ostream& operator<<(std::ostream& out, const Packet* obj) {
	out << "Packet[len: " << (obj->end - obj->start) << ", ";
	for(uint32_t i = obj->start; i < obj->end; i++) {
		char buf[4];
		sprintf(buf, "%02x ", obj->payload[i]);
		out << buf;
	}
	out << "]";

	return out;
}

};
