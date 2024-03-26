use clap::{arg, ArgMatches, Command};
use pnet::packet::{
    ethernet::{EtherTypes, MutableEthernetPacket},
    ip::IpNextHeaderProtocols,
    ipv4::{self, MutableIpv4Packet},
    tcp::{ipv4_checksum, MutableTcpPacket, TcpOptionNumbers},
    MutablePacket,
};
use std::{io, net::UdpSocket, thread, time::Duration};

fn main() {
    let cli_options = parse_cli_options();

    let interface = cli_options.get_one::<String>("interface").unwrap();
    let source = cli_options.get_one::<String>("source").unwrap();
    let destination = cli_options.get_one::<String>("destination").unwrap();

    const CHUNK_SIZE: usize = 2048;
    const CHUNK_COUNT: usize = 1024;
    const RX_RING_SIZE: usize = 64;
    const TX_RING_SIZE: usize = 64;
    const FILLING_RING_SIZE: usize = 64;
    const COMPLETION_RING_SIZE: usize = 64;

    let mut interface = pv::Nic::new(
        interface,
        CHUNK_SIZE,
        CHUNK_COUNT,
        FILLING_RING_SIZE,
        COMPLETION_RING_SIZE,
        TX_RING_SIZE,
        RX_RING_SIZE,
    )
    .unwrap_or_else(|err| panic!("Failed to create interface: {}", err));

    let socket = UdpSocket::bind(source).unwrap();
    socket
        .set_nonblocking(true)
        .expect("Socket Nonblocking Error");

    const RX_BATCH_SIZE: usize = 64;
    const ETH_HEADER: usize = 14;
    const ETH_MTU: usize = 1500;
    let mut buf = [0; ETH_HEADER + ETH_MTU];
    loop {
        // Listening for interface
        let mut packets = interface.receive(RX_BATCH_SIZE);
        let received = packets.len();

        if received > 0 {
            for packet in &mut packets {
                // check TCP handshake & update MMS field
                // 1500 - 20(IPv4) - 8(UDP) - 14(ETH) - 20(IPv4) - 20(TCP)
                const MMS: u16 = 1418;
                set_mms(packet, MMS);

                // send received packet to destination socket
                socket
                    .send_to(packet.get_buffer_mut(), destination)
                    .expect("Socket send Error");
            }
            packets.clear();
        }

        // Listening for socket
        match socket.recv_from(&mut buf) {
            Ok((n, _addr)) => {
                // received data from socket and send to interface
                let mut packet = interface.alloc_packet().unwrap();
                let fit_buffer = &buf[0..n];
                packet.replace_data(fit_buffer).unwrap();
                interface.send(&mut vec![packet]);
                buf.fill(0);
            }
            Err(ref e) if e.kind() == io::ErrorKind::WouldBlock => {
                // wait until network socket is ready, typically implemented
                // via platform-specific APIs such as epoll or IOCP
                thread::sleep(Duration::from_millis(100));
            }
            Err(e) => panic!("encountered IO error: {e}"),
        }
    }
}

fn set_mms(packet: &mut pv::Packet, mms: u16) {
    let mut eth = MutableEthernetPacket::new(packet.get_buffer_mut()).unwrap();

    let mut ipv4 = match eth.get_ethertype() {
        EtherTypes::Ipv4 => MutableIpv4Packet::new(eth.payload_mut()).unwrap(),
        _ => {
            return;
        }
    };

    let source_ip_addr = ipv4.get_source();
    let destination_ip_addr = ipv4.get_destination();

    let mut tcp = match ipv4.get_next_level_protocol() {
        IpNextHeaderProtocols::Tcp => MutableTcpPacket::new(ipv4.payload_mut()).unwrap(),
        _ => {
            return;
        }
    };

    let options = tcp.get_options_iter();
    let mut index = 0;

    // check that TCP packet has MMS option & set MMS field
    for option in options.into_iter() {
        match option.get_number() {
            TcpOptionNumbers::MSS => {
                index += 2;
                let option_raw = tcp.get_options_raw_mut();
                let mms_slice = mms.to_be_bytes();
                option_raw[index] = mms_slice[0];
                option_raw[index + 1] = mms_slice[1];
                break;
            }
            _ => match option.get_length().len() {
                0 => index += 1,
                _ => index += option.get_length()[0] as usize,
            },
        }
    }
    tcp.set_checksum(ipv4_checksum(
        &tcp.to_immutable(),
        &source_ip_addr,
        &destination_ip_addr,
    ));
    ipv4.set_checksum(ipv4::checksum(&ipv4.to_immutable()));
}

fn parse_cli_options() -> ArgMatches {
    Command::new("tunnel")
        .arg(arg!(interface: -i --interface <interface> "Interface to send or receive packet from inner host.").required(true))
        .arg(arg!(source: -s --source <source> "Source IP address:port to open and bind UDP socket. ex) 192.160.0.1:8080").required(true))
        .arg(
            arg!(destination: -d --destination <destination> "Destination IP address:port to send packet. ex) 192.160.0.2:8080")
            .required(true),
            )
        .get_matches()
}
