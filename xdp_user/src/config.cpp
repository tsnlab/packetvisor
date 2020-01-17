#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <linux/if_link.h>
#include <linux/if_xdp.h>
#include "config.h"

namespace pv {

static struct option opts[] = {
	{ "help", no_argument, nullptr, 'h' },
	{ "dev", required_argument, nullptr, 'd' },
	{ "mac", required_argument, nullptr, 'm' },
	{ "pcap", required_argument, nullptr, 'P' },
	{ "auto-mode", no_argument, nullptr, 'a' },
	{ "skb-mode", no_argument, nullptr, 's' },
	{ "native-mode", no_argument, nullptr, 'n' },
	{ "offload-mode", no_argument, nullptr, 'o' },
	{ "polling", no_argument, nullptr, 'p' },
	{ "copy", no_argument, nullptr, 'c' },
	{ "zero-copy", no_argument, nullptr, 'z' },
	{ "xdp-file", required_argument, nullptr, 'f' },
	{ "xdp-section", required_argument, nullptr, 'S' },
	{ "queue", required_argument, nullptr, 'q' },
	{ nullptr, 0, nullptr, 0 }
};

struct desc {
	const char* param;
	const char* desc;
};

static struct desc descs[] = {
	{ nullptr, "Print this message" },
	{ "<ifname>", "interface name" },
	{ "<mac>", "MAC address in 00:11:22:33:44:55 format" },
	{ "<pcap>", "PCAP FIFO path to capture" },
	{ nullptr, "Auto detects SKB or native mode" },
	{ nullptr, "Run XDP in SKB(generic) mode" },
	{ nullptr, "Run XDP in native mode" },
	{ nullptr, "Run XDP in offloading mode" },
	{ nullptr, "Use poll() API to wait for packets" },
	{ nullptr, "Force copy mode" },
	{ nullptr, "Force zero copy mode" },
	{ "<file_name>", "XDP file name" },
	{ "<section_name>", "XDP section name" },
	{ "<queue_number>", "Specify receive queue, default is 0" },
	{ nullptr, nullptr }
};

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

static bool str2mac(char* str, uint64_t* mac) {
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

uint64_t Config::mac = 0;
uint32_t Config::xdp_flags = 0;
uint16_t Config::xsk_flags = 0;
int Config::xsk_if_queue = 0;
bool Config::is_xsk_polling = false;
int Config::queue = 0;
int Config::ifindex = -1;
char Config::ifname[IF_NAMESIZE] = { 0, };
char Config::xdp_file[256] = { 0, };
char Config::xdp_section[32] = { 0, };
char Config::pcap_path[256] = { 0, };

int Config::parse(int argc, char** argv) {
	xdp_flags &= ~XDP_FLAGS_UPDATE_IF_NOEXIST;	// force update

	int ch;
	while((ch = getopt_long(argc, argv, "d:m:P:asnopczf:S:", opts, NULL)) != -1) {
		switch (ch) {
			case 'd':
				// ifname
				if(strlen(optarg) >= IF_NAMESIZE) {
					fprintf(stderr, "ERROR: --dev <ifname> must be less longer than %d.\n", IF_NAMESIZE);
					return -1;
				}
				strncpy(ifname, optarg, IF_NAMESIZE);

				// ifindex
				ifindex = if_nametoindex(ifname);
				if(ifindex == 0) {
					fprintf(stderr, "ERROR: Unknown ifname: %s, errno: %d.\n", ifname, errno);
					return -1;
				}
				break;
			case 'm':
				if(!str2mac(optarg, &mac)) {
					fprintf(stderr, "ERROR: --mac <mac> must be 00:11:22:33:44:55 format.\n");
					return -1;
				}
				break;
			case 'P':
				if(strlen(optarg) > 255) {
					fprintf(stderr, "ERROR: Pcap FIFO path is too long, must less than 255 bytes.\n");
					return -1;
				}

				strncpy(pcap_path, optarg, 256);
				break;
			case 'a':
				xdp_flags &= ~XDP_FLAGS_MODES;    // clear flags
				break;
			case 's':
				xdp_flags &= ~XDP_FLAGS_MODES;    // clear flags
				xdp_flags |= XDP_FLAGS_SKB_MODE;
				xsk_flags &= ~XDP_ZEROCOPY;
				xsk_flags |= XDP_COPY;
				break;
			case 'n':
				xdp_flags &= ~XDP_FLAGS_MODES;    // clear flags
				xdp_flags |= XDP_FLAGS_DRV_MODE; 
				break;
			case 'o':
				xdp_flags &= ~XDP_FLAGS_MODES;    // clear flags
				xdp_flags |= XDP_FLAGS_HW_MODE;
				break;
			case 'p':
				is_xsk_polling = true;
				break;
			case 'c':
				xsk_flags &= ~XDP_ZEROCOPY;
				xsk_flags |= XDP_COPY;
				break;
			case 'z':
				xsk_flags &= ~XDP_COPY;
				xsk_flags |= XDP_ZEROCOPY;
				break;
			case 'f':
				if(strlen(optarg) >= 255) {
					fprintf(stderr, "ERROR: --xdp-file <file> must less than 255 bytes long.\n");
					return -1;
				}

				strncpy(xdp_file, optarg, 256);
				break;
			case 'S':
				if(strlen(optarg) >= 31) {
					fprintf(stderr, "ERROR: --xdp-section <section> must less than 31 bytes long.\n");
					return -1;
				}

				strncpy(xdp_section, optarg, 32);
				break;
			case 'q':
				queue = atoi(optarg);
				break;
			default:
				return -1;
		}
	}

	if(ifindex < 0) {
		fprintf(stderr, "ERROR: --dev option must be specified\n");
		return -1;
	}

	return optind;
}

void Config::usage() {
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

}; // namespace pv
