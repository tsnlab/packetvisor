use clap::{arg, Command};

use pnet::{
    packet::ipv4::MutableIpv4Packet,
    packet::MutablePacket,
    packet::{
        ethernet::{EtherTypes, MutableEthernetPacket},
        ip::IpNextHeaderProtocols,
        tcp::MutableTcpPacket,
    },
};
use signal_hook::SigId;
use std::{
    io::Error,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc, os::unix::process,
};

fn main() {
    let matches = Command::new("tcp_demo")
        .arg(arg!(nic1: --nic1 <nic1> "nic1 to use").required(true))
        .arg(arg!(nic2: -p --nic2 <nic2> "nic2").required(true))
        .arg(arg!(port: --port <port> "accept port").required(true))
        .arg(
            arg!(filter: --filter <filter> "filter")
                .required(false)
                .default_value(""),
        )
        .arg(
            arg!(chunk_size: -s --chunk-size <size> "Chunk size")
                .required(false)
                .default_value("2048"),
        )
        .arg(
            arg!(chunk_count: -c --chunk-count <count> "Chunk count")
                .required(false)
                .default_value("1024"),
        )
        .arg(
            arg!(rx_ring_size: -r --rx-ring <count> "Rx ring size")
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(tx_ring_size: -t --tx-ring <count> "Tx ring size")
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(fill_ring_size: -f --fill-ring <count> "Fill ring size")
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(completion_ring_size: -o --completion-ring <count> "Completion ring size")
                .required(false)
                .default_value("64"),
        )
        .get_matches();

    let name1 = matches.get_one::<String>("nic1").unwrap().clone();
    let name2 = matches.get_one::<String>("nic2").unwrap().clone();
    let port = matches.get_one::<String>("port").unwrap().parse().unwrap();
    let chunk_size = matches
        .get_one::<String>("chunk_size")
        .unwrap()
        .parse()
        .unwrap();
    let chunk_count = matches
        .get_one::<String>("chunk_count")
        .unwrap()
        .parse()
        .unwrap();
    let rx_ring_size = matches
        .get_one::<String>("rx_ring_size")
        .unwrap()
        .parse()
        .unwrap();
    let tx_ring_size = matches
        .get_one::<String>("tx_ring_size")
        .unwrap()
        .parse()
        .unwrap();
    let filling_ring_size = matches
        .get_one::<String>("fill_ring_size")
        .unwrap()
        .parse()
        .unwrap();
    let completion_ring_size = matches
        .get_one::<String>("completion_ring_size")
        .unwrap()
        .parse()
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

    // udp test

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

    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets1: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);
    let mut packets2: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);

    while !term.load(Ordering::Relaxed) {
        let mut received = nic1.receive(&mut packets1);

        if received > 0 {
            forward(&mut nic1, &mut nic2, &mut packets1, &mut packets2, port);

            packets1.clear();
            packets2.clear();
        }

        received = nic2.receive(&mut packets2);

        if received > 0 {
            forward(&mut nic2, &mut nic1, &mut packets2, &mut packets1, port);

            packets1.clear();
            packets2.clear();
        }

        // while received == 0 {
        //     // No packets received. Sleep
        //     thread::sleep(Duration::from_millis(100));
        //     received = nic1.receive(&mut packets);
        // }
    }
}

fn process_packet(packet: &mut pv::Packet, port: u16) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = match MutableEthernetPacket::new(buffer) {
        Some(eth) => eth,
        None => {
            return false;
        }
    };

    if eth.get_ethertype() == EtherTypes::Arp {
        return true;
    }
    if eth.get_ethertype() != EtherTypes::Ipv4 {
        return false;
    }

    let mut ipv4 = match MutableIpv4Packet::new(eth.payload_mut()) {
        Some(ipv4) => ipv4,
        None => {
            return false;
        }
    };

    if ipv4.get_next_level_protocol() != IpNextHeaderProtocols::Tcp {
        return false;
    }

    let mut tcp = match MutableTcpPacket::new(ipv4.payload_mut()) {
        Some(tcp) => tcp,
        None => {
            return false;
        }
    };

    // if tcp.get_destination() != port {
    //     return false;
    // }

    true
}

fn forward(from: &mut pv::NIC, to: &mut pv::NIC, packets1: &mut Vec<pv::Packet>, packets2: &mut Vec<pv::Packet>, port: u16) {
    for packet in packets1 {
        match process_packet(packet, port) {
            true => {
                packets2.push(to.copy(packet).unwrap());
            }
            false => {
                from.free(packet);
                // packets.remove(i);
            }
        }
    }

    // for packet in packets1 {
    //     packets2.push(to.copy(packet).unwrap());
    // }

    for i in 0.. {
        match to.send(packets2) {
            // if failed to send
            0 => {
                // 3 retries
                if i > 3 {
                    for packet in packets2 {
                        to.free(packet);
                    }
                    break;
                }
            }
            _ => {
                break;
            }
        }
    }
}
