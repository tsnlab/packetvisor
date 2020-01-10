#include <stdio.h>
#include <string.h>
#include <pv/ethernet.h>

namespace pv {

Ethernet::Ethernet(Packet* packet) : Protocol(packet, 0) {
}

Ethernet::Ethernet(Protocol* parent) : Protocol(parent) {
}

Ethernet::~Ethernet() {
}

uint64_t Ethernet::getDst() const {
	CHECK(0, 6);
	return endian48(*(uint64_t*)OFFSET(0));
}

Ethernet* Ethernet::setDst(uint64_t mac) {
	CHECK(0, 6);
	mac = endian48(mac);
	memcpy(OFFSET(0), &mac, 6);

	return this;
}

uint64_t Ethernet::getSrc() const {
	CHECK(6, 6);
	return endian48(*(uint64_t*)(OFFSET(6)));
}

Ethernet* Ethernet::setSrc(uint64_t mac) {
	CHECK(6, 6);
	mac = endian48(mac);
	memcpy(OFFSET(6), &mac, 6);

	return this;
}

uint16_t Ethernet::getType() const {
	CHECK(12, 2);
	return endian16(*(uint16_t*)OFFSET(12));
}

Ethernet* Ethernet::setType(uint16_t type) {
	CHECK(12, 2);
	*(uint16_t*)OFFSET(12) = endian16(type);

	return this;
}

uint32_t Ethernet::getBodyOffset() const {
	uint16_t type = getType();

	switch(type) {
		case ETHER_TYPE_8021Q:
			return packet->start + ETHER_LEN + 2;
		case ETHER_TYPE_8021AD:
			return packet->start + ETHER_LEN + 4;
		default:
			return packet->start + ETHER_LEN;
	}
}

std::ostream& operator<<(std::ostream& out, const Ethernet& obj) {
	out << &obj;
	return out;
}

std::ostream& operator<<(std::ostream& out, const Ethernet* obj) {
	uint8_t* payload = obj->packet->payload + obj->packet->start;
	char buf[18];
	sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", 
		payload[0], payload[1], payload[2],
		payload[3], payload[4], payload[5]);

	out << "Ethernet[dst: " << buf;

	sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", 
		payload[6], payload[7], payload[8],
		payload[9], payload[10], payload[11]);
	out << ", src: " << buf;

	sprintf(buf, "%04x", obj->getType());
	out << ", type: " << buf;

	out << ", body: " << (obj->packet->end - obj->getBodyOffset()) << " bytes]";

	return out;
}

}; // namespace pv
