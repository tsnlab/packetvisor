#ifndef __PV_INTERNAL_OFFLOAD_H__
#define __PV_INTERNAL_OFFLOAD_H__

#include <pv/net/ethernet.h>
#include <pv/nic.h>

void offload_ipv4_checksum(const struct pv_nic* nic, struct pv_ethernet* const ether, struct rte_mbuf* const mbuf);

#endif
