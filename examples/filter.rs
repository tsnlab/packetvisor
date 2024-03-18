use clap::{arg, value_parser, ArgMatches, Command};

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
    let cli_options = parse_cli_options();

    let if_name1 = cli_options.get_one::<String>("nic1").unwrap().clone();
    let if_name2 = cli_options.get_one::<String>("nic2").unwrap().clone();
    let chunk_size = *cli_options.get_one::<usize>("chunk_size").unwrap();
    let chunk_count = *cli_options.get_one::<usize>("chunk_count").unwrap();
    let rx_ring_size = *cli_options.get_one::<usize>("rx_ring_size").unwrap();
    let tx_ring_size = *cli_options.get_one::<usize>("tx_ring_size").unwrap();
    let filling_ring_size = *cli_options.get_one::<usize>("fill_ring_size").unwrap();
    let completion_ring_size = *cli_options
        .get_one::<usize>("completion_ring_size")
        .unwrap();

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
        if let Some(sent_cnt) = do_filtering(&mut nic1, &mut nic2) {
            println!(
                "[{} -> {}] Sent Packet Count : {}",
                nic1.interface.name, nic2.interface.name, sent_cnt
            );
        }

        if let Some(sent_cnt) = do_filtering(&mut nic2, &mut nic1) {
            println!(
                "[{} -> {}] Sent Packet Count : {}",
                nic2.interface.name, nic1.interface.name, sent_cnt
            );
        }

        thread::sleep(Duration::from_millis(100));
    }
}

fn do_filtering(from: &mut pv::Nic, to: &mut pv::Nic) -> Option<usize> {
    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: usize = 64;
    let mut packets = from.receive(rx_batch_size);
    let received = packets.len();

    if 0 == received {
        return None;
    }

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
                send_tcp_rst(from, to, packet);
            }
        }
    }

    for _ in 0..3 {
        let sent_cnt = to.send(&mut filter_packets);

        if sent_cnt > 0 {
            return Some(sent_cnt);
        }
    }

    None
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

    if !word.is_empty() && (tcp.get_source() == port || tcp.get_destination() == port) {
        return !String::from_utf8_lossy(tcp.payload())
            .into_owned()
            .contains(word);
    }

    true
}

fn send_tcp_rst(from: &mut pv::Nic, to: &mut pv::Nic, received: &mut pv::Packet) {
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

fn parse_cli_options() -> ArgMatches {
    Command::new("filter")
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
        .get_matches()
}
