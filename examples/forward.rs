use clap::{arg, value_parser, ArgMatches, Command};

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
    let dump = *cli_options.get_one::<bool>("dump").unwrap_or(&false);
    let stat = *cli_options.get_one::<bool>("stat").unwrap_or(&false);

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

    if 0 == received {
        return 0;
    }

    if dump {
        for packet in packets.iter_mut() {
            packet.dump();
        }
    }

    for _ in 0..3 {
        let sent_cnt = to.send(&mut packets);

        if sent_cnt > 0 {
            return sent_cnt;
        }
    }

    0
}

fn parse_cli_options() -> ArgMatches {
    Command::new("filter")
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
        .get_matches()
}
