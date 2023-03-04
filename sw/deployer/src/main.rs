mod debug;
mod n64;
pub mod sc64;

use chrono::Local;
use clap::{Args, Parser, Subcommand, ValueEnum};
use clap_num::maybe_hex_range;
use colored::Colorize;
use panic_message::panic_message;
use std::{
    fs::File,
    io::{stdout, BufReader, Read, Write},
    path::PathBuf,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
    time::Duration,
    {panic, process, thread},
};

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

#[derive(Clone, ValueEnum)]
enum SaveType {
    None,
    Eeprom4k,
    Eeprom16k,
    Sram,
    SramBanked,
    Flashram,
}

impl From<n64::SaveType> for SaveType {
    fn from(value: n64::SaveType) -> Self {
        match value {
            n64::SaveType::None => Self::None,
            n64::SaveType::Eeprom4k => Self::Eeprom4k,
            n64::SaveType::Eeprom16k => Self::Eeprom16k,
            n64::SaveType::Sram => Self::Sram,
            n64::SaveType::SramBanked => Self::SramBanked,
            n64::SaveType::Flashram => Self::Flashram,
        }
    }
}

impl From<SaveType> for sc64::SaveType {
    fn from(value: SaveType) -> Self {
        match value {
            SaveType::None => Self::None,
            SaveType::Eeprom4k => Self::Eeprom4k,
            SaveType::Eeprom16k => Self::Eeprom16k,
            SaveType::Sram => Self::Sram,
            SaveType::SramBanked => Self::SramBanked,
            SaveType::Flashram => Self::Flashram,
        }
    }
}

#[derive(Clone, ValueEnum)]
enum TvType {
    PAL,
    NTSC,
    MPAL,
}

impl From<TvType> for sc64::TvType {
    fn from(value: TvType) -> Self {
        match value {
            TvType::PAL => Self::PAL,
            TvType::NTSC => Self::NTSC,
            TvType::MPAL => Self::MPAL,
        }
    }
}

fn main() {
    let cli = Cli::parse();

    #[cfg(not(debug_assertions))]
    {
        panic::set_hook(Box::new(|_| {}));
    }

    match panic::catch_unwind(|| handle_command(&cli.command, cli.sn)) {
        Ok(_) => {}
        Err(payload) => {
            eprintln!("{}", panic_message(&payload).red());
            process::exit(1);
        }
    }
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

    let (rom_file_unbuffered, rom_name, rom_length) = open_file(&args.rom)?;
    let mut rom_file = BufReader::new(rom_file_unbuffered);

    log_wait(format!("Uploading ROM [{rom_name}]"), || {
        sc64.upload_rom(&mut rom_file, rom_length, args.no_shadow)
    })?;

    let save: SaveType = if let Some(save_type) = args.save_type.clone() {
        save_type
    } else {
        let (save_type, name) = n64::guess_save_type(&mut rom_file)?;
        if let Some(name) = name {
            println!("Detected ROM name: {name}");
        };
        save_type.into()
    };

    let save_type: sc64::SaveType = save.into();
    println!("Save type set to [{save_type}]");
    sc64.set_save_type(save_type)?;

    if args.save.is_some() {
        let (mut save_file, save_name, save_length) = open_file(&args.save.as_ref().unwrap())?;

        log_wait(format!("Uploading save [{save_name}]"), || {
            sc64.upload_save(&mut save_file, save_length)
        })?;
    }

    let boot_mode = if args.direct {
        sc64::BootMode::DirectRom
    } else {
        sc64::BootMode::Rom
    };
    println!("Boot mode set to [{boot_mode}]");
    sc64.set_boot_mode(boot_mode)?;

    if let Some(tv) = args.tv.clone() {
        if args.direct {
            println!("TV type ignored due to direct boot mode being enabled");
        } else {
            let tv_type: sc64::TvType = tv.into();
            println!("TV type set to [{tv_type}]");
            sc64.set_tv_type(tv_type)?;
        }
    }

    sc64.calculate_cic_parameters()?;

    Ok(())
}

fn handle_64dd_command(sn: Option<String>, args: &_64DDArgs) -> Result<(), sc64::Error> {
    let _ = (sn, args);

    // TODO: handle 64DD stuff

    println!("{}", "Sorry nothing".yellow());

    Ok(())
}

fn handle_dump_command(sn: Option<String>, args: &DumpArgs) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(sn, true)?;

    let (mut dump_file, dump_name) = create_file(&args.path)?;

    let data = log_wait(
        format!(
            "Dumping from [0x{:08X}] length [0x{:X}] to [{dump_name}]",
            args.address, args.length
        ),
        || sc64.dump_memory(args.address, args.length),
    )?;

    dump_file.write(&data)?;

    Ok(())
}

fn handle_debug_command(sn: Option<String>, args: &DebugArgs) -> Result<(), sc64::Error> {
    let mut debug_handler = debug::new(args.gdb)?;
    if let Some(port) = args.gdb {
        println!("GDB TCP socket listening at [0.0.0.0:{port}]");
    }

    let mut sc64 = init_sc64(sn, true)?;

    if args.isv.is_some() {
        sc64.configure_is_viewer_64(args.isv)?;
        println!(
            "IS-Viewer configured and listening at offset [0x{:08X}]",
            args.isv.unwrap()
        );
    }

    println!("{}", "Debug mode started".bold());

    let exit = setup_exit_flag();
    while !exit.load(Ordering::Relaxed) {
        if let Some(data_packet) = sc64.receive_data_packet()? {
            match data_packet {
                sc64::DataPacket::IsViewer(message) => {
                    print!("{message}")
                }
                sc64::DataPacket::Debug(debug_packet) => {
                    debug_handler.handle_debug_packet(debug_packet)
                }
                _ => {}
            }
        } else if let Some(gdb_packet) = debug_handler.receive_gdb_packet() {
            sc64.send_debug_packet(gdb_packet)?;
        } else {
            // TODO: handle user input
            thread::sleep(Duration::from_millis(1));
        }
    }

    println!("{}", "Debug mode ended".bold());

    if args.isv.is_some() {
        sc64.configure_is_viewer_64(None)?;
    }

    Ok(())
}

fn handle_info_command(sn: Option<String>) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(sn, false)?;

    let (major, minor) = sc64.check_firmware_version()?;
    let state = sc64.get_device_state()?;
    let datetime = state.datetime.format("%Y-%m-%d %H:%M:%S %Z");

    println!("{}", "SC64 information and current state:".bold());
    println!(" Firmware version:    v{}.{}", major, minor);
    println!(" RTC datetime:        {}", datetime);
    println!(" Boot mode:           {}", state.boot_mode);
    println!(" Save type:           {}", state.save_type);
    println!(" CIC seed:            {}", state.cic_seed);
    println!(" TV type:             {}", state.tv_type);
    println!(" Bootloader switch:   {}", state.bootloader_switch);
    println!(" ROM write:           {}", state.rom_write_enable);
    println!(" ROM shadow:          {}", state.rom_shadow_enable);
    println!(" ROM extended:        {}", state.rom_extended_enable);
    println!(" IS-Viewer 64 offset: 0x{:08X}", state.isv_address);
    println!(" 64DD mode:           {}", state.dd_mode);
    println!(" 64DD SD card mode:   {}", state.dd_sd_enable);
    println!(" 64DD drive type:     {}", state.dd_drive_type);
    println!(" 64DD disk state:     {}", state.dd_disk_state);
    println!(" Button mode:         {}", state.button_mode);
    println!(" Button state:        {}", state.button_state);
    println!(" LED blink:           {}", state.led_enable);
    println!(" FPGA debug data:     {}", state.fpga_debug_data);
    println!(" MCU stack usage:     {}", state.mcu_stack_usage);

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
            let (mut firmware_file, _, firmware_length) = open_file(&args.firmware)?;

            let mut firmware = vec![0u8; firmware_length as usize];
            firmware_file.read_exact(&mut firmware)?;

            let metadata = sc64::firmware::verify(&firmware)?;
            println!("{}", "Firmware metadata:".bold());
            println!("{}", metadata);

            Ok(())
        }

        FirmwareCommands::Backup(args) => {
            let mut sc64 = init_sc64(sn, false)?;

            let (mut backup_file, backup_name) = create_file(&args.firmware)?;

            sc64.reset_state()?;

            let firmware = log_wait(
                format!("Generating firmware backup, this might take a while [{backup_name}]"),
                || sc64.backup_firmware(),
            )?;

            let metadata = sc64::firmware::verify(&firmware)?;
            println!("{}", "Firmware metadata:".bold());
            println!("{}", metadata);

            backup_file.write(&firmware)?;

            Ok(())
        }

        FirmwareCommands::Update(args) => {
            let mut sc64 = init_sc64(sn, false)?;

            let (mut update_file, update_name, update_length) = open_file(&args.firmware)?;

            let mut firmware = vec![0u8; update_length as usize];
            update_file.read_exact(&mut firmware)?;

            let metadata = sc64::firmware::verify(&firmware)?;
            println!("{}", "Firmware metadata:".bold());
            println!("{}", metadata);

            sc64.reset_state()?;

            log_wait(
                format!("Updating firmware, this might take a while [{update_name}]"),
                || sc64.update_firmware(&firmware),
            )?;

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

fn init_sc64(sn: Option<String>, check_firmware: bool) -> Result<sc64::SC64, sc64::Error> {
    let mut sc64 = sc64::new(sn)?;

    if check_firmware {
        sc64.check_firmware_version()?;
    }

    Ok(sc64)
}

fn log_wait<F: FnOnce() -> Result<T, E>, T, E>(message: String, operation: F) -> Result<T, E> {
    print!("{}... ", message);
    stdout().flush().unwrap();
    let result = operation();
    println!("done");
    result
}

fn open_file(path: &PathBuf) -> Result<(File, String, usize), sc64::Error> {
    let name: String = path.file_name().unwrap().to_string_lossy().to_string();
    let file = File::open(path)?;
    let length = file.metadata()?.len() as usize;
    Ok((file, name, length))
}

fn create_file(path: &PathBuf) -> Result<(File, String), sc64::Error> {
    let name: String = path.file_name().unwrap().to_string_lossy().to_string();
    let file = File::create(path)?;
    Ok((file, name))
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
