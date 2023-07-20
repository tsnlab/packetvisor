use clap::{arg, value_parser, Command};

use pnet::{
    packet::ipv4::MutableIpv4Packet,
    packet::{
        ethernet::{EtherTypes, MutableEthernetPacket},
        ip::IpNextHeaderProtocols,
        tcp::{MutableTcpPacket, TcpFlags},
    },
    packet::{MutablePacket, Packet},
};
use signal_hook::SigId;
use std::{
    io::Error,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::Duration,
};

fn main() {
    let matches = Command::new("filter")
        .arg(arg!(nic1: --nic1 <nic1> "nic1 to use").required(true))
        .arg(arg!(nic2: -p --nic2 <nic2> "nic2 to use").required(true))
        .arg(
            arg!(chunk_size: -s --"chunk-size" <size> "Chunk size")
                .value_parser(value_parser!(usize))
                .required(false)
                .default_value("2048"),
        )
        .arg(
            arg!(chunk_count: -c --"chunk-count" <count> "Chunk count")
                .required(false)
                .value_parser(value_parser!(usize))
                .default_value("1024"),
        )
        .arg(
            arg!(rx_ring_size: -r --"rx-ring" <count> "Rx ring size")
                .value_parser(value_parser!(u32))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(tx_ring_size: -t --"tx-ring" <count> "Tx ring size")
                .value_parser(value_parser!(u32))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(fill_ring_size: -f --"fill-ring" <count> "Fill ring size")
                .value_parser(value_parser!(u32))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(completion_ring_size: -o --"completion-ring" <count> "Completion ring size")
                .value_parser(value_parser!(u32))
                .required(false)
                .default_value("64"),
        )
        .get_matches();

    let name1 = matches.get_one::<String>("nic1").unwrap().clone();
    let name2 = matches.get_one::<String>("nic2").unwrap().clone();
    let chunk_size = *matches.get_one::<usize>("chunk_size").unwrap();
    let chunk_count = *matches.get_one::<usize>("chunk_count").unwrap();
    let rx_ring_size = *matches.get_one::<u32>("rx_ring_size").unwrap();
    let tx_ring_size = *matches.get_one::<u32>("tx_ring_size").unwrap();
    let filling_ring_size = *matches.get_one::<u32>("fill_ring_size").unwrap();
    let completion_ring_size = *matches.get_one::<u32>("completion_ring_size").unwrap();

    // Signal handlers
    let term: Arc<AtomicBool> = Arc::new(AtomicBool::new(false));
    let result_sigint: Result<SigId, Error> =
        signal_hook::flag::register(signal_hook::consts::SIGINT, Arc::clone(&term));
    let result_sigterm: Result<SigId, Error> =
        signal_hook::flag::register(signal_hook::consts::SIGTERM, Arc::clone(&term));
    let result_sigabrt: Result<SigId, Error> =
        signal_hook::flag::register(signal_hook::consts::SIGABRT, Arc::clone(&term));

    if result_sigint.or(result_sigterm).or(result_sigabrt).is_err() {
        panic!("signal is forbidden");
    }

    let mut nic1 = pv::NIC::new(&name1, chunk_size, chunk_count).unwrap();
    nic1.open(
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    )
    .unwrap();
    let mut nic2 = pv::NIC::new(&name2, chunk_size, chunk_count).unwrap();
    nic2.open(
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    )
    .unwrap();

    while !term.load(Ordering::Relaxed) {
        forward(&mut nic1, &mut nic2);
        forward(&mut nic2, &mut nic1);
    }
}

fn process_packet(packet: &mut pv::Packet) -> bool {
    let buffer = packet.get_buffer_mut();
    let port = 80; // filter port
    let word = "pineapple"; // filter word

    let mut eth = match MutableEthernetPacket::new(buffer) {
        Some(eth) => eth,
        None => {
            return false;
        }
    };

    if eth.get_ethertype() == EtherTypes::Arp {
        return true;
    }

    let mut ipv4 = match MutableIpv4Packet::new(eth.payload_mut()) {
        Some(ipv4) => ipv4,
        None => {
            return false;
        }
    };

    let tcp = match MutableTcpPacket::new(ipv4.payload_mut()) {
        Some(tcp) => tcp,
        None => {
            return false;
        }
    };

    if word.len() > 0 && (tcp.get_source() == port || tcp.get_destination() == port) {
        return !String::from_utf8_lossy(tcp.payload())
            .into_owned()
            .contains(word);
    }

    true
}

fn forward(from: &mut pv::NIC, to: &mut pv::NIC) {
    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets1: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);
    let mut packets2: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);
    let received = from.receive(&mut packets1);

    if received > 0 {
        for packet in &mut packets1 {
            match process_packet(packet) {
                true => {
                    packets2.push(to.copy_from(packet).unwrap());
                }
                false => {
                    send_rst(from, to, packet);
                }
            }
        }

        for retry in (0..3).rev() {
            match (to.send(&mut packets2), retry) {
                (cnt, _) if cnt > 0 => break, // Success
                (0, 0) => {
                    // Failed 3 times
                    for packet in packets2 {
                        to.free(&packet);
                    }
                    break;
                }
                _ => continue, // Retrying
            }
        }
    } else {
        // No packets received. Sleep
        thread::sleep(Duration::from_millis(100));
    }
}

fn send_rst(from: &mut pv::NIC, to: &mut pv::NIC, received: &mut pv::Packet) {
    let mut packet = from.alloc().unwrap();

    if packet
        .replace_data(&received.get_buffer_mut().to_vec())
        .is_err()
    {
        return;
    }

    let mut eth = match MutableEthernetPacket::new(packet.get_buffer_mut()) {
        Some(eth) => eth,
        None => return,
    };
    let src = eth.get_source();

    eth.set_source(eth.get_destination());
    eth.set_destination(src);

    let mut ipv4 = match MutableIpv4Packet::new(eth.payload_mut()) {
        Some(ipv4) => ipv4,
        None => return,
    };
    let src_ip = ipv4.get_source();
    let dst_ip = ipv4.get_destination();
    let mut tcp = match MutableTcpPacket::new(ipv4.payload_mut()) {
        Some(tcp) => tcp,
        None => return,
    };
    let src_port = tcp.get_source();

    tcp.set_source(tcp.get_destination());
    tcp.set_destination(src_port);
    tcp.set_sequence(tcp.get_acknowledgement());
    tcp.set_flags(TcpFlags::RST);
    tcp.set_checksum(pnet::packet::tcp::ipv4_checksum(
        &tcp.to_immutable(),
        &dst_ip,
        &src_ip,
    ));
    ipv4.set_source(dst_ip);
    ipv4.set_destination(src_ip);
    ipv4.set_checksum(pnet::packet::ipv4::checksum(&ipv4.to_immutable()));
    from.send(&mut vec![packet]);
    from.free(&packet);

    let mut packet = to.alloc().unwrap();

    if packet
        .replace_data(&received.get_buffer_mut().to_vec())
        .is_err()
    {
        return;
    }

    let mut eth = match MutableEthernetPacket::new(packet.get_buffer_mut()) {
        Some(eth) => eth,
        None => return,
    };

    let mut ipv4 = match MutableIpv4Packet::new(eth.payload_mut()) {
        Some(ipv4) => ipv4,
        None => return,
    };
    let src_ip = ipv4.get_source();
    let dst_ip = ipv4.get_destination();
    let mut tcp = match MutableTcpPacket::new(ipv4.payload_mut()) {
        Some(tcp) => tcp,
        None => return,
    };

    tcp.set_flags(TcpFlags::RST);
    tcp.set_checksum(pnet::packet::tcp::ipv4_checksum(
        &tcp.to_immutable(),
        &src_ip,
        &dst_ip,
    ));
    ipv4.set_checksum(pnet::packet::ipv4::checksum(&ipv4.to_immutable()));
    to.send(&mut vec![packet]);
    to.free(&packet);
}
