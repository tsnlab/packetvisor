#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pcap.h>

struct pv_pcap* pv_pcap_create(const char* path) {
	if(mkfifo(path, 0664) == -1) {
		fprintf(stderr, "Cannot create FIFO: %s\n", path);
		return NULL;
	}

	int fd = open(path, O_WRONLY | O_NONBLOCK);
	if(fd < 0) {
		fprintf(stderr, "Cannot open FIFO: %s\n", path);
		unlink(path);
		return NULL;
	}

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
		fprintf(stderr, "Cannot write FIFO header: %s\n", path);
		close(fd);
		unlink(path);
		return NULL;
	}

	struct pv_pcap* pcap = malloc(sizeof(struct pv_pcap));
	if(pcap == NULL) {
		fprintf(stderr, "Cannot allocate memory: %s\n", path);
		close(fd);
		unlink(path);
		return NULL;
	}

	pcap->fd = fd;
	strncpy(pcap->path, path, 256);

	return pcap;
}

void pv_pcap_delete(struct pv_pcap* pcap) {
	if(pcap != NULL) {
		close(pcap->fd);
		unlink(pcap->path);
		free(pcap);
	}
}

int32_t pv_pcap_received(struct pv_pcap* pcap, uint8_t* payload, uint32_t len) {
	if(pcap == NULL)
		return -1;

	struct timeval tv;
	gettimeofday(&tv,NULL);

	struct pv_pcap_rec rec = {
		.ts_sec = tv.tv_sec,
		.ts_usec = tv.tv_usec,
		.incl_len = len,
		.orig_len = len
	};

	int32_t len2 = write(pcap->fd, &rec, sizeof(struct pv_pcap_rec));
	if(len2 > 0)
		len2 += write(pcap->fd, payload, len);

	return len2;
}

int32_t pv_pcap_send(struct pv_pcap* pcap, uint8_t* payload, uint32_t len) {
	return pv_pcap_received(pcap, payload, len);
}
