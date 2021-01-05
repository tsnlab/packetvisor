#pragma once

#include <pv/protocol.h>

namespace pv {

#define TCP_LEN		20

class TCP : public Protocol {
public:
	TCP(Packet* packet, uint32_t offset);
	TCP(Protocol* parent);
	virtual ~TCP();

	uint16_t getSrcport() const;
	TCP* setSrcport(uint16_t port);

	uint16_t getDstport() const;
	TCP* setDstport(uint16_t port);

	uint32_t getSeq() const;
	TCP* setSeq(uint32_t seq);

	uint32_t getAckNum() const;
	TCP* setAckNum(uint32_t ackNum);

	uint8_t getHdrLen() const;
	TCP* setHdrLen(uint8_t hdrLen);

	bool isNs() const;
	TCP* setNs(bool ns);

	bool isCwr() const;
	TCP* setCwr(bool cwr);

	bool isEce() const;
	TCP* setEce(bool ece);

	bool isUrg() const;
	TCP* setUrg(bool urg);

	bool isAck() const;
	TCP* setAck(bool ack);

	bool isPsh() const;
	TCP* setPsh(bool psh);

	bool isRst() const;
	TCP* setRst(bool rst);

	bool isSyn() const;
	TCP* setSyn(bool syn);

	bool isFin() const;
	TCP* setFin(bool fin);

	uint16_t getWindowSize() const;
	TCP* setWindowSize(uint16_t windowSize);

	uint16_t getChecksum() const;
	TCP* setChecksum(uint16_t checksum);
	TCP* checksum();

	uint16_t getUrgentPointer() const;
	TCP* setUrgentPointer(uint16_t urgentPointer);

	uint32_t getOptionOffset() const;

	uint32_t getBodyOffset() const;

	friend std::ostream& operator<<(std::ostream& out, const TCP& obj);
	friend std::ostream& operator<<(std::ostream& out, const TCP* obj);
};

}; // namespace pv
