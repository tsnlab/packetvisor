#pragma once

#include <pv/driver.h>

namespace pv {

class Container : public Callback {
protected:
	static const uint32_t MAX_PACKETLETS = 8;

	uint32_t packetlet_count;
	Packetlet* packetlets[MAX_PACKETLETS];

	Driver* driver;

public:
	Container(Driver* driver);
	virtual ~Container();

	virtual bool received(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end, uint32_t size);

	virtual int32_t load(const char* path, int argc, char** argv);
	virtual bool unload(int32_t id);

	int32_t addPacketlet(Packetlet* packetlet);
	bool removePacketlet(int32_t id);
};

};
