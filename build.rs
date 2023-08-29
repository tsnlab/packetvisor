use std::env;
use std::path;
use std::path::PathBuf;
use std::process;

fn main() {
    let src_dir = path::PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());

    let bpftool_dir = src_dir.join("bpftool/src");
    let xdptools_dir = src_dir.join("xdp-tools");
    let libxdp_dir = xdptools_dir.join("lib/libxdp");
    let headers_dir = xdptools_dir.join("headers/xdp");
    let include_dir = xdptools_dir.join("lib/libbpf/src/root/usr/include");
    let libbpf_src_dir = xdptools_dir.join("lib/libbpf/src");

    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());

    let status = process::Command::new("make")
        .current_dir(&bpftool_dir)
        .status()
        .expect("could not execute make bpftool");

    assert!(status.success(), "make failed");

    let status = process::Command::new("make")
        .arg("libxdp")
        .current_dir(&xdptools_dir)
        .status()
        .expect("could not execute make libxdp");

    assert!(status.success(), "make failed");

    println!("cargo:include={}", headers_dir.display());
    println!("cargo:rustc-link-search={}", libxdp_dir.display());
    println!("cargo:rustc-link-lib=static=xdp");
    println!("cargo:rustc-link-search={}", libbpf_src_dir.display());
    println!("cargo:rustc-link-lib=static=bpf");
    println!("cargo:rustc-link-lib=elf");
    println!("cargo:rustc-link-lib=z");

    bindgen::Builder::default()
        .header("bindings.h")
        .clang_arg(format!("-I{}", headers_dir.display()))
        .clang_arg(format!("-I{}", include_dir.display()))
        .generate_inline_functions(true)
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file(out_path.join("bindings.rs"))
        .expect("Couldn't write bindings");
}
