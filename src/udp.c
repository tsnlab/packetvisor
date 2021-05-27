#include <pv/checksum.h>
#include <pv/net/ipv4.h>
#include <pv/net/udp.h>

struct pseudo_header {
    uint32_t src;
    uint32_t dst;
    uint8_t padding;
    uint8_t proto;
    uint16_t length;
} __attribute__((packed, scalar_storage_order("big-endian")));

void pv_udp_checksum_ipv4(struct pv_udp* udp, struct pv_ipv4* ipv4) {
    struct pseudo_header pseudo_header = {
        .src = ipv4->src,
        .dst = ipv4->dst,
        .padding = 0,
        .proto = PV_IP_PROTO_UDP,
        .length = udp->length,
    };

    uint32_t checksum1 = pv_checksum_partial(&pseudo_header, sizeof(pseudo_header));
    uint32_t checksum2 = pv_checksum_partial(udp, udp->length);
    return pv_checksum_finalise(checksum1 + checksum2);
}
