#include <stdio.h>
#include <pv/protocol.h>

namespace pv {

Protocol::Protocol(Packet* packet, uint32_t offset) {
	this->parent = nullptr;
	this->packet = packet;
	this->offset = offset;
	packet->protocol = this;
}

Protocol::Protocol(Protocol* parent) : Protocol(parent->getPacket(), parent->getBodyOffset()) {
	this->parent = parent;
}

Protocol::~Protocol() {
	if(parent != nullptr)
		delete parent;
}

Packet* Protocol::getPacket() const {
	return packet;
}

Protocol* Protocol::setPacket(Packet* packet) {
	this->packet = packet;
	return this;
}

uint32_t Protocol::getStart() const {
	return packet->start;
}

Protocol* Protocol::setStart(uint32_t start) {
	packet->start = start;
	return this;
}

uint32_t Protocol::getEnd() const {
	return packet->end;
}

Protocol* Protocol::setEnd(uint32_t end) {
	packet->end = end;
	return this;
}

uint32_t Protocol::getSize() const {
	return packet->size;
}

Protocol* Protocol::setSize(uint32_t size) {
	packet->size = size;
	return this;
}

uint32_t Protocol::getOffset() const {
	return offset;
}

Protocol* Protocol::setOffset(uint32_t offset) {
	this->offset = offset;
	return this;
}

uint32_t Protocol::getBodyOffset() const {
	return getEnd();
}

void Protocol::CHECK(uint32_t pos, uint32_t size) const {
	if(packet->start + offset + pos + size > packet->end) {
		char buf[160];
		sprintf(buf, "Buffer overflow start(%d) + offset(%d) + pos(%d) + size(%d) > end(%d)", packet->start, offset, pos, size, packet->end);
		throw std::overflow_error(buf);
	}
}

uint8_t* Protocol::OFFSET(uint32_t pos) const {
	return packet->payload + packet->start + offset + pos;
}

uint16_t Protocol::checksum(uint8_t* payload, uint32_t offset, uint32_t size) {
	uint32_t sum = 0;
	uint16_t* p = (uint16_t*)(payload + offset);
	
	while(size > 1) {
		sum += *p++;
		if(sum >> 16)
			sum = (sum & 0xffff) + (sum >> 16);
		
		size -= 2;
	}
	
	if(size > 0)
		sum += *(uint8_t*)p;
	
	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	
	return endian16((uint16_t)~sum);
}

uint16_t Protocol::checksum(uint32_t offset, uint32_t size) {
	CHECK(offset, size);

	return Protocol::checksum(OFFSET(offset), 0, size);
}

}; // namespace pv
