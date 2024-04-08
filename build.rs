use std::env;
use std::path;
use std::path::PathBuf;
use std::process;

fn main() {
    let src_dir = path::PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap());
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());

    if std::env::var("DOCS_RS").is_ok() {
        std::fs::copy(src_dir.join("src/bindings.rs"), out_dir.join("bindings.rs"))
            .expect("Failed to copy bindings.rs");
        return;
    }

    let bpftool_src_dir = src_dir.join("bpftool");
    let bpftool_out_dir = out_dir.join("bpftool");
    let xdptools_src_dir = src_dir.join("xdp-tools");
    let xdptools_out_dir = out_dir.join("xdp-tools");
    let libxdp_dir = xdptools_out_dir.join("lib/libxdp");
    let headers_dir = xdptools_out_dir.join("headers/xdp");
    let include_dir = xdptools_out_dir.join("lib/libbpf/src/root/usr/include");
    let libbpf_out_dir = xdptools_out_dir.join("lib/libbpf/src");

    std::fs::create_dir_all(&bpftool_out_dir).expect("Failed to create build dir for bpftool");
    std::fs::create_dir_all(&xdptools_out_dir).expect("Failed to create build dir for xdptools");

    let copy_options = fs_extra::dir::CopyOptions::new()
        .content_only(true)
        .overwrite(true);
    fs_extra::dir::copy(bpftool_src_dir, &bpftool_out_dir, &copy_options)
        .expect("Failed to copy bpftool");
    fs_extra::dir::copy(xdptools_src_dir, &xdptools_out_dir, &copy_options)
        .expect("Failed to copy xdptools");

    println!("Build bpftool on {}", bpftool_out_dir.display());
    let status = process::Command::new("make")
        .current_dir(&bpftool_out_dir)
        .arg("-Csrc")
        .status()
        .expect("Could not execute make for bpftool");

    assert!(status.success(), "Failed to build bpftool");

    let status = process::Command::new("make")
        .current_dir(&xdptools_out_dir)
        .arg(format!(
            "BPFTOOL={}",
            bpftool_out_dir.join("src").join("bpftool").display()
        ))
        .arg("libxdp")
        .status()
        .expect("could not execute make for libxdp");

    assert!(status.success(), "Failed to build libxdp");

    println!("cargo:include={}", headers_dir.display());
    println!("cargo:rustc-link-search={}", libxdp_dir.display());
    println!("cargo:rustc-link-lib=static=xdp");
    println!("cargo:rustc-link-search={}", libbpf_out_dir.display());
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
        .write_to_file(out_dir.join("bindings.rs"))
        .expect("Couldn't write bindings");
}
