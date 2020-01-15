#include <string>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pcap.h>

namespace pv {

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

Pcap::Pcap(const char* path) {
	if(mkfifo(path, 0644) != 0) {
		throw "Cannot create FIFO: " + std::string(path);
	}

	int fd = open(path, O_RDWR);
	if(fd < 0) {
		unlink(path);
		throw "Cannot open FIFO: " + std::string(path);
	}

	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	struct pv_pcap_header header = {
		.magic_number = 0xa1b2c3d4,
		.version_major = 2,
		.version_minor = 4,
		.thiszone = 0,
		.sigfigs = 0,
		.snaplen = 65536,
		.network = 1
	};

	ssize_t len = write(fd, &header, sizeof(struct pv_pcap_header));
	if(len != sizeof(struct pv_pcap_header)) {
		close(fd);
		unlink(path);

		throw "Cannot write FIFO header: " + std::string(path);
	}

	this->fd = fd;
	strncpy(this->path, path, 256);
	this->start = 0;
	this->end = 0;
}

Pcap::~Pcap() {
	close(fd);
	unlink(path);
}

int32_t Pcap::received(uint8_t* payload, uint32_t len) {
	if(start < end) {
		int32_t len2 = write(fd, buf + start, end - start);

		if(len2 < 0) {
			return len2;
		} else {
			start += len2;

			if(start < end)
				return -2;
		}
	}

	struct timeval tv;
	gettimeofday(&tv,NULL);

	struct pv_pcap_rec rec = {
		.ts_sec = (uint32_t)tv.tv_sec,
		.ts_usec = (uint32_t)tv.tv_usec,
		.incl_len = len,
		.orig_len = len
	};

	int32_t len2 = write(fd, &rec, sizeof(struct pv_pcap_rec));
	if(len2 < 0) {
		return len2;
	} else if((size_t)len2 < sizeof(struct pv_pcap_rec)) {
		start = 0;
		end = sizeof(struct pv_pcap_rec) - len2;
		memcpy(buf, (uint8_t*)&rec + len2, end);
		memcpy(buf + end, payload, len);
		end += len;

		return sizeof(struct pv_pcap_rec) + len;
	}

	len2 = write(fd, payload, len);
	if(len2 < 0) {
		start = 0;
		end = len;
		memcpy(buf, payload, end);

		return sizeof(struct pv_pcap_rec) + len;
	} else if(len2 < (int32_t)len) {
		start = 0;
		end = len - len2;
		memcpy(buf, payload + len2, end);

		return sizeof(struct pv_pcap_rec) + len;
	}

	return sizeof(struct pv_pcap_rec) + len;
}

int32_t Pcap::send(uint8_t* payload, uint32_t len) {
	return received(payload, len);
}

}; // namespace pv
