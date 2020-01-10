#include <string.h>
#include <pv/arp.h>

namespace pv {

ARP::ARP(Packet* packet, uint32_t offset) : Protocol(packet, offset) {
}

ARP::ARP(Protocol* parent) : Protocol(parent) {
}

ARP::~ARP() {
}

uint16_t ARP::getHwType() const {
	CHECK(0, 2);
	return endian16(*(uint16_t*)OFFSET(0));
}

ARP* ARP::setHwType(uint16_t type) {
	CHECK(0, 2);
	*(uint16_t*)OFFSET(0) = endian16(type);
	return this;
}

uint16_t ARP::getProtoType() const {
	CHECK(2, 2);
	return endian16(*(uint16_t*)OFFSET(2));
}

ARP* ARP::setProtoType(uint16_t type) {
	CHECK(2, 2);
	*(uint16_t*)OFFSET(2) = endian16(type);
	return this;
}

uint8_t ARP::getHwSize() const {
	CHECK(4, 1);
	return *OFFSET(4);
}

ARP* ARP::setHwSize(uint8_t size) {
	CHECK(4, 1);
	*OFFSET(2) = size;
	return this;
}

uint8_t ARP::getProtoSize() const {
	CHECK(5, 1);
	return *OFFSET(5);
}

ARP* ARP::setProtoSize(uint8_t size) {
	CHECK(5, 1);
	*OFFSET(2) = size;
	return this;
}

uint16_t ARP::getOpcode() const {
	CHECK(6, 2);
	return endian16(*(uint16_t*)OFFSET(6));
}

ARP* ARP::setOpcode(uint16_t opcode) {
	CHECK(6, 2);
	*(uint16_t*)OFFSET(6) = endian16(opcode);
	return this;
}

uint64_t ARP::getSrcHw() const {
	CHECK(8, 6);
	return endian48(*(uint64_t*)OFFSET(8));
}

ARP* ARP::setSrcHw(uint64_t addr) {
	CHECK(8, 6);
	addr = endian48(addr);
	memcpy(OFFSET(8), &addr, 6);
	return this;
}

uint32_t ARP::getSrcProto() const {
	CHECK(14, 4);
	return endian32(*(uint32_t*)OFFSET(14));
}

ARP* ARP::setSrcProto(uint32_t addr) {
	CHECK(14, 4);
	*(uint32_t*)OFFSET(14) = endian32(addr);
	return this;
}

uint64_t ARP::getDstHw() const {
	CHECK(18, 6);
	return endian48(*(uint64_t*)OFFSET(18));
}

ARP* ARP::setDstHw(uint64_t addr) {
	CHECK(18, 6);
	addr = endian48(addr);
	memcpy(OFFSET(18), &addr, 6);
	return this;
}

uint32_t ARP::getDstProto() const {
	CHECK(24, 4);
	return endian32(*(uint32_t*)OFFSET(24));
}

ARP* ARP::setDstProto(uint32_t addr) {
	CHECK(24, 4);
	*(uint32_t*)OFFSET(24) = endian32(addr);
	return this;
}

std::ostream& operator<<(std::ostream& out, const ARP& obj) {
	out << &obj;
	return out;
}

std::ostream& operator<<(std::ostream& out, const ARP* obj) {
	char buf[6];
	sprintf(buf, "%04x", obj->getHwType());
	out << "ARP[hw_type: " << buf;
	sprintf(buf, "%04x", obj->getProtoType());
	out << ", proto_type: " << buf;
	out << ", hw_size: " << obj->getHwSize();
	out << ", proto_size: " << obj->getProtoSize();
	out << ", opcode: " << obj->getOpcode();
	out << ", src_hw: ";
	for(int i = 0; i < 6; i++) {
		sprintf(buf, "%02x", *obj->OFFSET(8 + i));
		out << buf;
		if(i + 1 < 6)
			out << ":";
	}

	uint32_t addr = obj->getSrcProto();
	out << ", src_proto: " << std::to_string((addr >> 24) & 0xff) << ".";
	out << std::to_string((addr >> 16) & 0xff) << ".";
	out << std::to_string((addr >> 8) & 0xff) << ".";
	out << std::to_string((addr >> 0) & 0xff);

	out << ", dst_hw: ";
	for(int i = 0; i < 6; i++) {
		sprintf(buf, "%02x", *obj->OFFSET(18 + i));
		out << buf;
		if(i + 1 < 6)
			out << ":";
	}

	addr = obj->getDstProto();
	out << ", dst_proto: " << std::to_string((addr >> 24) & 0xff) << ".";
	out << std::to_string((addr >> 16) & 0xff) << ".";
	out << std::to_string((addr >> 8) & 0xff) << ".";
	out << std::to_string((addr >> 0) & 0xff);

	out << "]";

	return out;
}

}; // namespace pv
