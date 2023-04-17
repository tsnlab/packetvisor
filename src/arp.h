#pragma once

#include <net/ethernet.h>
#include "pv.h"

#define ARP_RSPN_PACKET_SIZE   42

void gen_arp_response_packet(struct pv_packet* packet, const struct ether_addr smac);
void gen_fixed_packet(struct pv_packet* packet);
