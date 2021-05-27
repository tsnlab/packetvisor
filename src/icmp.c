#include <pv/net/icmp.h>
#include <pv/checksum.h>

void pv_icmp_checksum(struct pv_icmp * icmp, uint16_t size) {
    icmp->checksum = 0;
    icmp->checksum = pv_checksum(icmp, size);
}
