use clap::{arg, value_parser, Command};

use pnet::{
    packet::ipv4::MutableIpv4Packet,
    packet::{
        ethernet::MutableEthernetPacket,
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
    let mut if_name1: String = Default::default();
    let mut if_name2: String = Default::default();
    let mut chunk_size: usize = 0;
    let mut chunk_count: usize = 0;
    let mut rx_ring_size: usize = 0;
    let mut tx_ring_size: usize = 0;
    let mut filling_ring_size: usize = 0;
    let mut completion_ring_size: usize = 0;

    do_command(
        &mut if_name1,
        &mut if_name2,
        &mut chunk_size,
        &mut chunk_count,
        &mut rx_ring_size,
        &mut tx_ring_size,
        &mut filling_ring_size,
        &mut completion_ring_size,
    );

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

    let mut nic1 = match pv::Nic::new(
        &if_name1,
        chunk_size,
        chunk_count,
        filling_ring_size,
        completion_ring_size,
        tx_ring_size,
        rx_ring_size,
    ) {
        Ok(nic) => nic,
        Err(err) => {
            panic!("Failed to create NIC1: {}", err);
        }
    };

    let mut nic2 = match pv::Nic::new(
        &if_name2,
        chunk_size,
        chunk_count,
        filling_ring_size,
        completion_ring_size,
        tx_ring_size,
        rx_ring_size,
    ) {
        Ok(nic) => nic,
        Err(err) => {
            panic!("Failed to create NIC2: {}", err);
        }
    };

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
        None => return true,
    };

    let mut ipv4 = match MutableIpv4Packet::new(eth.payload_mut()) {
        Some(ipv4) => ipv4,
        None => return true,
    };

    let tcp = match MutableTcpPacket::new(ipv4.payload_mut()) {
        Some(tcp) => tcp,
        None => return true,
    };

    if word.len() > 0 && (tcp.get_source() == port || tcp.get_destination() == port) {
        return !String::from_utf8_lossy(tcp.payload())
            .into_owned()
            .contains(word);
    }

    true
}

fn forward(from: &mut pv::Nic, to: &mut pv::Nic) {
    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: usize = 64;
    let mut packets = from.receive(rx_batch_size);
    let received = packets.len();

    if received > 0 {
        let mut filter_packets: Vec<pv::Packet> = Vec::with_capacity(received);
        for packet in &mut packets {
            match process_packet(packet) {
                true => {
                    let packet_data = packet.get_buffer_mut().to_vec();
                    let mut filter_packet = to.alloc_packet().unwrap();
                    filter_packet.replace_data(&packet_data).unwrap();

                    filter_packets.push(filter_packet);
                }
                false => {
                    send_rst(from, to, packet);
                }
            }
        }

        for retry in (0..3).rev() {
            match (to.send(&mut filter_packets), retry) {
                (cnt, _) if cnt > 0 => break, // Success
                (0, 0) => {
                    // Failed 3 times
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

fn send_rst(from: &mut pv::Nic, to: &mut pv::Nic, received: &mut pv::Packet) {
    let mut packet = from.alloc_packet().unwrap();

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

    let mut packet = to.alloc_packet().unwrap();

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
}

fn do_command(
    if_name1: &mut String,
    if_name2: &mut String,
    chunk_size: &mut usize,
    chunk_count: &mut usize,
    rx_ring_size: &mut usize,
    tx_ring_size: &mut usize,
    filling_ring_size: &mut usize,
    completion_ring_size: &mut usize,
) {
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
                .value_parser(value_parser!(usize))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(tx_ring_size: -t --"tx-ring" <count> "Tx ring size")
                .value_parser(value_parser!(usize))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(fill_ring_size: -f --"fill-ring" <count> "Fill ring size")
                .value_parser(value_parser!(usize))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(completion_ring_size: -o --"completion-ring" <count> "Completion ring size")
                .value_parser(value_parser!(usize))
                .required(false)
                .default_value("64"),
        )
        .get_matches();

    *if_name1 = matches.get_one::<String>("nic1").unwrap().clone();
    *if_name2 = matches.get_one::<String>("nic2").unwrap().clone();
    *chunk_size = *matches.get_one::<usize>("chunk_size").unwrap();
    *chunk_count = *matches.get_one::<usize>("chunk_count").unwrap();
    *rx_ring_size = *matches.get_one::<usize>("rx_ring_size").unwrap();
    *tx_ring_size = *matches.get_one::<usize>("tx_ring_size").unwrap();
    *filling_ring_size = *matches.get_one::<usize>("fill_ring_size").unwrap();
    *completion_ring_size = *matches.get_one::<usize>("completion_ring_size").unwrap();
}
