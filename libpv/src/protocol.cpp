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

uint16_t Protocol::checksum(uint32_t offset, uint32_t size) {
	uint32_t sum = 0;
	uint16_t* p = (uint16_t*)OFFSET(offset);
	
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

}; // namespace pv
