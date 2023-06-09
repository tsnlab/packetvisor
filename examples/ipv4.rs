use pv::pv::*;

use pnet::datalink::{interfaces, MacAddr, NetworkInterface};
use signal_hook::SigId;
use std::{
    env,
    io::Error,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        println!("check the number of arguments.");
        std::process::exit(-1);
    }

    let mut if_name = String::new();
    let chunk_size: u32 = 2048;
    let chunk_count: u32 = 1024;
    let rx_ring_size: u32 = 64;
    let tx_ring_size: u32 = 64;
    let filling_ring_size: u32 = 64;
    let completion_ring_size: u32 = 64;

    for i in 1..args.len() {
        match i {
            1 => {
                if_name = args[1].clone();
            }
            _ => {
                panic!("abnormal index");
            }
        }
    }

    /* signal define to end the application */
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
        }
        None => {
            panic!("couldn't find source MAC address");
        }
    };

    /* execute pv_open() */
    let pv_open_option: Option<PvNic> = pv::pv::pv_open(
        &if_name,
        chunk_size,
        chunk_count,
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    );
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
            unsafe {
                let payload_ptr = packets[0 as usize]
                    .buffer
                    .add(packets[0 as usize].start as usize)
                    .cast_const();
                if std::ptr::read(payload_ptr.offset(12)) == 0x08
                    && std::ptr::read(payload_ptr.offset(13)) == 0x00
                    && std::ptr::read(payload_ptr.offset(13)) >> 4 == 4
                {
                    packet_dump(&mut packets[0 as usize]);
                }
            }
        }
    }

    pv::pv::pv_close(nic);
    println!("PV END");
}

fn packet_dump(packet: &PvPacket) {
    let buffer_address: *const u8 = packet.buffer.cast_const();

    let length: u32 = packet.size;
    let mut count: u32 = 0;

    unsafe {
        loop {
            let read_offset: usize = count as usize;
            let read_address: *const u8 = buffer_address.add(read_offset);
            print!("{:02X?} ", std::ptr::read(read_address));

            count += 1;
            if count == length {
                break;
            } else if count % 8 == 0 {
                print!(" ");
                if count % 16 == 0 {
                    println!();
                }
            }
        }
    }
    println!("\n-------\n");
}
