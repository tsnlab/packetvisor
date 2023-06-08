/* ARP example */

use pv::pv::*;
use std::{env, sync::Arc, sync::atomic::{Ordering, AtomicBool}, io::Error};
use pnet::{datalink::{interfaces, NetworkInterface, MacAddr}, packet::arp::ArpPacket};
use signal_hook::SigId;

enum PacketKind {
    ArpReq,
    ArpResp,
    Unused,
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 8 {
        println!("check the number of arguments.");
        std::process::exit(-1);
    }

    let mut if_name = String::new();
    let mut chunk_size: u32 = 0;
    let mut chunk_count: u32 = 0;
    let mut rx_ring_size: u32 = 0;
    let mut tx_ring_size: u32 = 0;
    let mut filling_ring_size: u32 = 0;
    let mut completion_ring_size: u32 = 0;

    for i in 1..args.len() {
        match i {
            1 => { if_name = args[1].clone(); },
            2 => { chunk_size = args[2].parse::<u32>().expect("Check chunk size."); },
            3 => { chunk_count = args[3].parse::<u32>().expect("Check chunk count."); },
            4 => { rx_ring_size = args[4].parse::<u32>().expect("Check rx ring size.");},
            5 => { tx_ring_size = args[5].parse::<u32>().expect("Check tx ring size."); },
            6 => { filling_ring_size = args[6].parse::<u32>().expect("Check filling ring size.");},
            7 => { completion_ring_size = args[7].parse::<u32>().expect("Check filling ring size.");},
            _ => { panic!("abnormal index"); }
        }
    }

    /* signal define to end the application */
    let term:Arc<AtomicBool> = Arc::new(AtomicBool::new(false));
    let result_sigint: Result<SigId, Error> = signal_hook::flag::register(signal_hook::consts::SIGINT, Arc::clone(&term));
    let result_sigterm: Result<SigId, Error> = signal_hook::flag::register(signal_hook::consts::SIGTERM, Arc::clone(&term));
    let result_sigabrt: Result<SigId, Error> = signal_hook::flag::register(signal_hook::consts::SIGABRT, Arc::clone(&term));

    if result_sigint.or(result_sigterm).or(result_sigabrt).is_err() {
        panic!("signal is forbidden");
    }

    /* save source MAC address into variable */
    let all_interfaces: Vec<NetworkInterface> = interfaces();
    let is_exist: Option<&NetworkInterface> = all_interfaces
                            .iter()
                            .find(|element| element.name.as_str() == if_name.as_str());

    let src_mac_address: MacAddr = match is_exist {
        Some(target_interface) => {
            let option: Option<MacAddr> = target_interface.mac;
            if option.is_none() {
                panic!("couldn't find source MAC address");
            }

            option.unwrap()
        },
        None => { panic!("couldn't find source MAC address"); }
    };

    /* execute pv_open() */
    let pv_open_option: Option<PvNic> = pv::pv::pv_open(&if_name, chunk_size, chunk_count, rx_ring_size, tx_ring_size, filling_ring_size, completion_ring_size);
    let mut nic: PvNic;
    if let Some(a) = pv_open_option {
        nic = a;
    } else {
        panic!("nic allocation failed!");
    }

    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets: Vec<PvPacket> = Vec::with_capacity(rx_batch_size as usize);

    while !term.load(Ordering::Relaxed) {
        let received: u32 = pv_receive(&mut nic, &mut packets, rx_batch_size);

        if received > 0 {
            let processed: u32 = process_packets(&mut nic, &mut packets, received, &src_mac_address);
            let sent: u32 = pv_send(&mut nic, &mut packets, processed);

            if sent == 0 {
                for i in (0..processed).rev() {
                    pv_free(&mut nic, &mut packets, i as usize);
                }
            }
        }
    }

    pv::pv::pv_close(nic);
    println!("PV END");
}

fn process_packets(nic: &mut PvNic, packets: &mut Vec<PvPacket>, batch_size: u32, src_mac_address: &MacAddr) -> u32 {
    let mut processed = 0;
    for i in (0..batch_size).rev() {
        let packet_kind: PacketKind = packet_kind_checker(&packets[i as usize]);
        // analyze packet and create packet

        // process packet
        match packet_kind {
            PacketKind::ArpReq => {
                // gen_arp_response_packet();
                println!("ARP Request Packet!");
                processed += 1;
            },
            _ => { pv_free(nic, packets, i as usize); }
        }
    }
    processed
}

// analyze what kind of given packet
fn packet_kind_checker(packet: &PvPacket) -> PacketKind {
    let payload_ptr = unsafe { packet.buffer.add(packet.start as usize).cast_const() };

    unsafe {
        if std::ptr::read(payload_ptr.offset(12)) == 0x08 &&
           std::ptr::read(payload_ptr.offset(13)) == 0x06 &&
           std::ptr::read(payload_ptr.offset(20)) == 0x00 &&
           std::ptr::read(payload_ptr.offset(21)) == 0x01 // ARP request packet
        {
            return PacketKind::ArpReq;
        }
    }

    PacketKind::Unused
}
