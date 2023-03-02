mod debug;
mod n64;
mod sc64;

use chrono::Local;
use clap::{Args, Parser, Subcommand, ValueEnum};
use clap_num::maybe_hex_range;
use colored::Colorize;
use debug::handle_debug_packet;
use panic_message::panic_message;
use std::io::{Read, Write};
use std::path::PathBuf;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;
use std::{panic, process, thread};

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// Use SC64 device matching provided serial number
    #[arg(long)]
    sn: Option<String>,
}

#[derive(Subcommand)]
enum Commands {
    /// List connected SC64 devices
    List,

    /// Upload ROM (and save) to the SC64
    Upload(UploadArgs),

    /// Upload ROM, 64DD IPL and run disk server
    _64DD(_64DDArgs),

    /// Enter debug mode
    Debug(DebugArgs),

    /// Dump data from arbitrary location in SC64 memory space
    Dump(DumpArgs),

    /// Print information about SC64 device
    Info,

    /// Update persistent settings on SC64 device
    Set {
        #[command(subcommand)]
        command: SetCommands,
    },

    /// Print firmware metadata / update or backup SC64 firmware
    Firmware {
        #[command(subcommand)]
        command: FirmwareCommands,
    },
}

#[derive(Args)]
struct UploadArgs {
    /// Path to the ROM file
    rom: PathBuf,

    /// Path to the save file
    #[arg(short, long)]
    save: Option<PathBuf>,

    /// Override autodetected save type
    #[arg(short = 't', long)]
    save_type: Option<SaveType>,

    /// Use direct boot mode (skip bootloader)
    #[arg(short, long)]
    direct: bool,

    /// Do not put last 128 kiB of ROM inside flash memory (can corrupt non EEPROM saves)
    #[arg(short, long)]
    no_shadow: bool,

    /// Force TV type (ignored when used in conjunction with direct boot mode)
    #[arg(long)]
    tv: Option<TvType>,
}

#[derive(Args)]
struct _64DDArgs {
    /// Path to the ROM file
    #[arg(short, long)]
    rom: Option<PathBuf>,

    /// Path to the 64DD IPL file
    ddipl: PathBuf,

    /// Path to the 64DD disk file (.ndd format, can be specified multiple times)
    disk: Vec<PathBuf>,

    /// Use direct boot mode (skip bootloader)
    #[arg(short, long)]
    direct: bool,
}

#[derive(Args)]
struct DebugArgs {
    /// Enable IS-Viewer64 and set listening address at ROM offset (in most cases it's fixed at 0x03FF0000)
    #[arg(long, value_name = "offset", value_parser = |s: &str| maybe_hex_range::<u32>(s, 0x00000004, 0x03FF0000))]
    isv: Option<u32>,

    /// Expose TCP socket port for GDB debugging
    #[arg(long, value_name = "port", value_parser = clap::value_parser!(u16).range(1..))]
    gdb: Option<u16>,
}

#[derive(Args)]
struct DumpArgs {
    /// Starting memory address
    #[arg(value_parser = |s: &str| maybe_hex_range::<u32>(s, 0, sc64::MEMORY_LENGTH as u32))]
    address: u32,

    /// Dump length
    #[arg(value_parser = |s: &str| maybe_hex_range::<usize>(s, 1, sc64::MEMORY_LENGTH))]
    length: usize,

    /// Path to the dump file
    path: PathBuf,
}

#[derive(Subcommand)]
enum SetCommands {
    /// Synchronize real time clock (RTC) on the SC64 with local system time
    Rtc,

    /// Enable LED I/O activity blinking
    BlinkOn,

    /// Disable LED I/O activity blinking
    BlinkOff,
}

#[derive(Subcommand)]
enum FirmwareCommands {
    /// Print metadata included inside SC64 firmware file
    Info(FirmwareArgs),

    /// Download current SC64 firmware and save it to provided file
    Backup(FirmwareArgs),

    /// Update SC64 firmware from provided file
    Update(FirmwareArgs),
}

#[derive(Args)]
struct FirmwareArgs {
    /// Path to the firmware file
    firmware: PathBuf,
}

#[derive(Clone, Debug, ValueEnum)]
enum SaveType {
    None,
    Eeprom4k,
    Eeprom16k,
    Sram,
    SramBanked,
    Flashram,
}

#[derive(Clone, Debug, ValueEnum)]
enum TvType {
    PAL,
    NTSC,
    MPAL,
}

fn main() {
    let cli = Cli::parse();

    panic::set_hook(Box::new(|_| {}));

    match panic::catch_unwind(|| handle_command(&cli.command, cli.sn)) {
        Ok(_) => {}
        Err(payload) => {
            eprintln!("{}", panic_message(&payload).red());
            process::exit(1);
        }
    }
}

fn init_sc64(sn: Option<String>, check_firmware: bool) -> Result<sc64::SC64, sc64::Error> {
    let mut sc64 = sc64::new(sn)?;

    if check_firmware {
        sc64.check_firmware_version()?;
    }

    Ok(sc64)
}

fn setup_exit_flag() -> Arc<AtomicBool> {
    let exit_flag = Arc::new(AtomicBool::new(false));
    let handler_exit_flag = exit_flag.clone();

    ctrlc::set_handler(move || {
        handler_exit_flag.store(true, Ordering::Relaxed);
    })
    .unwrap();

    exit_flag
}

fn handle_command(command: &Commands, sn: Option<String>) {
    let result = match command {
        Commands::Upload(args) => handle_upload_command(sn, args),
        Commands::_64DD(args) => handle_64dd_command(sn, args),
        Commands::Dump(args) => handle_dump_command(sn, args),
        Commands::Debug(args) => handle_debug_command(sn, args),
        Commands::Info => handle_info_command(sn),
        Commands::Set { command } => handle_set_command(sn, command),
        Commands::Firmware { command } => handle_firmware_command(sn, command),
        Commands::List => handle_list_command(),
    };
    match result {
        Ok(()) => {}
        Err(error) => panic!("{error}"),
    };
}

fn handle_upload_command(sn: Option<String>, args: &UploadArgs) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(sn, true)?;

    sc64.reset_state()?;

    let rom_path = args.rom.to_str().unwrap();

    print!(
        "Uploading ROM [{}]... ",
        args.rom.file_name().unwrap().to_str().unwrap()
    );
    std::io::stdout().flush().unwrap();
    sc64.upload_rom(rom_path, args.no_shadow)?;
    println!("done");

    // TODO: autodetect save

    let args_save_type = args.save_type.as_ref().unwrap_or(&SaveType::None);
    let save_type = match args_save_type {
        SaveType::None => sc64::SaveType::None,
        SaveType::Eeprom4k => sc64::SaveType::Eeprom4k,
        SaveType::Eeprom16k => sc64::SaveType::Eeprom16k,
        SaveType::Sram => sc64::SaveType::Sram,
        SaveType::SramBanked => sc64::SaveType::SramBanked,
        SaveType::Flashram => sc64::SaveType::Flashram,
    };
    sc64.set_save_type(save_type)?;

    if args.save.is_some() {
        let save = args.save.as_ref().unwrap();

        print!(
            "Uploading save [{}]... ",
            save.file_name().unwrap().to_str().unwrap()
        );
        std::io::stdout().flush().unwrap();
        sc64.upload_save(save.to_str().unwrap())?;
        println!("done");
    }

    println!("Save type set to [{args_save_type:?}]");

    let boot_mode = if args.direct {
        sc64::BootMode::DirectRom
    } else {
        sc64::BootMode::Rom
    };

    sc64.set_boot_mode(boot_mode)?;
    println!("Boot mode set to [{:?}]", boot_mode);

    if let Some(tv) = args.tv.as_ref() {
        if args.direct {
            println!("TV type ignored because direct boot mode is enabled");
        } else {
            sc64.set_tv_type(match tv {
                TvType::PAL => sc64::TvType::PAL,
                TvType::NTSC => sc64::TvType::NTSC,
                TvType::MPAL => sc64::TvType::MPAL,
            })?;
            println!("TV type set to [{tv:?}]");
        }
    }

    sc64.calculate_cic_parameters()?;

    Ok(())
}

fn handle_64dd_command(sn: Option<String>, args: &_64DDArgs) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(sn, true)?;

    // TODO: parse 64DD disk files

    sc64.reset_state()?;

    let ddipl_path = args.ddipl.to_str().unwrap();

    print!(
        "Uploading DDIPL [{}]... ",
        args.ddipl.file_name().unwrap().to_str().unwrap()
    );
    std::io::stdout().flush().unwrap();
    sc64.upload_ddipl(ddipl_path)?;
    println!("done");

    // TODO: upload other stuff

    // TODO: set boot mode

    sc64.calculate_cic_parameters()?;

    let exit = setup_exit_flag();
    while exit.load(Ordering::Relaxed) {
        if let Some(data_packet) = sc64.receive_data_packet()? {
            match data_packet {
                sc64::DataPacket::Disk(_disk_packet) => {
                    // TODO: handle 64DD packet
                }
                _ => {}
            }
        } else {
            thread::sleep(Duration::from_millis(1));
        }
    }

    Ok(())
}

fn handle_dump_command(sn: Option<String>, args: &DumpArgs) -> Result<(), sc64::Error> {
    let dump_path = &args.path;

    let mut file = std::fs::File::create(dump_path)?;

    let mut sc64 = init_sc64(sn, true)?;

    print!(
        "Dumping from [0x{:08X}] length [0x{:X}] to [{}]... ",
        args.address,
        args.length,
        dump_path.file_name().unwrap().to_str().unwrap()
    );
    std::io::stdout().flush().unwrap();
    let data = sc64.dump_memory(args.address, args.length)?;

    file.write(&data)?;

    println!("done");

    Ok(())
}

fn handle_debug_command(sn: Option<String>, args: &DebugArgs) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(sn, true)?;

    if args.isv.is_some() {
        sc64.configure_isviewer64(args.isv)?;
    }

    println!("{}", "Debug mode started".bold());

    let exit = setup_exit_flag();
    while !exit.load(Ordering::Relaxed) {
        if let Some(data_packet) = sc64.receive_data_packet()? {
            match data_packet {
                sc64::DataPacket::IsViewer(message) => print!("{}", message),
                sc64::DataPacket::Debug(debug_packet) => handle_debug_packet(debug_packet),
                _ => {}
            }
        } else {
            thread::sleep(Duration::from_micros(1));
        }
    }

    println!("{}", "Debug mode ended".bold());

    if args.isv.is_some() {
        sc64.configure_isviewer64(None)?;
    }

    Ok(())
}

fn handle_info_command(sn: Option<String>) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(sn, false)?;

    let (major, minor) = sc64.check_firmware_version()?;
    let state = sc64.get_device_state()?;
    let datetime = state.datetime.format("%Y-%m-%d %H:%M:%S %Z");

    println!("{}", "SC64 information and current state:".bold());
    println!(" Firmware version:      {major}.{minor}");
    println!(" RTC datetime:          {}", datetime);
    println!(" LED blink enabled:     {}", state.led_enable);
    println!(" Bootloader switch:     {}", state.bootloader_switch);
    println!(" ROM write enabled:     {}", state.rom_write_enable);
    println!(" ROM shadow enabled:    {}", state.rom_shadow_enable);
    println!(" ROM extended enabled:  {}", state.rom_extended_enable);
    println!(" Boot mode:             {:?}", state.boot_mode);
    println!(" Save type:             {:?}", state.save_type);
    println!(" CIC seed:              {:?}", state.cic_seed);
    println!(" TV type:               {:?}", state.tv_type);
    println!(" 64DD mode:             {:?}", state.dd_mode);
    println!(" 64DD SD card mode:     {}", state.dd_sd_enable);
    println!(" 64DD drive type:       {:?}", state.dd_drive_type);
    println!(" 64DD disk state:       {:?}", state.dd_disk_state);
    println!(" Button mode:           {:?}", state.button_mode);
    println!(" Button state:          {}", state.button_state);
    println!(" IS-Viewer 64 offset:   0x{:08X}", state.isv_address);

    Ok(())
}

fn handle_set_command(sn: Option<String>, command: &SetCommands) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(sn, true)?;

    match command {
        SetCommands::Rtc => {
            let datetime = Local::now();
            sc64.set_datetime(datetime)?;
            println!(
                "SC64 RTC datetime synchronized to: {}",
                datetime.format("%Y-%m-%d %H:%M:%S %Z").to_string().green()
            );
        }

        SetCommands::BlinkOn => {
            sc64.set_led_blink(true)?;
            println!(
                "SC64 LED I/O activity blinking set to {}",
                "enabled".green()
            );
        }

        SetCommands::BlinkOff => {
            sc64.set_led_blink(false)?;
            println!("SC64 LED I/O activity blinking set to {}", "disabled".red());
        }
    }

    Ok(())
}

fn handle_firmware_command(
    sn: Option<String>,
    command: &FirmwareCommands,
) -> Result<(), sc64::Error> {
    match command {
        FirmwareCommands::Info(args) => {
            let firmware_path = &args.firmware;

            let mut file = std::fs::File::open(firmware_path)?;
            let length = file.metadata()?.len();
            let mut buffer = vec![0u8; length as usize];
            file.read_exact(&mut buffer)?;

            // TODO: print firmware metadata

            Ok(())
        }

        FirmwareCommands::Backup(args) => {
            let backup_path = &args.firmware;

            let mut file = std::fs::File::create(backup_path)?;

            let mut sc64 = init_sc64(sn, false)?;

            print!(
                "Generating firmware backup, this might take a while [{}]... ",
                backup_path.file_name().unwrap().to_str().unwrap()
            );
            std::io::stdout().flush().unwrap();
            let data = sc64.backup_firmware()?;
            file.write(&data)?;
            println!("done");

            Ok(())
        }

        FirmwareCommands::Update(args) => {
            let update_path = &args.firmware;

            let mut file = std::fs::File::open(update_path)?;
            let length = file.metadata()?.len();
            let mut buffer = vec![0u8; length as usize];
            file.read_exact(&mut buffer)?;

            // TODO: print firmware metadata

            let mut sc64 = init_sc64(sn, false)?;

            print!(
                "Updating firmware, this might take a while [{}]... ",
                update_path.file_name().unwrap().to_str().unwrap()
            );
            std::io::stdout().flush().unwrap();
            sc64.update_firmware(&buffer)?;
            println!("done");

            Ok(())
        }
    }
}

fn handle_list_command() -> Result<(), sc64::Error> {
    let devices = sc64::list_serial_devices()?;

    println!("{}", "Found devices:".bold());
    for (i, d) in devices.iter().enumerate() {
        println!(" {i}: {}", d.sn);
    }

    Ok(())
}
