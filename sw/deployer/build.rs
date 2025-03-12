fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("cargo::rerun-if-changed=../bootloader/src/");

    let out_dir = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());

    cc::Build::new()
        .file("../bootloader/src/fatfs/ff.c")
        .file("../bootloader/src/fatfs/ffsystem.c")
        .file("../bootloader/src/fatfs/ffunicode.c")
        .compile("fatfs");

    bindgen::Builder::default()
        .header("../bootloader/src/fatfs/ff.h")
        .blocklist_function("get_fattime")
        .generate()
        .expect("Unable to generate FatFs bindings")
        .write_to_file(out_dir.join("fatfs_bindings.rs"))
        .expect("Unable to write FatFs bindings");

    Ok(())
}
