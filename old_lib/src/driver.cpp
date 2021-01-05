#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <iostream>
#include <pv/packet.h>
#include <pv/driver.h>
#include "container.h"

namespace pv {

Callback::Callback() {
}

Callback::~Callback() {
}

bool Callback::received(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end) {
	return false;
}

int32_t Callback::load(const char* path, int argc, char** argv) {
	return -1;
}

bool Callback::unload(int32_t id) {
	return false;
}

Driver::Driver() {
	mac = 0;
}

Driver::~Driver() {
}

uint64_t Driver::getMAC() {
	return getMAC(0);
}

uint64_t Driver::getMAC(uint32_t id) {
	return 0;
}

uint32_t Driver::getPayloadSize() {
	return 0;
}

uint8_t* Driver::alloc() {
	return nullptr;
}

void Driver::free(uint8_t* payload) {
}

bool Driver::send(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end) {
	return false;
}

Container::Container(Driver* driver) {
	this->driver = driver;
	packetlet_count = 0;
	bzero(packetlets, sizeof(Packetlet*) * MAX_PACKETLETS);
}

Container::~Container() {
	for(uint32_t i = 0; i < packetlet_count; i++) {
		if(packetlets[i] != nullptr)
			delete packetlets[i];
	}
}

bool Container::received(uint32_t queueId, uint8_t* payload, uint32_t start, uint32_t end) {
	Packet* packet = new Packet(queueId, payload, start, end, driver->getPayloadSize());

	for(uint32_t i = 0; i < packetlet_count; i++) {
		try {
			if(packetlets[i] != nullptr && packetlets[i]->received(packet))
				return true;
		} catch(const std::exception& e) {
			fprintf(stderr, "Exception occurred: Packetlet[%u] - %s\n", i, e.what());
		}
	}

	packet->payload = nullptr;
	delete packet;

	return false;
}

int32_t Container::load(const char* path, int argc, char** argv) {
	void* handle = dlopen(path, RTLD_LAZY);
    if(!handle) {
        fprintf(stderr, "%s\n", dlerror());
        return -1;
    }

    dlerror();
    char* error;
    pv::packetlet pv_packetlet = (pv::packetlet)dlsym(handle, "pv_packetlet");
    if((error = dlerror()) != NULL) {
        fprintf(stderr, "%s\n", error);
		dlclose(handle);
        return -2;
    }

	Packetlet* packetlet = pv_packetlet(argc, argv);
	if(packetlet == nullptr) {
		fprintf(stderr, "Cannot create packetlet\n");
		dlclose(handle);
		return -3;
	}

	int32_t id = addPacketlet(packetlet);
	if(id < 0) {
		fprintf(stderr, "Cannot register packetlet to the container: errno: %d\n", id);
		delete packetlet;
		dlclose(handle);
		return -4;
	}

	packetlet->setId(id);
	packetlet->setHandle(handle);
	packetlet->init(driver);

	return id;
}

bool Container::unload(int32_t id) {
	Packetlet* packetlet = packetlets[id];
	if(removePacketlet(id)) {
		dlclose(packetlet->getHandle());
		delete packetlet;
		return true;
	} else {
		return false;
	}
}

int32_t Container::addPacketlet(Packetlet* packetlet) {
	if(packetlet_count < MAX_PACKETLETS) {
		packetlets[packetlet_count++] = (Packetlet*)packetlet;

		return packetlet_count - 1;
	}

	return -1;
}

bool Container::removePacketlet(int32_t id) {
	if(id < 0 || id >= (int32_t)packetlet_count)
		return false;
	
	if(packetlets[id] != nullptr) {
		packetlets[id] = nullptr;
		return true;
	} else {
		return false;
	}
}

extern "C" {

Callback* pv_init(Driver* driver) {
	return new Container(driver);
}

void pv_destroy(Callback* callback) {
	delete callback;
}

} // extern "C"

}; // namespace pv
