#include <pv/checksum.h>

#include <netinet/in.h>

uint16_t pv_checksum(const void* start, uint32_t size) {
    uint32_t checksum = pv_checksum_partial(start, size);
    return pv_checksum_finalize(checksum);
}

uint32_t pv_checksum_partial(const void* start, uint32_t size) {
    uint32_t checksum = 0;
    const uint16_t* buffer = start;

    while(size > 1) {
        checksum += ntohs(*buffer);
        buffer += 1;
        size -= 2;
        if(checksum >> 16 > 0) {
            checksum = (checksum >> 16) + (checksum & 0xffff);
        }
    }

    if(size > 0) {
        checksum += *(uint8_t*)buffer << 8;
        if(checksum >> 16 > 0) {
            checksum = (checksum >> 16) + (checksum & 0xffff);
        }
    }

    return checksum;
}

uint16_t pv_checksum_finalize(uint32_t checksum) {
    checksum = (checksum >> 16) + (checksum & 0xffff);
    checksum = (checksum >> 16) + (checksum & 0xffff);

    return ~checksum;
}
