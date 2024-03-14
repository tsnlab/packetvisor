use clap::{arg, value_parser, Command};

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
    let mut dump: bool = false;
    let mut stat: bool = false;

    do_command(
        &mut if_name1,
        &mut if_name2,
        &mut chunk_size,
        &mut chunk_count,
        &mut rx_ring_size,
        &mut tx_ring_size,
        &mut filling_ring_size,
        &mut completion_ring_size,
        &mut dump,
        &mut stat,
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
        let processed1 = forward(&mut nic1, &mut nic2, dump);
        let processed2 = forward(&mut nic2, &mut nic1, dump);

        if processed1 + processed2 == 0 {
            thread::sleep(Duration::from_millis(100));
        } else if stat {
            println!("Processed {}, {} packets", processed1, processed2);
        }
    }
}

fn forward(from: &mut pv::Nic, to: &mut pv::Nic, dump: bool) -> usize {
    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size = 64;
    let mut packets = from.receive(rx_batch_size);
    let received = packets.len();

    if 0 >= received {
        return 0;
    }

    if dump {
        for packet in packets.iter_mut() {
            packet.dump();
        }
    }

    for _ in 0..4 {
        let sent_cnt = to.send(&mut packets);

        if sent_cnt > 0 {
            return sent_cnt;
        }
    }

    0
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
    dump: &mut bool,
    stat: &mut bool,
) {
    let matches = Command::new("filter")
        .arg(arg!(nic1: <nic1> "nic1 to use").required(true))
        .arg(arg!(nic2: <nic2> "nic2 to use").required(true))
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
        .arg(arg!(dump: -d --dump "Dump packets").required(false))
        .arg(arg!(stat: -S --stat "Show statistics").required(false))
        .get_matches();

    *if_name1 = matches.get_one::<String>("nic1").unwrap().clone();
    *if_name2 = matches.get_one::<String>("nic2").unwrap().clone();
    *chunk_size = *matches.get_one::<usize>("chunk_size").unwrap();
    *chunk_count = *matches.get_one::<usize>("chunk_count").unwrap();
    *rx_ring_size = *matches.get_one::<usize>("rx_ring_size").unwrap();
    *tx_ring_size = *matches.get_one::<usize>("tx_ring_size").unwrap();
    *filling_ring_size = *matches.get_one::<usize>("fill_ring_size").unwrap();
    *completion_ring_size = *matches.get_one::<usize>("completion_ring_size").unwrap();
    *dump = *matches.get_one::<bool>("dump").unwrap_or(&false);
    *stat = *matches.get_one::<bool>("stat").unwrap_or(&false);
}
