#include <stdio.h>
#include <pv/protocol.h>

namespace pv {

Protocol::Protocol() {
}

Protocol::Protocol(Packet* packet, uint32_t offset) {
	this->packet = packet;
	this->offset = offset;
}

Protocol::~Protocol() {
}

Packet* Protocol::getPacket() {
	return packet;
}

Protocol* Protocol::setPacket(Packet* packet) {
	this->packet = packet;
	return this;
}

uint32_t Protocol::getOffset() {
	return offset;
}

Protocol* Protocol::setOffset(uint32_t offset) {
	this->offset = offset;
	return this;
}

void Protocol::CHECK(uint32_t pos, uint32_t size) const {
	if(packet->start + offset + pos + size >= packet->end) {
		char buf[80];
		sprintf(buf, "Buffer overflow pos(%d) + size(%d) >= end(%d)", pos, size, packet->end);
		printf("exception: %s\n", buf);
		throw new std::overflow_error(buf);
	}
}

uint8_t* Protocol::OFFSET(uint32_t pos) const {
	return packet->payload + packet->start + offset + pos;
}

}; // namespace pv
