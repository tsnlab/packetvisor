#include <pv/icmp.h>

namespace pv {

ICMP::ICMP(Packet* packet, uint32_t offset) : Protocol(packet, offset) {
}

ICMP::ICMP(Protocol* parent) : Protocol(parent) {
}

ICMP::~ICMP() {
}

uint8_t ICMP::getType() const {
	CHECK(0, 1);
	return *OFFSET(0);
}

ICMP* ICMP::setType(uint8_t type) {
	CHECK(0, 1);
	*OFFSET(0) = type;
	return this;
}

uint8_t ICMP::getCode() const {
	CHECK(1, 1);
	return *OFFSET(1);
}

ICMP* ICMP::setCode(uint8_t code) {
	CHECK(1, 1);
	*OFFSET(1) = code;
	return this;
}

uint16_t ICMP::getChecksum() const {
	CHECK(2, 2);
	return endian16(*(uint16_t*)OFFSET(2));
}

ICMP* ICMP::setChecksum(uint16_t checksum) {
	CHECK(2, 2);
	*(uint16_t*)OFFSET(2) = endian16(checksum);
	return this;
}

uint32_t ICMP::getBodyOffset() const {
	return packet->start + offset + ICMP_LEN;
}

std::ostream& operator<<(std::ostream& out, const ICMP& obj) {
	out << &obj;
	return out;
}

std::ostream& operator<<(std::ostream& out, const ICMP* obj) {
	char buf[6];

	out << "ICMP[type: " << std::to_string(obj->getType());
	out << ", code: " << std::to_string(obj->getCode());
	sprintf(buf, "%04x", obj->getChecksum());
	out << ", checksum: " << buf;
	out << ", rest: " << std::to_string(obj->packet->end - obj->getBodyOffset()) << " bytes, ";
	for(uint32_t i = obj->getBodyOffset(); i < obj->packet->end; i++) {
		sprintf(buf, "%02x ", obj->getPacket()->payload[i]);
		out << buf;
	}
	out << "]";

	return out;
}

}; // namespace pv
