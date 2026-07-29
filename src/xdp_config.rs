// SPDX-License-Identifier: GPLv3+

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AfXdpRxConfig {
    pub l2_flags: u32,
    pub l3_flags: u32,
    pub l4_flags: u32,
}

impl AfXdpRxConfig {
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
            l2_flags: Self::L2_FLAGS_ETH | Self::L2_FLAGS_VLAN | Self::L2_FLAGS_ARP,
            l3_flags: Self::L3_FLAGS_IPV4
                | Self::L3_FLAGS_IPV6
                | Self::L3_FLAGS_EAPOL
                | Self::L3_FLAGS_OTHER,
            l4_flags: 0,
        }
    }

    pub fn arp_filter() -> Self {
        Self {
            l2_flags: Self::L2_FLAGS_ETH | Self::L2_FLAGS_VLAN | Self::L2_FLAGS_ARP,
            l3_flags: 0,
            l4_flags: 0,
        }
    }

    pub fn vlan_filter() -> Self {
        Self {
            l2_flags: Self::L2_FLAGS_VLAN | Self::L2_FLAGS_ARP,
            l3_flags: 0,
            l4_flags: 0,
        }
    }

    pub fn l3_other_filter() -> Self {
        Self {
            l2_flags: Self::L2_FLAGS_ETH | Self::L2_FLAGS_VLAN,
            l3_flags: Self::L3_FLAGS_OTHER,
            l4_flags: 0,
        }
    }

    pub fn tcp_udp_filter() -> Self {
        Self {
            l2_flags: Self::L2_FLAGS_ETH | Self::L2_FLAGS_VLAN,
            l3_flags: Self::L3_FLAGS_IPV4 | Self::L3_FLAGS_IPV6,
            l4_flags: Self::L4_FLAGS_TCP | Self::L4_FLAGS_UDP,
        }
    }
}
