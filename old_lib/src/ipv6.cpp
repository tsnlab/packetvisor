#include <stdio.h>
#include <pv/ipv6.h>

#include <string.h>

namespace pv {

IPv6::IPv6(Packet* packet, uint32_t offset) : Protocol(packet, offset) {
}

IPv6::IPv6(Protocol* parent) : Protocol(parent) {
}

IPv6::~IPv6() {
}

uint8_t IPv6::getVersion() const {
	CHECK(0, 1);
	return (*OFFSET(0) >> 4) & 0x0f;
}

IPv6* IPv6::setVersion(uint8_t version) {
	CHECK(0, 1);
	*OFFSET(0) = ((version & 0x0f) << 4) | (*OFFSET(0) & 0x0f);
	return this;
}

uint8_t IPv6::getTrafficClass() const {
	CHECK(0, 2);
	return ((*OFFSET(0) & 0x0f) << 4) | ((*OFFSET(1) & 0xf0) >> 4);
}

IPv6* IPv6::setTrafficClass(uint8_t traffic_class) {
	CHECK(0, 2);
	*OFFSET(0) = (*OFFSET(0) & 0xf0) | ((traffic_class & 0xf0) >> 4);
	*OFFSET(1) = (*OFFSET(1) & 0x0f) | ((traffic_class & 0x0f) << 4);
	return this;
}

uint32_t IPv6::getFlowLabel() const {
	CHECK(1, 3);
	uint32_t label = (((uint32_t)*OFFSET(1)) << 16) | (((uint32_t)*OFFSET(2)) << 8) | *OFFSET(3);
	return label;
}

IPv6* IPv6::setFlowLabel(uint32_t flow_label) {
	CHECK(1, 3);
	*OFFSET(1) &= 0xf0;
	*OFFSET(1) |= (uint8_t)((flow_label >> 16) & 0x0f);
	*OFFSET(2) = (uint8_t)(flow_label >> 8);
	*OFFSET(3) = (uint8_t)flow_label;
	return this;
}

uint16_t IPv6::getPayLen() const {
	CHECK(4, 2);
	return endian16(*(uint16_t*)OFFSET(4));
}

IPv6* IPv6::setPayLen(uint16_t pay_len) {
	CHECK(4, 2);
	*(uint16_t*)OFFSET(4) = endian16(pay_len);
	return this;
}

uint8_t IPv6::getNextHdr() const {
	CHECK(6, 1);
	return *OFFSET(6);
}

IPv6* IPv6::setNextHdr(uint8_t next_hdr) {
	CHECK(6, 1);
	*OFFSET(6) = next_hdr;
	return this;
}

uint8_t IPv6::getHopLimit() const {
	CHECK(7, 1);
	return *OFFSET(7);
}

IPv6* IPv6::setHopLimit(uint8_t hop_limit) {
	CHECK(7, 1);
	*OFFSET(7) = hop_limit;
	return this;
}

struct ip6_addr IPv6::getSrc() const {
	CHECK(8, 16);
	struct ip6_addr src = {};
	src.u.addr32[0] = *(uint32_t*)OFFSET(8);
	src.u.addr32[1] = *(uint32_t*)OFFSET(12);
	src.u.addr32[2] = *(uint32_t*)OFFSET(16);
	src.u.addr32[3] = *(uint32_t*)OFFSET(20);
	return src;
}

IPv6* IPv6::setSrc(struct ip6_addr src) {
	CHECK(8, 16);
	*(uint32_t*)OFFSET(8) = src.u.addr32[0];
	*(uint32_t*)OFFSET(12) = src.u.addr32[1];
	*(uint32_t*)OFFSET(16) = src.u.addr32[2];
	*(uint32_t*)OFFSET(20) = src.u.addr32[3];
	return this;
}

struct ip6_addr IPv6::getDst() const {
	CHECK(24, 16);
	struct ip6_addr dst = {};
	dst.u.addr32[0] = *(uint32_t*)OFFSET(24);
	dst.u.addr32[1] = *(uint32_t*)OFFSET(28);
	dst.u.addr32[2] = *(uint32_t*)OFFSET(32);
	dst.u.addr32[3] = *(uint32_t*)OFFSET(36);
	return dst;
}

IPv6* IPv6::setDst(struct ip6_addr dst) {
	CHECK(24, 16);
	*(uint32_t*)OFFSET(24) = dst.u.addr32[0];
	*(uint32_t*)OFFSET(28) = dst.u.addr32[1];
	*(uint32_t*)OFFSET(32) = dst.u.addr32[2];
	*(uint32_t*)OFFSET(36) = dst.u.addr32[3];
	return this;
}

uint16_t IPv6::getPseudoChecksum() const {
	struct IPv6_Pseudo pseudo;
	pseudo.source = getSrc();
	pseudo.destination = getDst();
	pseudo.upper_len = endian32(getPayLen());
	memset(pseudo.zero, 0x00, 3);
	pseudo.next_hdr = getNextHdr();

	return (uint16_t)~Protocol::checksum((uint8_t*)&pseudo, 0, sizeof(pseudo));
}

uint32_t IPv6::getBodyOffset() const {
	return packet->start + offset + IP6_LEN;
}

std::ostream& operator<<(std::ostream& out, const IPv6& obj) {
	out << & obj;
	return out;
}

std::ostream& operator<<(std::ostream& out, const IPv6* obj) {
	char buf[40] = {};

	out << "IPv6[version: " << std::to_string(obj->getVersion());
	out << ", traffic_class: " << std::to_string(obj->getTrafficClass());
	out << ", flow_label: " << std::to_string(obj->getFlowLabel());
	out << ", pay_len: " << std::to_string(obj->getPayLen());
	out << ", next_hdr: " << std::to_string(obj->getNextHdr());
	out << ", hop_limit: " << std::to_string(obj->getHopLimit());
	struct ip6_addr src = obj->getSrc();
	sprintf(buf, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", 
			endian16(src.u.addr16[0]),	endian16(src.u.addr16[1]), 
			endian16(src.u.addr16[2]),	endian16(src.u.addr16[3]),
			endian16(src.u.addr16[4]),	endian16(src.u.addr16[5]),
			endian16(src.u.addr16[6]),	endian16(src.u.addr16[7]));
	out << ", src: " << buf;
	struct ip6_addr dst = obj->getDst();
	sprintf(buf, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x", 
			endian16(dst.u.addr16[0]),	endian16(dst.u.addr16[1]), 
			endian16(dst.u.addr16[2]),	endian16(dst.u.addr16[3]),
			endian16(dst.u.addr16[4]),	endian16(dst.u.addr16[5]),
			endian16(dst.u.addr16[6]),	endian16(dst.u.addr16[7]));
	out << ", dst: " << buf;

	return out;
}

}; // namespace pv
