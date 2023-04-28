mod pv;
use std::{env, sync::Arc, sync::atomic::{Ordering, AtomicBool}};
use crate::pv::*;

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

    // println!("{} {} {} {} {} {} {}", if_name, chunk_size, chunk_count, rx_ring_size, tx_ring_size, filling_ring_size, completion_ring_size);

    let pv_open_option: Option<PvNic> = pv::pv_open(if_name, chunk_size, chunk_count, rx_ring_size, tx_ring_size, filling_ring_size, completion_ring_size);

    let mut nic: PvNic;
    if pv_open_option.is_some() {
        nic = pv_open_option.unwrap();
    } else {
        panic!("nic allocation failed!");
    }

    let rx_batch_size: u32 = 64;
    let mut packets: Vec<PvPacket> = Vec::with_capacity(rx_batch_size as usize);

    let term = Arc::new(AtomicBool::new(false));
    signal_hook::flag::register(signal_hook::consts::SIGINT, Arc::clone(&term));
    signal_hook::flag::register(signal_hook::consts::SIGTERM, Arc::clone(&term));
    signal_hook::flag::register(signal_hook::consts::SIGABRT, Arc::clone(&term));

    while !term.load(Ordering::Relaxed) {
        let received: u32 = pv_receive(&mut nic, &mut packets, rx_batch_size);

        // [WIP]
        if received > 0 {
            println!("received: {}", received);
            // let processed: u32 = process_packets(&mut nic, &mut packets, received);
            pv::pv_free(&mut nic, &mut packets);
        }

    }

    pv::pv_close(nic);

    println!("PV END");
}

// [WIP]
fn process_packets(nic: &mut PvNic, packets: &mut Vec<PvPacket>, batch_size: u32) -> u32 {
    let mut processed = 0;

    for i in 0..batch_size {
        let payload_ptr = unsafe { packets[i as usize].buffer.add(packets[i as usize].start as usize).cast_const() };

        // analyze packet and create packet
        unsafe {
            if (std::ptr::read(payload_ptr.offset(12)) == 0x08 &&
                std::ptr::read(payload_ptr.offset(13)) == 0x06 &&
                std::ptr::read(payload_ptr.offset(20)) == 0x00 &&
                std::ptr::read(payload_ptr.offset(21)) == 0x01) // ARP request packet
            {
                println!("ARP request");
                processed += 1;
            } else {
                pv_free(nic, packets);
            }
        }


    }

    processed
}