#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IP protocols.
 */
enum pv_ip_protocol {
    PV_IP_PROTO_HOPOPT = 0,            ///< IPv6 Hop-by-Hop Option
    PV_IP_PROTO_ICMP = 1,              ///< Internet Control Message
    PV_IP_PROTO_IGMP = 2,              ///< Internet Group Management
    PV_IP_PROTO_GGP = 3,               ///< Gateway-to-Gateway
    PV_IP_PROTO_IPv4 = 4,              ///< IPv4 encapsulation
    PV_IP_PROTO_ST = 5,                ///< Stream
    PV_IP_PROTO_TCP = 6,               ///< Transmission Control
    PV_IP_PROTO_CBT = 7,               ///< CBT
    PV_IP_PROTO_EGP = 8,               ///< Exterior Gateway Protocol
    PV_IP_PROTO_IGP = 9,               ///< any private interior gateway(used by Cisco for their IGRP)
    PV_IP_PROTO_BBN_RCC_MON = 10,      ///< BBN RCC Monitoring
    PV_IP_PROTO_NVP_II = 11,           ///< Network Voice Protocol
    PV_IP_PROTO_PUP = 12,              ///< PUP
    PV_IP_PROTO_EMCON = 14,            ///< EMCON
    PV_IP_PROTO_XNET = 15,             ///< Cross Net Debugger
    PV_IP_PROTO_CHAOS = 16,            ///< Chaos
    PV_IP_PROTO_UDP = 17,              ///< User Datagram
    PV_IP_PROTO_MUX = 18,              ///< Multiplexing
    PV_IP_PROTO_DCN_MEAS = 19,         ///< DCN Measurement Subsystems
    PV_IP_PROTO_HMP = 20,              ///< Host Monitoring
    PV_IP_PROTO_PRM = 21,              ///< Packet Radio Measurement
    PV_IP_PROTO_XNS_IDP = 22,          ///< XEROX NS IDP
    PV_IP_PROTO_TRUNK_1 = 23,          ///< Trunk-1
    PV_IP_PROTO_TRUNK_2 = 24,          ///< Trunk-2
    PV_IP_PROTO_LEAF_1 = 25,           ///< Leaf-1
    PV_IP_PROTO_LEAF_2 = 26,           ///< Leaf-2
    PV_IP_PROTO_RDP = 27,              ///< Reliable Data Protocol
    PV_IP_PROTO_IRTP = 28,             ///< Internet Reliable Transaction
    PV_IP_PROTO_ISO_TP4 = 29,          ///< ISO Transport Protocol Class 4
    PV_IP_PROTO_NETBLT = 30,           ///< Bulk Data Transfer Protocol
    PV_IP_PROTO_MFE_NSP = 31,          ///< MFE Network Services Protocol
    PV_IP_PROTO_MERIT_INP = 32,        ///< MERIT Internodal Protocol
    PV_IP_PROTO_DCCP = 33,             ///< Datagram Congestion Control Protocol
    PV_IP_PROTO_3PC = 34,              ///< Third Party Connect Protocol
    PV_IP_PROTO_IDPR = 35,             ///< Inter-Domain Policy Routing Protocol
    PV_IP_PROTO_XTP = 36,              ///< XTP
    PV_IP_PROTO_DDP = 37,              ///< Datagram Delivery Protocol
    PV_IP_PROTO_IDPR_CMTP = 38,        ///< IDPR Control Message Transport Proto
    PV_IP_PROTO_TPpp = 39,             ///< TP++ Transport Protocol
    PV_IP_PROTO_IL = 40,               ///< IL Transport Protocol
    PV_IP_PROTO_IPv6 = 41,             ///< IPv6 encapsulation
    PV_IP_PROTO_SDRP = 42,             ///< Source Demand Routing Protocol
    PV_IP_PROTO_IPv6_Route = 43,       ///< Routing Header for IPv6
    PV_IP_PROTO_IPv6_Frag = 44,        ///< Fragment Header for IPv6
    PV_IP_PROTO_IDRP = 45,             ///< Inter-Domain Routing Protocol
    PV_IP_PROTO_RSVP = 46,             ///< Reservation Protocol
    PV_IP_PROTO_GRE = 47,              ///< Generic Routing Encapsulation
    PV_IP_PROTO_DSR = 48,              ///< Dynamic Source Routing Protocol
    PV_IP_PROTO_BNA = 49,              ///< BNA
    PV_IP_PROTO_ESP = 50,              ///< Encap Security Payload
    PV_IP_PROTO_AH = 51,               ///< Authentication Header
    PV_IP_PROTO_I_NLSP = 52,           ///< Integrated Net Layer Security  TUBA
    PV_IP_PROTO_NARP = 54,             ///< NBMA Address Resolution Protocol
    PV_IP_PROTO_MOBILE = 55,           ///< IP Mobility
    PV_IP_PROTO_TLSP = 56,             ///< Transport Layer Security Protocol using Kryptonet key management
    PV_IP_PROTO_SKIP = 57,             ///< SKIP
    PV_IP_PROTO_IPv6_ICMP = 58,        ///< ICMP for IPv6
    PV_IP_PROTO_IPv6_NoNxt = 59,       ///< No Next Header for IPv6
    PV_IP_PROTO_IPv6_Opts = 60,        ///< Destination Options for IPv6
    PV_IP_PROTO_CFTP = 62,             ///< CFTP
    PV_IP_PROTO_SAT_EXPAK = 64,        ///< SATNET and Backroom EXPAK
    PV_IP_PROTO_KRYPTOLAN = 65,        ///< Kryptolan
    PV_IP_PROTO_RVD = 66,              ///< MIT Remote Virtual Disk Protocol
    PV_IP_PROTO_IPPC = 67,             ///< Internet Pluribus Packet Core
    PV_IP_PROTO_SAT_MON = 69,          ///< SATNET Monitoring
    PV_IP_PROTO_VISA = 70,             ///< VISA Protocol
    PV_IP_PROTO_IPCV = 71,             ///< Internet Packet Core Utility
    PV_IP_PROTO_CPNX = 72,             ///< Computer Protocol Network Executive
    PV_IP_PROTO_CPHB = 73,             ///< Computer Protocol Heart Beat
    PV_IP_PROTO_WSN = 74,              ///< Wang Span Network
    PV_IP_PROTO_PVP = 75,              ///< Packet Video Protocol
    PV_IP_PROTO_BR_SAT_MON = 76,       ///< Backroom SATNET Monitoring
    PV_IP_PROTO_SUN_ND = 77,           ///< SUN ND PROTOCOL-Temporary
    PV_IP_PROTO_WB_MON = 78,           ///< WIDEBAND Monitoring
    PV_IP_PROTO_WB_EXPAK = 79,         ///< WIDEBAND EXPAK
    PV_IP_PROTO_ISO_IP = 80,           ///< ISO Internet Protocol
    PV_IP_PROTO_VMTP = 81,             ///< VMTP
    PV_IP_PROTO_SECURE_VMTP = 82,      ///< SECURE-VMTP
    PV_IP_PROTO_VINES = 83,            ///< VINES
    PV_IP_PROTO_TTP = 84,              ///< Transaction Transport Protocol
    PV_IP_PROTO_IPTM = 84,             ///< Internet Protocol Traffic Manager
    PV_IP_PROTO_NSFNET_IGP = 85,       ///< NSFNET-IGP
    PV_IP_PROTO_DGP = 86,              ///< Dissimilar Gateway Protocol
    PV_IP_PROTO_TCF = 87,              ///< TCF
    PV_IP_PROTO_EIGRP = 88,            ///< EIGRP
    PV_IP_PROTO_OSPFIGP = 89,          ///< OSPFIGP
    PV_IP_PROTO_Sprite_RPC = 90,       ///< Sprite RPC Protocol
    PV_IP_PROTO_LARP = 91,             ///< Locus Address Resolution Protocol
    PV_IP_PROTO_MTP = 92,              ///< Multicast Transport Protocol
    PV_IP_PROTO_AX_25 = 93,            ///< AX.25 Frames
    PV_IP_PROTO_IPIP = 94,             ///< IP-within-IP Encapsulation Protocol
    PV_IP_PROTO_SCC_SP = 96,           ///< Semaphore Communications Sec. Pro.
    PV_IP_PROTO_ETHERIP = 97,          ///< Ethernet-within-IP Encapsulation
    PV_IP_PROTO_ENCAP = 98,            ///< Encapsulation Header
    PV_IP_PROTO_GMTP = 100,            ///< GMTP
    PV_IP_PROTO_IFMP = 101,            ///< Ipsilon Flow Management Protocol
    PV_IP_PROTO_PNNI = 102,            ///< PNNI over IP
    PV_IP_PROTO_PIM = 103,             ///< Protocol Independent Multicast
    PV_IP_PROTO_ARIS = 104,            ///< ARIS
    PV_IP_PROTO_SCPS = 105,            ///< SCPS
    PV_IP_PROTO_QNX = 106,             ///< QNX
    PV_IP_PROTO_A_N = 107,             ///< Active Networks
    PV_IP_PROTO_IPComp = 108,          ///< IP Payload Compression Protocol
    PV_IP_PROTO_SNP = 109,             ///< Sitara Networks Protocol
    PV_IP_PROTO_Compaq_Peer = 110,     ///< Compaq Peer Protocol
    PV_IP_PROTO_IPX_in_IP = 111,       ///< IPX in IP
    PV_IP_PROTO_VRRP = 112,            ///< Virtual Router Redundancy Protocol
    PV_IP_PROTO_PGM = 113,             ///< PGM Reliable Transport Protocol
    PV_IP_PROTO_L2TP = 115,            ///< Layer Two Tunneling Protocol
    PV_IP_PROTO_DDX = 116,             ///< D-II Data Exchange (DDX)
    PV_IP_PROTO_IATP = 117,            ///< Interactive Agent Transfer Protocol
    PV_IP_PROTO_STP = 118,             ///< Schedule Transfer Protocol
    PV_IP_PROTO_SRP = 119,             ///< SpectraLink Radio Protocol
    PV_IP_PROTO_UTI = 120,             ///< UTI
    PV_IP_PROTO_SMP = 121,             ///< Simple Message Protocol
    PV_IP_PROTO_PTP = 123,             ///< Performance Transparency Protocol
    PV_IP_PROTO_ISIS = 124,            ///<
    PV_IP_PROTO_FIRE = 125,            ///<
    PV_IP_PROTO_CRTP = 126,            ///< Combat Radio Transport Protocol
    PV_IP_PROTO_CRUDP = 127,           ///< Combat Radio User Datagram
    PV_IP_PROTO_SSCOPMCE = 128,        ///<
    PV_IP_PROTO_IPLT = 129,            ///<
    PV_IP_PROTO_SPS = 130,             ///< Secure Packet Shield
    PV_IP_PROTO_PIPE = 131,            ///< Private IP Encapsulation within IP
    PV_IP_PROTO_SCTP = 132,            ///< Stream Control Transmission Protocol
    PV_IP_PROTO_FC = 133,              ///< Fibre Channel
    PV_IP_PROTO_RSVP_E2E_IGNORE = 134, ///<
    PV_IP_PROTO_MH = 135,              ///<
    PV_IP_PROTO_UDPLite = 136,         ///<
    PV_IP_PROTO_MPLS_in_IP = 137,      ///<
    PV_IP_PROTO_manet = 138,           ///< MANET Protocols
    PV_IP_PROTO_HIP = 139,             ///< Host Identity Protocol
    PV_IP_PROTO_Shim6 = 140,           ///< Shim6 Protocol
    PV_IP_PROTO_WESP = 141,            ///< Wrapped Encapsulating Security Payload
    PV_IP_PROTO_ROHC = 142,            ///< Robust Header Compression
};

#ifdef __cplusplus
}
#endif
