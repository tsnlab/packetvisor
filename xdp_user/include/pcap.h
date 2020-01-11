#ifndef __PV_PCAP_H__
#define __PV_PCAP_H__

#include <stdint.h>

// Ref: https://wiki.wireshark.org/Development/LibpcapFileFormat

struct pv_pcap_header {
	uint32_t	magic_number;	/* magic number */
	uint16_t	version_major;	/* major version number */
	uint16_t	version_minor;	/* minor version number */
	int32_t		thiszone;		/* GMT to local correction */
	uint32_t	sigfigs;		/* accuracy of timestamps */
	uint32_t	snaplen;		/* max length of captured packets, in octets */
	uint32_t	network;		/* data link type */
};

struct pv_pcap_rec {
	uint32_t	ts_sec;			/* timestamp seconds */
	uint32_t	ts_usec;		/* timestamp microseconds */
	uint32_t	incl_len;		/* number of octets of packet saved in file */
	uint32_t	orig_len;		/* actual length of packet */
};

struct pv_pcap {
	int			fd;
	char		path[256];
};

struct pv_pcap* pv_pcap_create(const char* path);
void pv_pcap_delete(struct pv_pcap* pcap);
int32_t pv_pcap_received(struct pv_pcap* pcap, uint8_t* payload, uint32_t len);
int32_t pv_pcap_send(struct pv_pcap* pcap, uint8_t* payload, uint32_t len);

#endif /* __PV_PCAP_H__ */
