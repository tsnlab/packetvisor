#include <stdio.h>
#include <string.h>
#include <pv/ethernet.h>

namespace pv {

Ethernet::Ethernet(Packet* packet) {
	this->packet = packet;
}

Ethernet::~Ethernet() {
}

uint64_t Ethernet::getDst() const {
	return endian48(*(uint64_t*)(packet->payload + packet->start + 0));
}

Ethernet* Ethernet::setDst(uint64_t mac) {
	mac = endian48(mac);
	memcpy(packet->payload + packet->start + 0, &mac, 6);

	return this;
}

uint64_t Ethernet::getSrc() const {
	return endian48(*(uint64_t*)(packet->payload + packet->start + 6));
}

Ethernet* Ethernet::setSrc(uint64_t mac) {
	mac = endian48(mac);
	memcpy(packet->payload + packet->start + 6, &mac, 6);

	return this;
}

uint16_t Ethernet::getType() const {
	return endian16(*(uint16_t*)(packet->payload + packet->start + 12));
}

Ethernet* Ethernet::setType(uint16_t type) {
	*(uint16_t*)(packet->payload + packet->start + 12) = endian16(type);

	return this;
}

uint32_t Ethernet::getPayloadOffset() const {
	uint16_t type = getType();

	switch(type) {
		case ETHER_TYPE_8021Q:
			return packet->start + 16;
		case ETHER_TYPE_8021AD:
			return packet->start + 18;
		default:
			return packet->start + 14;
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

	out << ", payload: " << (obj->packet->end - obj->getPayloadOffset()) << " bytes";

	return out;
}

}; // namespace pv
