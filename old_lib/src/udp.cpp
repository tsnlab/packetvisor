#include <pv/ipv4.h>
#include <pv/udp.h>

namespace pv {

struct UDP_Pseudo {
	uint32_t	source;			///< Source address (endian32)
	uint32_t	destination;	///< Destination address (endian32)
	uint8_t		padding;		///< Zero padding
	uint8_t		protocol;		///< UDP protocol number, 0x11
	uint16_t	length;			///< Header and data length in bytes (endian16)
} __attribute__((packed));

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

UDP* UDP::checksumWithPseudo(uint16_t pseudo_checksum) {
	CHECK(6, 2);
	*(uint16_t*)OFFSET(6) = 0;
	uint32_t sum = pseudo_checksum + (uint16_t)~Protocol::checksum(0, getLength());
	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	*(uint16_t*)OFFSET(6) = endian16(~sum);

	return this;
}

UDP* UDP::checksum() {
	CHECK(6, 2);
	*(uint16_t*)OFFSET(6) = 0;

	IPv4* ip = (IPv4*)parent;
	uint16_t len = getLength();
	UDP_Pseudo pseudo;
	pseudo.source = endian32(ip->getSrc());
	pseudo.destination = endian32(ip->getDst());
	pseudo.padding = 0;
	pseudo.protocol = ip->getProto();
	pseudo.length = endian16(len);
	
	uint32_t sum = (uint16_t)~Protocol::checksum((uint8_t*)&pseudo, 0, sizeof(pseudo)) + (uint16_t)~Protocol::checksum(0, len);
	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	*(uint16_t*)OFFSET(6) = endian16(~sum);

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
