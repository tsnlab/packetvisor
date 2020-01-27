#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include <linux/if_link.h>
#include <linux/if_xdp.h>
#include "config.h"
#undef min
#undef max
#include <iostream>
#include <pugixml.hpp>

namespace pv {

static struct option opts[] = {
	{ "help", no_argument, nullptr, 'h' },
	{ "config", required_argument, nullptr, 'c' },
	{ nullptr, 0, nullptr, 0 }
};

struct desc {
	const char* param;
	const char* desc;
};

static struct desc descs[] = {
	{ nullptr, "Print this message" },
	{ "<config>", "specify config.xml file location, default is ./config.xml" },
	{ nullptr, nullptr }
};

UmemConfig::UmemConfig() {
	num_frames = 4096;
}

UmemConfig::~UmemConfig() {
}

XDPConfig::XDPConfig(const std::string& dev) {
	time_t t;
	srand((unsigned) time(&t));
	mac = (((uint64_t)0x2) << 40) | (((uint64_t)random() & 0xff) << 32) | ((uint64_t)random() & 0xffffffff);

	xdp_flags = 0;
	xdp_flags &= ~XDP_FLAGS_UPDATE_IF_NOEXIST;	// force update

	xsk_flags = 0;
	xsk_if_queue = 0;
	is_xsk_polling = false;
	queue = 0;
	ifindex = -1;
	ifname = "veth";
	xdp_file = "pv.o";
	xdp_section = "xdp_sock";
	pcap_path = "";

	if(dev != "")
		ifname = dev;
}

XDPConfig::~XDPConfig() {
	for(PacketletInfo* info: packetlets)
		delete info;
}

static uint64_t hex(char ch) {
	if(ch >= '0' && ch <= '9')
		return ch - '0';
	else if(ch >= 'a' && ch <= 'f')
		return ch - 'a' + 10;
	else if(ch >= 'A' && ch <= 'F')
		return ch - 'A' + 10;
	else
		return 0;
}

static bool str2mac(const char* str, uint64_t* mac) {
	if(strlen(str) != 17)
		return false;

	*mac = 0;
	*mac |= hex(str[0]) << 44;
	*mac |= hex(str[1]) << 40;
	*mac |= hex(str[3]) << 36;
	*mac |= hex(str[4]) << 32;
	*mac |= hex(str[6]) << 28;
	*mac |= hex(str[7]) << 24;
	*mac |= hex(str[9]) << 20;
	*mac |= hex(str[10]) << 16;
	*mac |= hex(str[12]) << 12;
	*mac |= hex(str[13]) << 8;
	*mac |= hex(str[15]) << 4;
	*mac |= hex(str[16]) << 0;

	return true;
};

UmemConfig* Config::umem = nullptr;
std::map<std::string, XDPConfig*> Config::xdp;

static bool parse_umem(const pugi::xml_node& node, UmemConfig* config) {
	config->num_frames = 4096;

	if(node.child("frames").text())
		config->num_frames = node.child("frames").text().as_uint();

	return true;
}

static bool parse_xdp(const pugi::xml_node& node, XDPConfig* config) {
	if(config->ifname.length() >= IF_NAMESIZE) {
		fprintf(stderr, "ERROR: --dev <ifname> must be less longer than %d.\n", IF_NAMESIZE);
		return false;
	}

	config->ifindex = if_nametoindex(config->ifname.c_str());
	if(config->ifindex == 0) {
		fprintf(stderr, "ERROR: Unknown ifname: %s, errno: %d.\n", config->ifname.c_str(), errno);
		return false;
	}

	if(node.child("mac")) {
		std::string str = node.child("mac").text().as_string();
		if(!str2mac(str.c_str(), &config->mac)) {
			fprintf(stderr, "ERROR: --mac <mac> must be 00:11:22:33:44:55 format.\n");
			return -1;
		}
	}

	if(node.child("pcap")) {
		std::string str = node.child("pcap").text().as_string();
		if(str.length() > 255) {
			fprintf(stderr, "ERROR: Pcap FIFO path is too long, must less than 255 bytes.\n");
			return -1;
		}

		config->pcap_path = str;
	}

	if(node.child("mode")) {
		std::string str = node.child("mode").text().as_string();
		if(str == "skb") {
			config->xdp_flags &= ~XDP_FLAGS_MODES;    // clear flags
			config->xdp_flags |= XDP_FLAGS_SKB_MODE;
			config->xsk_flags &= ~XDP_ZEROCOPY;
			config->xsk_flags |= XDP_COPY;
		} else if(str == "native") {
			config->xdp_flags &= ~XDP_FLAGS_MODES;    // clear flags
			config->xdp_flags |= XDP_FLAGS_DRV_MODE; 
		} else if(str == "offload") {
			config->xdp_flags &= ~XDP_FLAGS_MODES;    // clear flags
			config->xdp_flags |= XDP_FLAGS_HW_MODE;
		} else {
			config->xdp_flags &= ~XDP_FLAGS_MODES;    // clear flags
		}
	}

	if(node.child("polling")) {
		std::string str = node.child("polling").text().as_string();
		config->is_xsk_polling = str == "true";
	}

	if(node.child("copy")) {
		std::string str = node.child("copy").text().as_string();
		if(str == "copy") {
			config->xsk_flags &= ~XDP_ZEROCOPY;
			config->xsk_flags |= XDP_COPY;
		} else if(str == "zero") {
			config->xsk_flags &= ~XDP_COPY;
			config->xsk_flags |= XDP_ZEROCOPY;
		}
	}

	if(node.child("program")) {
		std::string str = node.child("program").text().as_string();
		if(str.length() > 255) {
			fprintf(stderr, "ERROR: --xdp-file <file> must less than 255 bytes long.\n");
			return -1;
		}

		config->xdp_file = str;
	}

	if(node.child("section")) {
		std::string str = node.child("section").text().as_string();
		if(str.length() > 31) {
			fprintf(stderr, "ERROR: --xdp-section <section> must less than 31 bytes long.\n");
			return -1;
		}

		config->xdp_section = str;
	}

	if(node.child("queue")) {
		config->queue = node.child("queue").text().as_int();
	}

	for(pugi::xml_node p = node.child("packetlet"); p; p = p.next_sibling("packetlet")) {
		PacketletInfo* info = new PacketletInfo();
		info->path = p.child("path").text().as_string();
		for(pugi::xml_node a = p.child("arg"); a; a = a.next_sibling("arg")) {
			info->args.push_back(a.text().as_string());
		}

		config->packetlets.push_back(info);
	}

	return true;
}

static void usage() {
	char buf[32];

	printf("Usage: pv\n");
	for(int i = 0; opts[i].name != nullptr; i++) {
		if(descs[i].param != nullptr)
			snprintf(buf, 32, "%s %s", opts[i].name, descs[i].param);
		else
			snprintf(buf, 32, "%s", opts[i].name);

		printf("\t-%c,--%-30s%s\n", opts[i].val, buf, descs[i].desc);
	}
}

int Config::parse(int argc, char** argv) {
	char path[256];
	sprintf(path, "config.xml");

	int ch;
	while((ch = getopt_long(argc, argv, "c:", opts, NULL)) != -1) {
		switch (ch) {
			case 'c':
				snprintf(path, 256, "%s", optarg);
				break;
			default:
				usage();
				return 0;
		}
	}

	printf("Using config file: %s\n", path);

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(path);
	if(result) {
		Config::umem = new UmemConfig();
		if(!parse_umem(doc.child("packetvisor").child("umem"), Config::umem))
			return -1;

		for(pugi::xml_node node = doc.child("packetvisor").child("xdp"); node; node = node.next_sibling("xdp")) {
			std::string dev = node.attribute("dev").as_string();
			XDPConfig* config = new XDPConfig(dev);
			Config::xdp[dev] = config;
			if(!parse_xdp(node, config))
				return -2;
		}
	} else {
		return -3;
	}

	return optind;
}

void Config::destroy() {
	for(auto pair: Config::xdp) {
		delete pair.second;
	}

	delete umem;
}

}; // namespace pv
