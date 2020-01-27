#include <stdio.h>
#include <pv/ipv4.h>

namespace pv {

IPv4::IPv4(Packet* packet, uint32_t offset) : Protocol(packet, offset) {
}

IPv4::IPv4(Protocol* parent) : Protocol(parent) {
}

IPv4::~IPv4() {
}

uint8_t IPv4::getVersion() const {
	CHECK(0, 1);
	return (*OFFSET(0) >> 4) & 0x0f;
}

IPv4* IPv4::setVersion(uint8_t version) {
	CHECK(0, 1);
	*OFFSET(0) = ((version & 0x0f) << 4) | (*OFFSET(0) & 0x0f);
	return this;
}

uint8_t IPv4::getHdrLen() const {
	CHECK(0, 1);
	return *OFFSET(0) & 0x0f;
}

IPv4* IPv4::setHdrLen(uint8_t hdr_len) {
	CHECK(0, 1);
	*OFFSET(0) = (hdr_len & 0x0f) | (*OFFSET(0) & 0xf0);
	return this;
}

uint8_t IPv4::getDscp() const {
	CHECK(1, 1);
	return *OFFSET(1) >> 2;
}

IPv4* IPv4::setDscp(uint8_t dscp) {
	CHECK(1, 1);
	*OFFSET(1) = (dscp << 2) | (*OFFSET(1) & 0x03);
	return this;
}

uint8_t IPv4::getEcn() const {
	CHECK(1, 1);
	return *OFFSET(1) & 0x03;
}

IPv4* IPv4::setEcn(uint8_t ecn) {
	CHECK(1, 1);
	*OFFSET(1) = (ecn & 0x03) | (*OFFSET(1) & 0xfc);
	return this;
}

uint16_t IPv4::getLen() const {
	CHECK(2, 2);
	return endian16(*(uint16_t*)OFFSET(2));
}

IPv4* IPv4::setLen(uint16_t len) {
	CHECK(2, 2);
	*(uint16_t*)OFFSET(2) = endian16(len);
	return this;
}

uint16_t IPv4::getId() const {
	CHECK(4, 2);
	return endian16(*(uint16_t*)OFFSET(4));
}

IPv4* IPv4::setId(uint16_t id) {
	CHECK(4, 2);
	*(uint16_t*)OFFSET(4) = endian16(id);
	return this;
}

uint8_t IPv4::getFlags() const {
	CHECK(6, 1);
	return *OFFSET(6) >> 5;
}

IPv4* IPv4::setFlags(uint8_t flags) {
	CHECK(6, 1);
	*OFFSET(6) = ((flags & 0x03) << 5) | (*OFFSET(6) & 0x1f);
	return this;
}

bool IPv4::isRb() const {
	CHECK(6, 1);
	return !!(*OFFSET(6) & 0x80);
}

IPv4* IPv4::setRb(bool rb) {
	CHECK(6, 1);
	*OFFSET(6) = (rb ? (uint8_t)0x80 : (uint8_t)0x00) | (*OFFSET(6) & 0x7f);
	return this;
}

bool IPv4::isDf() const {
	CHECK(6, 1);
	return !!(*OFFSET(6) & 0x40);
}

IPv4* IPv4::setDf(bool df) {
	CHECK(6, 1);
	*OFFSET(6) = (df ? (uint8_t)0x40 : (uint8_t)0x00) | (*OFFSET(6) & 0xbf);
	return this;
}

bool IPv4::isMf() const {
	CHECK(6, 1);
	return !!(*OFFSET(6) & 0x20);
}

IPv4* IPv4::setMf(bool mf) {
	CHECK(6, 1);
	*OFFSET(6) = (mf ? (uint8_t)0x20 : (uint8_t)0x00) | (*OFFSET(6) & 0xdf);
	return this;
}

uint16_t IPv4::getFragOffset() const {
	CHECK(6, 2);
	return (uint16_t)(*OFFSET(6) & 0x1f) | (uint16_t)*OFFSET(7);
}

IPv4* IPv4::setFragOffset(uint16_t frag_offset) {
	CHECK(6, 2);
	*OFFSET(6) = ((frag_offset & 0x1f00) >> 8) | (*OFFSET(6) & 0xe0);
	*OFFSET(7) = (uint8_t)(frag_offset & 0xff);
	return this;
}

uint8_t IPv4::getTtl() const {
	CHECK(8, 1);
	return *OFFSET(8);
}

IPv4* IPv4::setTtl(uint8_t ttl) {
	CHECK(8, 1);
	*OFFSET(8) = ttl;
	return this;
}

uint8_t IPv4::getProto() const {
	CHECK(9, 1);
	return *OFFSET(9);
}

IPv4* IPv4::setProto(uint8_t proto) {
	CHECK(9, 1);
	*OFFSET(9) = proto;
	return this;
}

uint16_t IPv4::getChecksum() const {
	CHECK(10, 2);
	return endian16(*(uint16_t*)OFFSET(10));
}

IPv4* IPv4::setChecksum(uint16_t checksum) {
	CHECK(10, 2);
	*(uint16_t*)OFFSET(10) = endian16(checksum);
	return this;
}

IPv4* IPv4::checksum() {
	CHECK(10, 2);
	*(uint16_t*)OFFSET(10) = 0;
	*(uint16_t*)OFFSET(10) = endian16(Protocol::checksum(0, getHdrLen() * 4));
	return this;
}

uint32_t IPv4::getSrc() const {
	CHECK(12, 4);
	return endian32(*(uint32_t*)OFFSET(12));
}

IPv4* IPv4::setSrc(uint32_t src) {
	CHECK(12, 4);
	*(uint32_t*)OFFSET(12) = endian32(src);
	return this;
}

uint32_t IPv4::getDst() const {
	CHECK(16, 4);
	return endian32(*(uint32_t*)OFFSET(16));
}

IPv4* IPv4::setDst(uint32_t dst) {
	CHECK(16, 4);
	*(uint32_t*)OFFSET(16) = endian32(dst);
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
	sprintf(buf, "0x%04x", obj->getId());
	out << ", id: " << buf;
	sprintf(buf, "0x%01x", obj->getFlags());
	out << ", flags: " << buf;
	out << ", flags.rb: " << (obj->isRb() ? 1 : 0);
	out << ", flags.df: " << (obj->isDf() ? 1 : 0);
	out << ", flags.mf: " << (obj->isMf() ? 1 : 0);
	out << ", frag_offset: " << obj->getFragOffset();
	out << ", ttl: " << std::to_string(obj->getTtl());
	sprintf(buf, "0x%02x", obj->getProto());
	out << ", proto: " << buf;
	sprintf(buf, "0x%04x", obj->getChecksum());
	out << ", checksum: " << buf;
	uint32_t addr = obj->getSrc();
	out << ", src: " << std::to_string((addr >> 24) & 0xff) << ".";
	out << std::to_string((addr >> 16) & 0xff) << ".";
	out << std::to_string((addr >> 8) & 0xff) << ".";
	out << std::to_string((addr >> 0) & 0xff);
	addr = obj->getDst();
	out << ", dst: " << std::to_string((addr >> 24) & 0xff) << ".";
	out << (std::to_string((addr >> 16) & 0xff)) << ".";
	out << std::to_string((addr >> 8) & 0xff) << ".";
	out << std::to_string((addr >> 0) & 0xff);
	out << ", body_offset: " << obj->getBodyOffset();
	out << ", body: " << std::to_string(obj->getEnd() - obj->getBodyOffset()) << " bytes]";

	return out;
}

}; // namespace pv
