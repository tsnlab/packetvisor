#include <stdio.h>
#include <pv/ipv4.h>

namespace pv {

#define OFFSET(pos)	(packet->payload + packet->start + offset + pos)

IPv4::IPv4(Packet* packet, uint32_t offset) {
	this->packet = packet;
	this->offset = offset;
}

IPv4::~IPv4() {
}

uint8_t IPv4::getVersion() const {
	return *(uint8_t*)OFFSET(0) & 0x0f;
}

IPv4* IPv4::setVersion(uint8_t ver) {
	*(uint8_t*)OFFSET(0) |= ver & 0x0f;
	return this;
}

uint8_t IPv4::getHdrLen() const {
	return (*(uint8_t*)OFFSET(0) & 0xf0) >> 4;
}

IPv4* IPv4::setHdrLen(uint8_t hdr_len) {
	*(uint8_t*)OFFSET(0) |= (hdr_len & 0x0f) << 4;
	return this;
}

uint8_t IPv4::getDscp() const {
	return *(uint8_t*)OFFSET(1) & 0x3f;
}

IPv4* IPv4::setDscp(uint8_t dscp) {
	*(uint8_t*)OFFSET(1) |= dscp & 0x3f;
	return this;
}

uint8_t IPv4::getEcn() const {
	return (*(uint8_t*)OFFSET(1) & 0xc0) >> 6;
}

IPv4* IPv4::setEcn(uint8_t ecn) {
	*(uint8_t*)OFFSET(1) |= (ecn& 0x03) << 6;
	return this;
}

uint16_t IPv4::getLen() const {
	return endian16(*(uint16_t*)OFFSET(2));
}

IPv4* IPv4::setLen(uint16_t len) {
	*(uint16_t*)OFFSET(2) = endian16(len);
	return this;
}

uint16_t IPv4::getId() const {
	return endian16(*(uint16_t*)OFFSET(3));
}

IPv4* IPv4::setId(uint16_t id) {
	*(uint16_t*)OFFSET(3) = endian16(id);
	return this;
}

uint8_t IPv4::getFlags() const {
	return *(uint8_t*)OFFSET(5) & 0x08;
}

IPv4* IPv4::setFlags(uint8_t flags) {
	*(uint8_t*)OFFSET(5) |= flags & 0x08;
	return this;
}

bool IPv4::isRb() const {
	return !!*(uint8_t*)OFFSET(6) & 0x01;
}

IPv4* IPv4::setRb(bool rb) {
	*(uint8_t*)OFFSET(6) |= !!rb;
	return this;
}

bool IPv4::isDf() const {
	return !!(*(uint8_t*)OFFSET(6) & 0x02);
}

IPv4* IPv4::setDf(bool df) {
	*(uint8_t*)OFFSET(6) |= !!df << 1;
	return this;
}

bool IPv4::isMf() const {
	return !!(*(uint8_t*)OFFSET(6) & 0x04);
}

IPv4* IPv4::setMf(bool mf) {
	*(uint8_t*)OFFSET(6) |= !!mf << 2;
	return this;
}

uint16_t IPv4::getFragOffset() const {
	return endian16(*(uint16_t*)OFFSET(7) >> 3);
}

IPv4* IPv4::setFragOffset(uint16_t frag_offset) {
	*(uint16_t*)OFFSET(7) = (frag_offset << 3) | (*(uint16_t*)OFFSET(7) & 0x07);
	return this;
}

uint8_t IPv4::getTtl() const {
	return *(uint8_t*)OFFSET(8);
}

IPv4* IPv4::setTtl(uint8_t ttl) {
	*(uint8_t*)OFFSET(8) = ttl;
	return this;
}

uint8_t IPv4::getProto() const {
	return *(uint8_t*)OFFSET(9);
}

IPv4* IPv4::setProto(uint8_t proto) {
	*(uint8_t*)OFFSET(9) = proto;
	return this;
}

uint16_t IPv4::getChecksum() const {
	return endian16(*(uint16_t*)OFFSET(10));
}

IPv4* IPv4::setChecksum(uint16_t checksum) {
	*(uint16_t*)OFFSET(10) = endian16(checksum);
	return this;
}

uint32_t IPv4::getSrc() const {
	return endian32(*(uint16_t*)OFFSET(12));
}

IPv4* IPv4::setSrc(uint32_t src) {
	*(uint32_t*)OFFSET(12) = endian32(src);
	return this;
}

uint32_t IPv4::getDst() const {
	return endian32(*(uint16_t*)OFFSET(14));
}

IPv4* IPv4::setDst(uint32_t dst) {
	*(uint32_t*)OFFSET(14) = endian32(dst);
	return this;
}

uint32_t IPv4::getBodyOffset() const {
	return packet->start + offset + getHdrLen() * 4;
}

std::ostream& operator<<(std::ostream& out, const IPv4& obj) {
	out << &obj;
	return out;
}

std::ostream& operator<<(std::ostream& out, const IPv4* obj) {
	char buf[10];

	out << "IPv4[version: " << std::to_string(obj->getVersion());
	out << ", hdr_len: " << std::to_string(obj->getHdrLen());
	out << ", dscp: " << std::to_string(obj->getDscp());
	out << ", ecn: " << std::to_string(obj->getEcn());
	out << ", len: " << std::to_string(obj->getLen());
	sprintf(buf, "%04x", obj->getId());
	out << ", id: " << buf;
	sprintf(buf, "%01x", obj->getFlags());
	out << ", id: " << buf;
	out << ", flags.rb: " << (obj->isRb() ? 1 : 0);
	out << ", flags.df: " << (obj->isDf() ? 1 : 0);
	out << ", flags.mf: " << (obj->isMf() ? 1 : 0);
	out << ", frag_offset: " << obj->getFragOffset();
	out << ", ttl: " << std::to_string(obj->getTtl());
	sprintf(buf, "%04x", obj->getProto());
	out << ", proto: " << buf;
	sprintf(buf, "%08x", obj->getChecksum());
	out << ", checksum: " << buf;
	uint32_t addr = obj->getSrc();
	out << ", src: " << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << ".";
	out << ((addr >> 8) & 0xff) << "." << ((addr >> 0) & 0xff);
	addr = obj->getDst();
	out << ", dst: " << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << ".";
	out << ((addr >> 8) & 0xff) << "." << ((addr >> 0) & 0xff);
	out << ", body_offset: " << obj->getBodyOffset();

	return out;
}

}; // namespace pv
