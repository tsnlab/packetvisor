#include <pv/ipv4.h>
#include <pv/tcp.h>

namespace pv {

struct TCP_Pseudo {
	uint32_t	source;			///< Source address (endian32)
	uint32_t	destination;	///< Destination address (endian32)
	uint8_t		padding;		///< Zero padding
	uint8_t		protocol;		///< TCP protocol number, 0x06
	uint16_t	length;			///< Header and data length in bytes (endian16)
} __attribute__((packed));

TCP::TCP(Packet* packet, uint32_t offset) : Protocol(packet, offset) {
}

TCP::TCP(Protocol* parent) : Protocol(parent) {
}

TCP::~TCP() {
}

uint16_t TCP::getSrcport() const {
	CHECK(0, 2);
	return endian16(*(uint16_t*)OFFSET(0));
}

TCP* TCP::setSrcport(uint16_t port) {
	CHECK(0, 2);
	*(uint16_t*)OFFSET(0) = endian16(port);
	return this;
}

uint16_t TCP::getDstport() const {
	CHECK(2, 2);
	return endian16(*(uint16_t*)OFFSET(2));
}

TCP* TCP::setDstport(uint16_t port) {
	CHECK(2, 2);
	*(uint16_t*)OFFSET(2) = endian16(port);
	return this;
}

uint32_t TCP::getSeq() const {
	CHECK(4, 4);
	return endian32(*(uint32_t*)OFFSET(4));
}

TCP* TCP::setSeq(uint32_t seq) {
	CHECK(4, 4);
	*(uint32_t*)OFFSET(4) = endian32(seq);
	return this;
}

uint32_t TCP::getAckNum() const {
	CHECK(8, 4);
	return endian32(*(uint32_t*)OFFSET(4));
}

TCP* TCP::setAckNum(uint32_t ackNum) {
	CHECK(8, 4);
	*(uint32_t*)OFFSET(8) = endian32(ackNum);
	return this;
}

uint8_t TCP::getHdrLen() const {
	CHECK(12, 1);
	return *OFFSET(12) >> 4;
}

TCP* TCP::setHdrLen(uint8_t hdrLen) {
	CHECK(12, 1);
	*OFFSET(12) = ((hdrLen & 0x0f) << 4) | (*OFFSET(12) & 0x0f);
	return this;
}

bool TCP::isNs() const {
	CHECK(12, 1);
	return !!(*OFFSET(12) & 0x01);
}

TCP* TCP::setNs(bool ns) {
	CHECK(12, 1);
	*OFFSET(12) = (*OFFSET(12) & 0xfe) | (ns ? 0x01 : 0);
	return this;
}

bool TCP::isCwr() const {
	CHECK(13, 1);
	return !!(*OFFSET(13) & 0x80);
}

TCP* TCP::setCwr(bool cwr) {
	CHECK(13, 1);
	*OFFSET(13) = (*OFFSET(13) & 0x7f) | (cwr ? 0x80 : 0);
	return this;
}

bool TCP::isEce() const {
	CHECK(13, 1);
	return !!(*OFFSET(13) & 0x40);
}

TCP* TCP::setEce(bool ece) {
	CHECK(13, 1);
	*OFFSET(13) = (*OFFSET(13) & 0xbf) | (ece ? 0x40 : 0);
	return this;
}

bool TCP::isUrg() const {
	CHECK(13, 1);
	return !!(*OFFSET(13) & 0x20);
}

TCP* TCP::setUrg(bool urg) {
	CHECK(13, 1);
	*OFFSET(13) = (*OFFSET(13) & 0xdf) | (urg ? 0x20 : 0);
	return this;
}

bool TCP::isAck() const {
	CHECK(13, 1);
	return !!(*OFFSET(13) & 0x10);
}

TCP* TCP::setAck(bool ack) {
	CHECK(13, 1);
	*OFFSET(13) = (*OFFSET(13) & 0xef) | (ack ? 0x10 : 0);
	return this;
}

bool TCP::isPsh() const {
	CHECK(13, 1);
	return !!(*OFFSET(13) & 0x08);
}

TCP* TCP::setPsh(bool psh) {
	CHECK(13, 1);
	*OFFSET(13) = (*OFFSET(13) & 0xf7) | (psh ? 0x08 : 0);
	return this;
}

bool TCP::isRst() const {
	CHECK(13, 1);
	return !!(*OFFSET(13) & 0x04);
}

TCP* TCP::setRst(bool rst) {
	CHECK(13, 1);
	*OFFSET(13) = (*OFFSET(13) & 0xfb) | (rst ? 0x04 : 0);
	return this;
}

bool TCP::isSyn() const {
	CHECK(13, 1);
	return !!(*OFFSET(13) & 0x02);
}

TCP* TCP::setSyn(bool fin) {
	CHECK(13, 1);
	*OFFSET(13) = (*OFFSET(13) & 0xfd) | (fin ? 0x02 : 0);
	return this;
}

bool TCP::isFin() const {
	CHECK(13, 1);
	return !!(*OFFSET(13) & 0x01);
}

TCP* TCP::setFin(bool fin) {
	CHECK(13, 1);
	*OFFSET(13) = (*OFFSET(13) & 0xfe) | (fin ? 0x01 : 0);
	return this;
}

uint16_t TCP::getWindowSize() const {
	CHECK(14, 2);
	return endian16(*(uint16_t*)OFFSET(14));
}

TCP* TCP::setWindowSize(uint16_t windowSize) {
	CHECK(14, 2);
	*(uint16_t*)OFFSET(14) = endian16(windowSize);
	return this;
}

uint16_t TCP::getChecksum() const {
	CHECK(16, 2);
	return endian16(*(uint16_t*)OFFSET(16));
}

TCP* TCP::setChecksum(uint16_t checksum) {
	CHECK(16, 2);
	*(uint16_t*)OFFSET(16) = endian16(checksum);
	return this;
}

TCP* TCP::checksum() {
	CHECK(16, 2);
	*(uint16_t*)OFFSET(16) = 0;

	IPv4* ip = (IPv4*)parent;

	uint16_t len = getHdrLen() * 4 + (getEnd() - getBodyOffset());
	TCP_Pseudo pseudo;
	pseudo.source = endian32(ip->getSrc());
	pseudo.destination = endian32(ip->getDst());
	pseudo.padding = 0;
	pseudo.protocol = ip->getProto();
	pseudo.length = endian16(len);
	
	uint32_t sum = (uint16_t)~Protocol::checksum((uint8_t*)&pseudo, 0, sizeof(pseudo)) + (uint16_t)~Protocol::checksum(0, len);
	while(sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);
	*(uint16_t*)OFFSET(16) = endian16(~sum);

	return this;
}

uint16_t TCP::getUrgentPointer() const {
	CHECK(18, 2);
	return endian16(*(uint16_t*)OFFSET(18));
}

TCP* TCP::setUrgentPointer(uint16_t urgentPointer) {
	CHECK(18, 2);
	*(uint16_t*)OFFSET(18) = endian16(urgentPointer);
	return this;
}

uint32_t TCP::getOptionOffset() const {
	return packet->start + offset + TCP_LEN;
}

uint32_t TCP::getBodyOffset() const {
	return packet->start + offset + getHdrLen() * 4;
}

std::ostream& operator<<(std::ostream& out, const TCP& obj) {
	out << &obj;
	return out;
}

std::ostream& operator<<(std::ostream& out, const TCP* obj) {
	out << "TCP[srcport: " << std::to_string(obj->getSrcport());
	out << ", dstport: " << std::to_string(obj->getDstport());
	out << ", seq: " << std::to_string(obj->getSeq());
	out << ", ackNum: " << std::to_string(obj->getAckNum());
	out << ", hdrLen: " << std::to_string(obj->getHdrLen());
	out << ", ns: " << (obj->isNs() ? "true" : "false");
	out << ", cwr: " << (obj->isCwr() ? "true" : "false");
	out << ", ece: " << (obj->isEce() ? "true" : "false");
	out << ", urg: " << (obj->isUrg() ? "true" : "false");
	out << ", ack: " << (obj->isAck() ? "true" : "false");
	out << ", psh: " << (obj->isPsh() ? "true" : "false");
	out << ", rst: " << (obj->isRst() ? "true" : "false");
	out << ", syn: " << (obj->isSyn() ? "true" : "false");
	out << ", fin: " << (obj->isFin() ? "true" : "false");
	out << ", windowSize: " << std::to_string(obj->getWindowSize());
	char buf[6];
	sprintf(buf, "%04x", obj->getChecksum());
	out << ", checksum: " << buf;
	out << ", urgentPointer: " << std::to_string(obj->getUrgentPointer());
	out << ", body: " << std::to_string(obj->packet->end - obj->getBodyOffset()) << " bytes]";

	return out;
}

}; // namespace pv
