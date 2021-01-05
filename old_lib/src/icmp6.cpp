#include <string.h>
#include <pv/ipv6.h>
#include <pv/icmp6.h>

namespace pv {

ICMP6::ICMP6(Packet* packet, uint32_t offset) : Protocol(packet, offset) {
}

ICMP6::ICMP6(Protocol* parent) : Protocol(parent) {
}

ICMP6::~ICMP6() {
}

uint8_t ICMP6::getType() const {
	CHECK(0, 1);
	return *OFFSET(0);
}

ICMP6* ICMP6::setType(uint8_t type) {
	CHECK(0, 1);
	*OFFSET(0) = type;
	return this;
}

uint8_t ICMP6::getCode() const {
	CHECK(1, 1);
	return *OFFSET(1);
}

ICMP6* ICMP6::setCode(uint8_t code) {
	CHECK(1, 1);
	*OFFSET(1) = code;
	return this;
}

uint16_t ICMP6::getChecksum() const {
	CHECK(2, 2);
	return endian16(*(uint16_t*)OFFSET(2));
}

ICMP6* ICMP6::setChecksum(uint16_t checksum) {
	CHECK(2, 2);
	*(uint16_t*)OFFSET(2) = endian16(checksum);
	return this;
}

ICMP6* ICMP6::checksum() {
	CHECK(2, 2);
	*(uint16_t*)OFFSET(2) = 0;

	IPv6* ip = (IPv6*)parent;
	uint32_t pay_len = ip->getPayLen();
	struct IPv6_Pseudo pseudo;
	pseudo.source = ip->getSrc();
	pseudo.destination = ip->getDst();
	pseudo.upper_len = endian32(pay_len);
	memset(pseudo.zero, 0x00, 3);
	pseudo.next_hdr = IP_PROTOCOL_ICMP6;

	uint32_t sum = (uint16_t)~Protocol::checksum((uint8_t*)&pseudo, 0, sizeof(pseudo)) + (uint16_t)~Protocol::checksum(0, pay_len);
	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	*(uint16_t*)OFFSET(2) = endian16(~sum);

	return this;
}

uint32_t ICMP6::getBodyOffset() const {
	return packet->start + offset + ICMP6_LEN;
}

uint8_t ICMP6::getRFlag() const {
	CHECK(4, 1);
	return (*OFFSET(4) & 0x80) >> 7;
}

ICMP6* ICMP6::setRFlag(uint8_t r) {
	CHECK(4, 1);
	*OFFSET(4) = (*OFFSET(4) & 0x7f) | (r << 7);
	return this;
}

uint8_t ICMP6::getSFlag() const {
	CHECK(4, 1);
	return (*OFFSET(4) & 0x40) >> 6;
}

ICMP6* ICMP6::setSFlag(uint8_t s) {
	CHECK(4, 1);
	*OFFSET(4) = (*OFFSET(4) & 0xbf) | (s << 6);
	return this;
}

uint8_t ICMP6::getOFlag() const {
	CHECK(4, 1);
	return (*OFFSET(4) & 0x20) >> 5;
}

ICMP6* ICMP6::setOFlag(uint8_t o) {
	CHECK(4, 1);
	*OFFSET(4) = (*OFFSET(4) & 0xdf) | (o << 5);
	return this;
}

ICMP6* ICMP6::clearReserved() {
	CHECK(4, 1);
	*OFFSET(4) = *OFFSET(4) & 0xe0;
	memset(OFFSET(5), 0x00, 3);
}

struct ip6_addr ICMP6::getTarget() const {
	CHECK(8, 16);
	struct ip6_addr target = {};
	target.u.addr32[0] = *(uint32_t*)OFFSET(8);
	target.u.addr32[1] = *(uint32_t*)OFFSET(12);
	target.u.addr32[2] = *(uint32_t*)OFFSET(16);
	target.u.addr32[3] = *(uint32_t*)OFFSET(20);
	return target;
}

ICMP6* ICMP6::setTarget(struct ip6_addr target) {
	CHECK(8, 16);
	*(uint32_t*)OFFSET(8) = target.u.addr32[0];
	*(uint32_t*)OFFSET(12) = target.u.addr32[1];
	*(uint32_t*)OFFSET(16) = target.u.addr32[2];
	*(uint32_t*)OFFSET(20) = target.u.addr32[3];
	return this;
}

uint8_t ICMP6::getOptType() const {
	CHECK(24, 1);
	return *OFFSET(24);
}

ICMP6* ICMP6::setOptType(uint8_t type) {
	CHECK(24, 1);
	*OFFSET(24) = type;
	return this;
}

uint8_t ICMP6::getOptLen() const {
	CHECK(25, 1);
	return *OFFSET(25);
}

ICMP6* ICMP6::setOptLen(uint8_t len) {
	CHECK(25, 1);
	*OFFSET(25) = len;
	return this;
}

uint64_t ICMP6::getOptLinkAddr() const {
	CHECK(26, 6);
	return endian48(*(uint64_t*)OFFSET(26));
}

ICMP6* ICMP6::setOptLinkAddr(uint64_t link_addr) {
	CHECK(26, 6);
	link_addr = endian48(link_addr);
	memcpy(OFFSET(26), &link_addr, 6);
	return this;
}

uint16_t ICMP6::getId() const {
	CHECK(4, 2);
	return endian16(*(uint16_t*)OFFSET(4));
}

ICMP6* ICMP6::setId(uint16_t id) {
	CHECK(4, 2);
	*(uint16_t*)OFFSET(4) = endian16(id);
	return this;
}

uint16_t ICMP6::getSeq() const {
	CHECK(6, 2);
	return endian16(*(uint16_t*)OFFSET(6));
}

ICMP6* ICMP6::setSeq(uint16_t seq) {
	CHECK(6, 2);
	*(uint16_t*)OFFSET(6) = endian16(seq);
	return this;
}


std::ostream& operator<<(std::ostream& out, const ICMP6& obj) {
	out << &obj;
	return out;
}

std::ostream& operator<<(std::ostream& out, const ICMP6* obj) {
	char buf[6];

	out << "ICMP6[type: " << std::to_string(obj->getType());
	out << ", code: " << std::to_string(obj->getCode());
	sprintf(buf, "%04x", obj->getChecksum());
	out << ", checksum: " << buf;
	out << ", rest: " << std::to_string(obj->packet->end - obj->getBodyOffset()) << " bytes]";
	return out;
}

};	// namespace pv
