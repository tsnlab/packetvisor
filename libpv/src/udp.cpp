#include <pv/udp.h>

namespace pv {

UDP::UDP(Packet* packet, uint32_t offset) : Protocol(packet, offset) {
}

UDP::UDP(Protocol* parent) : Protocol(parent) {
}

UDP::~UDP() {
}

uint16_t UDP::getSrcport() const {
	CHECK(0, 2);
	return endian16(*(uint16_t*)OFFSET(0));
}

UDP* UDP::setSrcport(uint16_t port) {
	CHECK(0, 2);
	*(uint16_t*)OFFSET(0) = endian16(port);
	return this;
}

uint16_t UDP::getDstport() const {
	CHECK(2, 2);
	return endian16(*(uint16_t*)OFFSET(2));
}

UDP* UDP::setDstport(uint16_t port) {
	CHECK(2, 2);
	*(uint16_t*)OFFSET(2) = endian16(port);
	return this;
}

uint16_t UDP::getLength() const {
	CHECK(4, 2);
	return endian16(*(uint16_t*)OFFSET(4));
}

UDP* UDP::setLength(uint16_t length) {
	CHECK(4, 2);
	*(uint16_t*)OFFSET(4) = endian16(length);
	return this;
}

uint16_t UDP::getChecksum() const {
	CHECK(6, 2);
	return endian16(*(uint16_t*)OFFSET(6));
}

UDP* UDP::setChecksum(uint16_t checksum) {
	CHECK(6, 2);
	*(uint16_t*)OFFSET(6) = endian16(checksum);
	return this;
}

UDP* UDP::checksum() {
	CHECK(6, 2);
	*(uint16_t*)OFFSET(6) = 0;
	uint32_t len = getLength();
	uint32_t len2 = getEnd() - getBodyOffset();
	if(len2 < len)
		len = len2;

	*(uint16_t*)OFFSET(6) = Protocol::checksum(0, len);
	return this;
}

uint32_t UDP::getBodyOffset() const {
	return packet->start + offset + UDP_LEN;
}

std::ostream& operator<<(std::ostream& out, const UDP& obj) {
	out << &obj;
	return out;
}

std::ostream& operator<<(std::ostream& out, const UDP* obj) {
	char buf[6];

	out << "UDP[srcport: " << std::to_string(obj->getSrcport());
	out << ", dstport: " << std::to_string(obj->getDstport());
	out << ", length: " << std::to_string(obj->getLength());
	sprintf(buf, "%04x", obj->getChecksum());
	out << ", checksum: " << buf;
	out << ", body: " << std::to_string(obj->packet->end - obj->getBodyOffset()) << " bytes]";

	return out;
}

}; // namespace pv
