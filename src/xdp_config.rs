// SPDX-License-Identifier: GPLv3+

use crate::{
    AfXdpRxConfig
};

pub const L2_FLAGS_ETH: u32 = 1 << 0;
pub const L2_FLAGS_VLAN: u32 = 1 << 1;
pub const L2_FLAGS_ARP: u32 = 1 << 2;
pub const L2_FLAGS_OTHER: u32 = 1 << 3;

pub const L3_FLAGS_IPV4: u32 = 1 << 0;
pub const L3_FLAGS_IPV6: u32 = 1 << 1;
pub const L3_FLAGS_EAPOL: u32 = 1 << 2;
pub const L3_FLAGS_OTHER: u32 = 1 << 3;

pub const L4_FLAGS_TCP: u32 = 1 << 0;
pub const L4_FLAGS_UDP: u32 = 1 << 1;
pub const L4_FLAGS_ICMP: u32 = 1 << 2;
pub const L4_FLAGS_ICMPV6: u32 = 1 << 3;
pub const L4_FLAGS_OTHER: u32 = 1 << 4;

impl AfXdpRxConfig {
    pub fn user_all() -> Self {
        Self::all_filter()
    }

    pub fn kernel_only() -> Self {
        Self {
            l2_flags: 0,
            l3_flags: 0,
            l4_flags: 0,
        }
    }

    pub fn all_filter() -> Self {
        Self {
            l2_flags: L2_FLAGS_ETH | L2_FLAGS_VLAN | L2_FLAGS_ARP,
            l3_flags: L3_FLAGS_IPV4 | L3_FLAGS_IPV6 | L3_FLAGS_EAPOL | L3_FLAGS_OTHER,
            l4_flags: 0,
        }
    }

    pub fn arp_filter() -> Self {
        Self {
            l2_flags: L2_FLAGS_ETH | L2_FLAGS_VLAN | L2_FLAGS_ARP,
            l3_flags: 0,
            l4_flags: 0,
        }
    }

    pub fn vlan_filter() -> Self {
        Self {
            l2_flags: L2_FLAGS_VLAN | L2_FLAGS_ARP,
            l3_flags: 0,
            l4_flags: 0,
        }
    }

    pub fn l3_other_filter() -> Self {
        Self {
            l2_flags: L2_FLAGS_ETH | L2_FLAGS_VLAN,
            l3_flags: L3_FLAGS_OTHER,
            l4_flags: 0,
        }
    }

    pub fn tcp_udp_filter() -> Self {
        Self {
            l2_flags: L2_FLAGS_ETH | L2_FLAGS_VLAN,
            l3_flags: L3_FLAGS_IPV4 | L3_FLAGS_IPV6,
            l4_flags: L4_FLAGS_TCP | L4_FLAGS_UDP,
        }
    }
}
