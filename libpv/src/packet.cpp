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

void Packetlet::send(Packet* packet) {
	pv_driver_send(packet->queueId, packet->payload, packet->start, packet->end, packet->size);
	delete packet;
}

Packet::Packet(uint32_t size) {
	payload = pv_driver_alloc(size);
	if(payload == nullptr)
		throw "cannot allocate payload";

	queueId = -1;
	start = 0;
	end = size;
	this->size = size;
}

Packet::Packet(int32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size) {
	this->queueId = queueId;
	this->payload = payload;
	this->start = start;
	this->end = end;
	this->size = size;
}

Packet::~Packet() {
	if(payload != nullptr)
		pv_driver_free(payload);
}

};
