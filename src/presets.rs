use crate::{
    AfXdpRxConfig, L2_FLAGS_ARP, L2_FLAGS_ETH, L2_FLAGS_VLAN, L3_FLAGS_EAPOL, L3_FLAGS_IPV4,
    L3_FLAGS_IPV6, L3_FLAGS_OTHER, L4_FLAGS_TCP, L4_FLAGS_UDP,
};

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

    pub fn eapol_filter() -> Self {
        Self {
            l2_flags: L2_FLAGS_ETH | L2_FLAGS_VLAN,
            l3_flags: L3_FLAGS_EAPOL,
            l4_flags: 0,
        }
    }

    pub fn tcp_udp_userspace_filter() -> Self {
        Self {
            l2_flags: L2_FLAGS_ETH | L2_FLAGS_VLAN,
            l3_flags: L3_FLAGS_IPV4 | L3_FLAGS_IPV6,
            l4_flags: L4_FLAGS_TCP | L4_FLAGS_UDP,
        }
    }

    pub fn eapol_kernel_filter() -> Self {
        Self {
            l2_flags: L2_FLAGS_ETH | L2_FLAGS_VLAN | L2_FLAGS_ARP,
            l3_flags: L3_FLAGS_IPV4 | L3_FLAGS_IPV6 | L3_FLAGS_OTHER,
            l4_flags: 0,
        }
    }
}
