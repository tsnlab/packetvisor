#include <stdio.h>

#include "util.h"

void ip_to_str(uint32_t ip, char* buf) {
    sprintf(buf, "%d.%d.%d.%d", ip >> (8 * 3) & 0xff, ip >> (8 * 2) & 0xff, ip >> (8 * 1) & 0xff, ip >> (8 * 0) & 0xff);
}
