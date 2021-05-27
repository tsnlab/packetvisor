#include <pv/checksum.h>
#include <pv/net/ipv4.h>

void pv_ipv4_checksum(struct pv_ipv4* ipv4) {
    ipv4->checksum = 0;
    ipv4->checksum = pv_checksum(ipv4, PV_IPv4_HDR_LEN(ipv4));
}
