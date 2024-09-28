mod debug;
mod disk;
mod n64;
mod sc64;

use chrono::Local;
use clap::{Args, Parser, Subcommand, ValueEnum};
use clap_num::{maybe_hex, maybe_hex_range};
use colored::Colorize;
use panic_message::panic_message;
use std::{
    fs::File,
    io::{stdin, stdout, Read, Write},
    panic,
    path::PathBuf,
    process,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
};

#[derive(Parser)]
#[command(author, version, about, long_about = None)]
struct Cli {
    #[command(subcommand)]
    command: Commands,

    /// Connect to SC64 device on provided local port
    #[arg(short, long)]
    port: Option<String>,

    /// Connect to SC64 device on provided remote address
    #[arg(short, long, conflicts_with = "port")]
    remote: Option<String>,
}

#[derive(Subcommand)]
enum Commands {
    /// List connected SC64 devices
    List,

    /// Upload ROM (and save) to the SC64
    Upload(UploadArgs),

    /// Download specific memory region and write it to file
    Download {
        #[command(subcommand)]
        command: DownloadCommands,
    },

    /// Upload ROM (and save), 64DD IPL then run disk/debug server
    _64DD(_64DDArgs),

    /// Enter debug mode
    Debug(DebugArgs),

    /// Dump data from arbitrary location in SC64 memory space
    Dump(DumpArgs),

    /// Perform operations on the SD card
    SD {
        #[command(subcommand)]
        command: SDCommands,
    },

    /// Print information about connected SC64 device
    Info,

    /// Reset SC64 state (same as after power-up)
    Reset,

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

    /// Test SC64 hardware
    Test,

    /// Expose SC64 device over network
    Server(ServerArgs),
}

#[derive(Args)]
struct UploadArgs {
    /// Path to the ROM file
    rom: PathBuf,

    /// Attempt to reboot the console (requires specific support in the running game)
    #[arg(short = 'a', long)]
    reboot: bool,

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

    /// Force TV type
    #[arg(long, conflicts_with = "direct")]
    tv: Option<TvType>,

    /// Force CIC seed
    #[arg(long, value_parser = |s: &str| maybe_hex::<u8>(s))]
    cic_seed: Option<u8>,
}

#[derive(Subcommand)]
enum DownloadCommands {
    /// Download save and write it to file
    Save(DownloadArgs),
}

#[derive(Args)]
struct DownloadArgs {
    /// Path to the file
    path: PathBuf,
}

#[derive(Args)]
struct _64DDArgs {
    /// Path to the 64DD IPL file
    ddipl: PathBuf,

    /// Path to the 64DD disk file (.ndd format, can be specified multiple times)
    disk: Vec<PathBuf>,

    /// Path to the ROM file
    #[arg(short, long)]
    rom: Option<PathBuf>,

    /// Attempt to reboot the console (requires specific support in the running game)
    #[arg(short = 'a', long)]
    reboot: bool,

    /// Path to the save file (also used by save writeback mechanism)
    #[arg(short, long, requires = "rom")]
    save: Option<PathBuf>,

    /// Override autodetected save type
    #[arg(short = 't', long, requires = "rom")]
    save_type: Option<SaveType>,

    /// Use direct boot mode (skip bootloader)
    #[arg(short, long)]
    direct: bool,

    /// Force TV type
    #[arg(long, conflicts_with = "direct")]
    tv: Option<TvType>,

    /// Force CIC seed
    #[arg(long, value_parser = |s: &str| maybe_hex::<u8>(s))]
    cic_seed: Option<u8>,
}

#[derive(Args)]
struct DebugArgs {
    /// Path to the save file to use by the save writeback mechanism
    #[arg(short, long)]
    save: Option<PathBuf>,

    /// Enable IS-Viewer64 and set listening address at ROM offset (in most cases it's fixed at 0x03FF0000)
    #[arg(long, value_name = "offset", value_parser = |s: &str| maybe_hex_range::<u32>(s, 0x00000004, 0x03FF0000))]
    isv: Option<u32>,

    /// Use EUC-JP encoding for text printing
    #[arg(long)]
    euc_jp: bool,

    /// Do not enable save writeback via USB
    #[arg(long)]
    no_writeback: bool,

    /// List of commands to send after connecting to the SC64, semicolon separated (;)
    #[arg(long)]
    init: Option<String>,
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
enum SDCommands {
    /// List a directory on the SD card
    #[command(name = "ls")]
    List {
        /// Path to the directory
        path: Option<PathBuf>,
    },

    /// Display a file or directory status
    #[command(name = "stat")]
    Stat {
        /// Path to the file or directory
        path: PathBuf,
    },

    /// Move or rename a file or directory
    #[command(name = "mv")]
    Move {
        /// Path to the current file or directory
        src: PathBuf,

        /// Path to the new file or directory
        dst: PathBuf,
    },

    /// Remove a file or empty directory
    #[command(name = "rm")]
    Delete {
        /// Path to the file or directory
        path: PathBuf,
    },

    /// Create a new directory
    #[command(name = "mkdir")]
    CreateDirectory {
        /// Path to the directory
        path: PathBuf,
    },

    /// Download a file to the PC
    #[command(name = "download")]
    Download {
        /// Path to the file on the SD card
        src: PathBuf,

        /// Path to the file on the PC
        dst: Option<PathBuf>,
    },

    /// Upload a file to the SD card
    #[command(name = "upload")]
    Upload {
        /// Path to the file on the PC
        src: PathBuf,

        /// Path to the file on the SD card
        dst: Option<PathBuf>,
    },

    /// Format the SD card
    #[command(name = "mkfs")]
    Format,
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
    Update(FirmwareUpdateArgs),
}

#[derive(Args)]
struct FirmwareArgs {
    /// Path to the firmware file
    firmware: PathBuf,
}

#[derive(Args)]
struct FirmwareUpdateArgs {
    /// Path to the firmware file
    firmware: PathBuf,

    /// Put firmware update in the Flash memory instead of SDRAM
    #[arg(long)]
    use_flash_memory: bool,
}

#[derive(Args)]
struct ServerArgs {
    /// Listen on provided address:port
    #[arg(default_value = "127.0.0.1:9064")]
    address: String,
}

#[derive(Clone, ValueEnum)]
enum SaveType {
    None,
    Eeprom4k,
    Eeprom16k,
    Sram,
    SramBanked,
    Sram1m,
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
            n64::SaveType::Sram1m => Self::Sram1m,
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
            SaveType::Sram1m => Self::Sram1m,
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

enum Connection {
    Local(Option<String>),
    Remote(String),
}

fn main() {
    let cli = Cli::parse();

    #[cfg(not(debug_assertions))]
    {
        panic::set_hook(Box::new(|_| {}));
    }

    match panic::catch_unwind(|| handle_command(&cli.command, cli.port, cli.remote)) {
        Ok(_) => {}
        Err(payload) => {
            eprintln!("{}", panic_message(&payload).red());
            process::exit(1);
        }
    }
}

fn handle_command(command: &Commands, port: Option<String>, remote: Option<String>) {
    let connection = if let Some(remote) = remote {
        Connection::Remote(remote)
    } else {
        Connection::Local(port)
    };
    let result = match command {
        Commands::List => handle_list_command(),
        Commands::Upload(args) => handle_upload_command(connection, args),
        Commands::Download { command } => handle_download_command(connection, command),
        Commands::_64DD(args) => handle_64dd_command(connection, args),
        Commands::Debug(args) => handle_debug_command(connection, args),
        Commands::Dump(args) => handle_dump_command(connection, args),
        Commands::SD { command } => handle_sd_command(connection, command),
        Commands::Info => handle_info_command(connection),
        Commands::Reset => handle_reset_command(connection),
        Commands::Set { command } => handle_set_command(connection, command),
        Commands::Firmware { command } => handle_firmware_command(connection, command),
        Commands::Test => handle_test_command(connection),
        Commands::Server(args) => handle_server_command(connection, args),
    };
    match result {
        Ok(()) => {}
        Err(error) => panic!("{error}"),
    };
}

fn handle_list_command() -> Result<(), sc64::Error> {
    let devices = sc64::list_local_devices()?;

    println!("{}", "Found devices:".bold());
    for (i, d) in devices.iter().enumerate() {
        let index = i + 1;
        println!(
            " {index}: [{}] at port [{}] (using \"{}\" backend)",
            d.serial.bold(),
            d.port.bold(),
            d.backend.to_string().bold()
        );
    }

    Ok(())
}

fn handle_upload_command(connection: Connection, args: &UploadArgs) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(connection, true)?;

    if args.reboot && !sc64.try_notify_via_aux(sc64::AuxMessage::Halt)? {
        println!(
            "{}",
            "Warning: no response for [Halt] AUX message".bright_yellow()
        );
    }

    sc64.reset_state()?;

    let (mut rom_file, rom_name, rom_length) = open_file(&args.rom)?;

    log_wait(format!("Uploading ROM [{rom_name}]"), || {
        sc64.upload_rom(&mut rom_file, rom_length, args.no_shadow)
    })?;

    let save: SaveType = if let Some(save_type) = args.save_type.clone() {
        save_type
    } else {
        let (save_type, title) = n64::guess_save_type(&mut rom_file)?;
        if let Some(title) = title {
            println!("ROM title: {title}");
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
        let tv_type: sc64::TvType = tv.into();
        println!("TV type set to [{tv_type}]");
        sc64.set_tv_type(tv_type)?;
    }

    sc64.calculate_cic_parameters(args.cic_seed)?;

    if args.reboot && !sc64.try_notify_via_aux(sc64::AuxMessage::Reboot)? {
        println!(
            "{}",
            "Warning: no response for [Reboot] AUX message".bright_yellow()
        );
    }

    Ok(())
}

fn handle_download_command(
    connection: Connection,
    command: &DownloadCommands,
) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(connection, true)?;

    match command {
        DownloadCommands::Save(args) => {
            let (mut file, name) = create_file(&args.path)?;

            log_wait(format!("Downloading save [{name}]"), || {
                sc64.download_save(&mut file)
            })?;
        }
    }

    Ok(())
}

fn handle_64dd_command(connection: Connection, args: &_64DDArgs) -> Result<(), sc64::Error> {
    const MAX_ROM_LENGTH: usize = 32 * 1024 * 1024;

    let mut sc64 = init_sc64(connection, true)?;

    let mut debug_handler = debug::Handler::new();

    println!(
        "{}\n{}\n{}\n{}",
        "========== [WARNING] ==========".bold().bright_yellow(),
        "Do not use this mode when real 64DD accessory is connected to the N64".bright_yellow(),
        "Doing so might permanently damage either N64, 64DD or SC64".bright_yellow(),
        "\"Only 64DD IPL\" mode should be safe on development units without IPL builtin"
            .bright_green()
    );

    if args.reboot && !sc64.try_notify_via_aux(sc64::AuxMessage::Halt)? {
        println!(
            "{}",
            "Warning: no response for [Halt] AUX message".bright_yellow()
        );
    }

    sc64.reset_state()?;

    if let Some(rom) = &args.rom {
        let (mut rom_file, rom_name, rom_length) = open_file(rom)?;
        if rom_length > MAX_ROM_LENGTH {
            return Err(sc64::Error::new("ROM file size too big for 64DD mode"));
        }
        log_wait(format!("Uploading ROM [{rom_name}]"), || {
            sc64.upload_rom(&mut rom_file, rom_length, false)
        })?;

        let save: SaveType = if let Some(save_type) = args.save_type.clone() {
            save_type
        } else {
            let (save_type, title) = n64::guess_save_type(&mut rom_file)?;
            if let Some(title) = title {
                println!("ROM title: {title}");
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
    }

    let (mut ddipl_file, ddipl_name, ddipl_length) = open_file(&args.ddipl)?;

    log_wait(format!("Uploading DDIPL [{ddipl_name}]"), || {
        sc64.upload_ddipl(&mut ddipl_file, ddipl_length)
    })?;

    let boot_mode = if args.rom.is_some() {
        if args.direct {
            sc64::BootMode::DirectRom
        } else {
            sc64::BootMode::Rom
        }
    } else {
        if args.direct {
            sc64::BootMode::DirectDdIpl
        } else {
            sc64::BootMode::DdIpl
        }
    };
    println!("Boot mode set to [{boot_mode}]");
    sc64.set_boot_mode(boot_mode)?;

    if let Some(tv) = args.tv.clone() {
        let tv_type: sc64::TvType = tv.into();
        println!("TV type set to [{tv_type}]");
        sc64.set_tv_type(tv_type)?;
    }

    sc64.calculate_cic_parameters(args.cic_seed)?;

    if args.disk.len() == 0 {
        let dd_mode = sc64::DdMode::DdIpl;
        println!("64DD mode set to [{dd_mode}]");
        sc64.configure_64dd(dd_mode, None)?;
        return Ok(());
    }

    let disk_paths: Vec<String> = args
        .disk
        .iter()
        .map(|path| path.to_string_lossy().to_string())
        .collect();
    let disk_names: Vec<String> = args
        .disk
        .iter()
        .map(|path| path.file_name().unwrap().to_string_lossy().to_string())
        .collect();
    let mut disks = disk::open_multiple(&disk_paths)?;

    let drive_type = match disks[0].get_format() {
        disk::Format::Retail => sc64::DdDriveType::Retail,
        disk::Format::Development => sc64::DdDriveType::Development,
    };

    let dd_mode = sc64::DdMode::Full;
    println!("64DD mode set to [{dd_mode} / {drive_type}]");
    sc64.configure_64dd(dd_mode, Some(drive_type))?;

    println!(
        "{}: {}",
        "[64DD]".bold(),
        "Press button on the back of SC64 device to cycle through provided disks"
            .bold()
            .bright_green()
    );

    let mut selected_disk_index: usize = 0;
    let mut selected_disk = Some(&mut disks[selected_disk_index]);
    println!(
        "{}: Disk inserted [{}]",
        "[64DD]".bold(),
        disk_names[selected_disk_index].bright_green()
    );
    sc64.set_64dd_disk_state(sc64::DdDiskState::Inserted)?;

    sc64.set_save_writeback(true)?;

    if args.reboot && !sc64.try_notify_via_aux(sc64::AuxMessage::Reboot)? {
        println!(
            "{}",
            "Warning: no response for [Reboot] AUX message".bright_yellow()
        );
    }

    let exit = setup_exit_flag();
    while !exit.load(Ordering::Relaxed) {
        if let Some(data_packet) = sc64.receive_data_packet()? {
            match data_packet {
                sc64::DataPacket::DiskRequest(mut disk_packet) => {
                    let track = disk_packet.info.track;
                    let head = disk_packet.info.head;
                    let block = disk_packet.info.block;
                    if let Some(ref mut disk) = selected_disk {
                        let (reply_packet, rw) = match disk_packet.kind {
                            sc64::DiskPacketKind::Read => (
                                disk.read_block(track, head, block)?.map(|data| {
                                    disk_packet.info.set_data(&data);
                                    disk_packet
                                }),
                                "[R]".bright_blue(),
                            ),
                            sc64::DiskPacketKind::Write => (
                                disk.write_block(track, head, block, &disk_packet.info.data)?
                                    .map(|_| disk_packet),
                                "[W]".bright_yellow(),
                            ),
                        };
                        let lba = if let Some(lba) = disk.get_lba(track, head, block) {
                            format!("{lba}")
                        } else {
                            "Invalid".to_string()
                        };
                        let message = format!("{track:4}:{head}:{block} | LBA: {lba}");
                        if reply_packet.is_some() {
                            println!("{}: {} {}", "[64DD]".bold(), rw, message.green());
                        } else {
                            println!("{}: {} {}", "[64DD]".bold(), rw, message.red());
                        }
                        sc64.reply_disk_packet(reply_packet)?;
                    } else {
                        sc64.reply_disk_packet(None)?;
                    }
                }
                sc64::DataPacket::Button => {
                    if selected_disk.is_some() {
                        sc64.set_64dd_disk_state(sc64::DdDiskState::Ejected)?;
                        selected_disk = None;
                        println!(
                            "{}: Disk ejected [{}]",
                            "[64DD]".bold(),
                            disk_names[selected_disk_index].green()
                        );
                    } else {
                        selected_disk_index += 1;
                        if selected_disk_index >= disks.len() {
                            selected_disk_index = 0;
                        }
                        selected_disk = Some(&mut disks[selected_disk_index]);
                        println!(
                            "{}: Disk inserted [{}]",
                            "[64DD]".bold(),
                            disk_names[selected_disk_index].bright_green()
                        );
                        sc64.set_64dd_disk_state(sc64::DdDiskState::Inserted)?;
                    }
                }
                sc64::DataPacket::DebugData(debug_packet) => {
                    debug_handler.handle_debug_packet(debug_packet);
                }
                sc64::DataPacket::SaveWriteback(save_writeback) => {
                    debug_handler.handle_save_writeback(save_writeback, &args.save);
                }
                sc64::DataPacket::DataFlushed => {
                    debug_handler.handle_data_flushed();
                }
                _ => {}
            }
        } else if let Some(user_input) = debug_handler.process_user_input() {
            match user_input {
                debug::UserInput::Packet(debug_packet) => sc64.send_debug_packet(debug_packet)?,
                debug::UserInput::EOF => break,
            }
        }
    }

    sc64.reset_state()?;

    Ok(())
}

fn handle_debug_command(connection: Connection, args: &DebugArgs) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(connection, true)?;

    let mut debug_handler = debug::Handler::new();

    if args.euc_jp {
        debug_handler.set_text_encoding(debug::Encoding::EUCJP);
    }

    if args.isv.is_some() {
        sc64.configure_is_viewer_64(args.isv)?;
        println!(
            "{}: Listening on ROM offset [{}]",
            "[IS-Viewer 64]".bold(),
            format!("0x{:08X}", args.isv.unwrap())
                .to_string()
                .bright_blue()
        );
    }
    if !args.no_writeback {
        sc64.set_save_writeback(true)?;
    }

    println!("{}: Started", "[Debug]".bold());

    if let Some(init) = args.init.clone() {
        for command in init.split(";") {
            println!("{}: {}", "[Init]".bold(), command);
            debug_handler.send_external_input(command);
        }
    }

    let exit = setup_exit_flag();
    while !exit.load(Ordering::Relaxed) {
        if let Some(data_packet) = sc64.receive_data_packet()? {
            match data_packet {
                sc64::DataPacket::DebugData(debug_packet) => {
                    debug_handler.handle_debug_packet(debug_packet);
                }
                sc64::DataPacket::IsViewer64(message) => {
                    debug_handler.handle_is_viewer_64(&message);
                }
                sc64::DataPacket::SaveWriteback(save_writeback) => {
                    debug_handler.handle_save_writeback(save_writeback, &args.save);
                }
                sc64::DataPacket::DataFlushed => {
                    debug_handler.handle_data_flushed();
                }
                _ => {}
            }
        } else if let Some(user_input) = debug_handler.process_user_input() {
            match user_input {
                debug::UserInput::Packet(debug_packet) => sc64.send_debug_packet(debug_packet)?,
                debug::UserInput::EOF => break,
            }
        }
    }

    if !args.no_writeback {
        sc64.set_save_writeback(false)?;
    }
    if args.isv.is_some() {
        sc64.configure_is_viewer_64(None)?;
        println!("{}: Stopped listening", "[IS-Viewer 64]".bold());
    }

    println!("{}: Stopped", "[Debug]".bold());

    Ok(())
}

fn handle_dump_command(connection: Connection, args: &DumpArgs) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(connection, true)?;

    let (mut dump_file, dump_name) = create_file(&args.path)?;

    log_wait(
        format!(
            "Dumping from [0x{:08X}] length [0x{:X}] to [{dump_name}]",
            args.address, args.length
        ),
        || sc64.dump_memory(&mut dump_file, args.address, args.length),
    )?;

    Ok(())
}

fn handle_sd_command(connection: Connection, command: &SDCommands) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(connection, true)?;

    match sc64.init_sd_card()? {
        sc64::SdCardResult::OK => {}
        error => {
            return Err(sc64::Error::new(
                format!("Couldn't init the SD card: {error}").as_str(),
            ))
        }
    }

    if sc64.is_console_powered_on()? {
        println!(
            "{}\n{}\n{}",
            "========== [WARNING] ==========".bold().bright_yellow(),
            "The console is powered on. To avoid potential data corruption it's strongly"
                .bright_yellow(),
            "recommended to access the SD card only when the console is powered off."
                .bright_yellow()
        );
        let answer = prompt(format!("{}", "Continue anyways? [y/N] ".bold()));
        if answer.to_ascii_lowercase() != "y" {
            sc64.deinit_sd_card()?;
            println!("{}", "SD card access aborted".red());
            return Ok(());
        }
    }

    sc64.reset_state()?;

    sc64::ff::run(sc64, |ff| {
        match command {
            SDCommands::List { path } => {
                for item in ff.list(path.clone().unwrap_or(PathBuf::from("/")))? {
                    let sc64::ff::Entry {
                        info,
                        datetime,
                        name,
                    } = item;
                    let name = match info {
                        sc64::ff::EntryInfo::Directory => ("/".to_owned() + &name).bright_blue(),
                        sc64::ff::EntryInfo::File { size: _ } => name.bright_green(),
                    };
                    println!("{info} {datetime} | {}", name.bold());
                }
            }
            SDCommands::Stat { path } => {
                let sc64::ff::Entry {
                    info,
                    datetime,
                    name,
                } = ff.stat(path)?;
                let name = match info {
                    sc64::ff::EntryInfo::Directory => ("/".to_owned() + &name).bright_blue(),
                    sc64::ff::EntryInfo::File { size: _ } => name.bright_green(),
                };
                println!("{info} {datetime} | {}", name.bold());
            }
            SDCommands::Move { src, dst } => {
                ff.rename(src, dst)?;
                println!(
                    "Successfully moved {} to {}",
                    src.to_str().unwrap_or_default().bright_green(),
                    dst.to_str().unwrap_or_default().bright_green()
                );
            }
            SDCommands::Delete { path } => {
                ff.delete(path)?;
                println!(
                    "Successfully deleted {}",
                    path.to_str().unwrap_or_default().bright_green()
                );
            }
            SDCommands::CreateDirectory { path } => {
                ff.mkdir(path)?;
                println!(
                    "Successfully created {}",
                    path.to_str().unwrap_or_default().bright_green()
                );
            }
            SDCommands::Download { src, dst } => {
                let dst = &dst.clone().unwrap_or(
                    src.file_name()
                        .map(PathBuf::from)
                        .ok_or(sc64::ff::Error::InvalidParameter)?,
                );
                let mut src_file = ff.open(src)?;
                let mut dst_file = std::fs::File::create(dst)?;
                let mut buffer = vec![0; 128 * 1024];
                print!(
                    "{}",
                    format!(
                        "Downloading {} to {}... ",
                        src.to_str().unwrap_or_default().bright_green(),
                        dst.to_str().unwrap_or_default().bright_green()
                    )
                );
                stdout().flush().unwrap();
                loop {
                    match src_file.read(&mut buffer)? {
                        0 => break,
                        bytes => dst_file.write_all(&buffer[0..bytes])?,
                    }
                }
                println!("{}", "done!".bright_green());
            }
            SDCommands::Upload { src, dst } => {
                let dst = &dst.clone().unwrap_or(
                    src.file_name()
                        .map(PathBuf::from)
                        .ok_or(sc64::ff::Error::InvalidParameter)?,
                );
                let mut src_file = std::fs::File::open(src)?;
                let mut dst_file = ff.create(dst)?;
                let mut buffer = vec![0; 128 * 1024];
                print!(
                    "{}",
                    format!(
                        "Uploading {} to {}... ",
                        src.to_str().unwrap_or_default().bright_green(),
                        dst.to_str().unwrap_or_default().bright_green()
                    )
                );
                stdout().flush().unwrap();
                loop {
                    match src_file.read(&mut buffer)? {
                        0 => break,
                        bytes => dst_file.write_all(&buffer[0..bytes])?,
                    }
                }
                println!("{}", "done!".bright_green());
            }
            SDCommands::Format => {
                let answer = prompt(format!(
                    "{}",
                    "Do you really want to format the SD card? [y/N] ".bold()
                ));
                if answer.to_ascii_lowercase() != "y" {
                    println!("{}", "Format operation aborted".red());
                    return Ok(());
                }
                print!("Formatting the SD card... ",);
                stdout().flush().unwrap();
                ff.mkfs()?;
                println!("{}", "done!".bright_green());
            }
        }

        Ok(())
    })?;

    Ok(())
}

fn handle_info_command(connection: Connection) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(connection, true)?;

    let (major, minor, revision) = sc64.check_firmware_version()?;
    let state = sc64.get_device_state()?;

    println!("{}", "SummerCart64 state information:".bold());
    println!(" Firmware version:  v{}.{}.{}", major, minor, revision);
    println!(" RTC datetime:      {}", state.datetime);
    println!(" Boot mode:         {}", state.boot_mode);
    println!(" Save type:         {}", state.save_type);
    println!(" CIC seed:          {}", state.cic_seed);
    println!(" TV type:           {}", state.tv_type);
    println!(" Bootloader switch: {}", state.bootloader_switch);
    println!(" ROM write:         {}", state.rom_write_enable);
    println!(" ROM shadow:        {}", state.rom_shadow_enable);
    println!(" ROM extended:      {}", state.rom_extended_enable);
    println!(" 64DD mode:         {}", state.dd_mode);
    println!(" 64DD SD card mode: {}", state.dd_sd_enable);
    println!(" 64DD drive type:   {}", state.dd_drive_type);
    println!(" 64DD disk state:   {}", state.dd_disk_state);
    println!(" Button mode:       {}", state.button_mode);
    println!(" Button state:      {}", state.button_state);
    println!(" LED blink:         {}", state.led_enable);
    println!(" IS-Viewer 64:      {}", state.isviewer);
    println!(" SD card status:    {}", state.sd_card_status);
    println!("{}", "SummerCart64 diagnostic information:".bold());
    println!(" PI I/O access:     {}", state.fpga_debug_data.pi_io_access);
    println!(
        " PI FIFO flags:     {}",
        state.fpga_debug_data.pi_fifo_flags
    );
    println!(" Current CIC step:  {}", state.fpga_debug_data.cic_step);
    println!(" Diagnostic data:   {}", state.diagnostic_data);

    Ok(())
}

fn handle_reset_command(connection: Connection) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(connection, true)?;

    sc64.reset_state()?;

    println!("SC64 state has been reset");

    Ok(())
}

fn handle_set_command(connection: Connection, command: &SetCommands) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(connection, true)?;

    match command {
        SetCommands::Rtc => {
            let datetime = Local::now().naive_local();
            sc64.set_datetime(datetime)?;
            println!(
                "SC64 RTC datetime synchronized to: {}",
                datetime.format("%Y-%m-%d %H:%M:%S").to_string().green()
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
    connection: Connection,
    command: &FirmwareCommands,
) -> Result<(), sc64::Error> {
    match command {
        FirmwareCommands::Info(args) => {
            let (mut firmware_file, _, firmware_length) = open_file(&args.firmware)?;

            let mut firmware = vec![0u8; firmware_length as usize];
            firmware_file.read_exact(&mut firmware)?;

            let metadata = sc64::firmware::verify(&firmware)?;
            println!("{}", "Firmware metadata:".bold());
            println!("{}", format!("{}", metadata).bright_blue().to_string());

            Ok(())
        }

        FirmwareCommands::Backup(args) => {
            let mut sc64 = init_sc64(connection, false)?;

            let (mut backup_file, backup_name) = create_file(&args.firmware)?;

            let firmware = log_wait(
                format!("Generating firmware backup, this might take a while [{backup_name}]"),
                || sc64.backup_firmware(),
            )?;

            let metadata = sc64::firmware::verify(&firmware)?;
            println!("{}", "Firmware metadata:".bold());
            println!("{}", format!("{}", metadata).bright_blue().to_string());

            backup_file.write_all(&firmware)?;

            Ok(())
        }

        FirmwareCommands::Update(args) => {
            let mut sc64 = init_sc64(connection, false)?;

            let (mut update_file, update_name, update_length) = open_file(&args.firmware)?;

            let mut firmware = vec![0u8; update_length as usize];
            update_file.read_exact(&mut firmware)?;

            let metadata = sc64::firmware::verify(&firmware)?;
            println!("{}", "Firmware metadata:".bold());
            println!("{}", format!("{}", metadata).bright_blue().to_string());
            println!("{}", "Firmware file verification was successful".green());
            if args.use_flash_memory {
                println!(
                    "{}",
                    "Warning: using Flash memory to perform firmware update".yellow()
                );
            }
            let answer = prompt(format!("{}", "Continue with update process? [y/N] ".bold()));
            if answer.to_ascii_lowercase() != "y" {
                println!("{}", "Firmware update process aborted".red());
                return Ok(());
            }
            println!(
                "{}",
                "Do not unplug SC64 from the computer, doing so might brick your device".yellow()
            );

            log_wait(
                format!("Updating firmware, this might take a while [{update_name}]"),
                || sc64.update_firmware(&firmware, args.use_flash_memory),
            )?;

            Ok(())
        }
    }
}

fn handle_test_command(connection: Connection) -> Result<(), sc64::Error> {
    let mut sc64 = init_sc64(connection, true)?;

    sc64.reset_state()?;

    println!("{}: USB", "[SC64 Tests]".bold());

    print!(" Performing USB read speed test... ");
    stdout().flush().unwrap();
    let usb_read_speed = sc64.test_usb_speed(sc64::SpeedTestDirection::Read)?;
    println!("{}", format!("{usb_read_speed:.2} MiB/s",).bright_green());

    print!(" Performing USB write speed test... ");
    stdout().flush().unwrap();
    let usb_write_speed = sc64.test_usb_speed(sc64::SpeedTestDirection::Write)?;
    println!("{}", format!("{usb_write_speed:.2} MiB/s",).bright_green());

    println!("{}: SD card", "[SC64 Tests]".bold());

    print!(" Performing SD card read speed test... ");
    stdout().flush().unwrap();
    match sc64.test_sd_card() {
        Ok(sd_read_speed) => println!("{}", format!("{sd_read_speed:.2} MiB/s",).bright_green()),
        Err(result) => println!("{}", format!("error! {result}").bright_red()),
    }

    println!("{}: SDRAM (pattern)", "[SC64 Tests]".bold());

    let sdram_pattern_tests = [
        (sc64::MemoryTestPattern::OwnAddress(false), None),
        (sc64::MemoryTestPattern::OwnAddress(true), None),
        (sc64::MemoryTestPattern::AllZeros, None),
        (sc64::MemoryTestPattern::AllOnes, None),
        (sc64::MemoryTestPattern::Custom(0xAAAA5555), None),
        (sc64::MemoryTestPattern::Custom(0x5555AAAA), None),
        (sc64::MemoryTestPattern::Random, None),
        (sc64::MemoryTestPattern::Random, None),
        (sc64::MemoryTestPattern::Random, None),
        (sc64::MemoryTestPattern::Random, None),
        (sc64::MemoryTestPattern::Custom(0x00010001), None),
        (sc64::MemoryTestPattern::Custom(0xFFFEFFFE), None),
        (sc64::MemoryTestPattern::Custom(0x00020002), None),
        (sc64::MemoryTestPattern::Custom(0xFFFDFFFD), None),
        (sc64::MemoryTestPattern::Custom(0x00040004), None),
        (sc64::MemoryTestPattern::Custom(0xFFFBFFFB), None),
        (sc64::MemoryTestPattern::Custom(0x00080008), None),
        (sc64::MemoryTestPattern::Custom(0xFFF7FFF7), None),
        (sc64::MemoryTestPattern::Custom(0x00100010), None),
        (sc64::MemoryTestPattern::Custom(0xFFEFFFEF), None),
        (sc64::MemoryTestPattern::Custom(0x00200020), None),
        (sc64::MemoryTestPattern::Custom(0xFFDFFFDF), None),
        (sc64::MemoryTestPattern::Custom(0x00400040), None),
        (sc64::MemoryTestPattern::Custom(0xFFBFFFBF), None),
        (sc64::MemoryTestPattern::Custom(0x00800080), None),
        (sc64::MemoryTestPattern::Custom(0xFF7FFF7F), None),
        (sc64::MemoryTestPattern::Custom(0x01000100), None),
        (sc64::MemoryTestPattern::Custom(0xFEFFFEFF), None),
        (sc64::MemoryTestPattern::Custom(0x02000200), None),
        (sc64::MemoryTestPattern::Custom(0xFDFFFDFF), None),
        (sc64::MemoryTestPattern::Custom(0x04000400), None),
        (sc64::MemoryTestPattern::Custom(0xFBFFFBFF), None),
        (sc64::MemoryTestPattern::Custom(0x08000800), None),
        (sc64::MemoryTestPattern::Custom(0xF7FFF7FF), None),
        (sc64::MemoryTestPattern::Custom(0x10001000), None),
        (sc64::MemoryTestPattern::Custom(0xEFFFEFFF), None),
        (sc64::MemoryTestPattern::Custom(0x20002000), None),
        (sc64::MemoryTestPattern::Custom(0xDFFFDFFF), None),
        (sc64::MemoryTestPattern::Custom(0x40004000), None),
        (sc64::MemoryTestPattern::Custom(0xBFFFBFFF), None),
        (sc64::MemoryTestPattern::Custom(0x80008000), None),
        (sc64::MemoryTestPattern::Custom(0x7FFF7FFF), None),
        (sc64::MemoryTestPattern::AllZeros, Some(60)),
        (sc64::MemoryTestPattern::AllOnes, Some(60)),
    ];
    let sdram_pattern_tests_count = sdram_pattern_tests.len();

    let mut sdram_tests_failed = false;

    for (i, (pattern, fade)) in sdram_pattern_tests.into_iter().enumerate() {
        let fadeout_text = if let Some(fade) = fade {
            format!(", fadeout {fade} seconds")
        } else {
            "".to_string()
        };
        print!(
            " ({} / {sdram_pattern_tests_count}) Testing {pattern}{fadeout_text}... ",
            i + 1
        );
        stdout().flush().unwrap();

        let result = sc64.test_sdram_pattern(pattern, fade)?;

        if let Some((address, (written, read))) = result.first_error {
            sdram_tests_failed = true;
            println!("{}", "error!".bright_red());
            println!("  Found a mismatch at address 0x{address:08X}",);
            println!("   0x{written:08X} (W) != 0x{read:08X} (R)");
            println!("   Total errors found: {}", result.all_errors.len());
        } else {
            println!("{}", "ok".bright_green());
        }
    }

    if sdram_tests_failed {
        println!(
            "{}",
            "Some SDRAM tests failed, SDRAM chip might be defective".bright_red()
        );
    } else {
        println!("{}", "All SDRAM tests passed without error".bright_green());
    }

    Ok(())
}

fn handle_server_command(connection: Connection, args: &ServerArgs) -> Result<(), sc64::Error> {
    let port = if let Connection::Local(port) = connection {
        port
    } else {
        None
    };

    sc64::server::run(port, args.address.clone(), |event| match event {
        sc64::ServerEvent::Listening(address) => {
            println!(
                "{}: Listening on address [{}]",
                "[Server]".bold(),
                address.bright_blue()
            )
        }
        sc64::ServerEvent::Connected(peer) => {
            println!(
                "{}: New connection from [{}]",
                "[Server]".bold(),
                peer.bright_green()
            );
        }
        sc64::ServerEvent::Disconnected(peer) => {
            println!(
                "{}: Client disconnected [{}]",
                "[Server]".bold(),
                peer.green()
            );
        }
        sc64::ServerEvent::Err(error) => {
            println!(
                "{}: Client disconnected - server error: {}",
                "[Server]".bold(),
                error.red()
            );
        }
    })?;

    Ok(())
}

fn init_sc64(connection: Connection, check_firmware: bool) -> Result<sc64::SC64, sc64::Error> {
    let mut sc64 = match connection {
        Connection::Local(port) => sc64::SC64::open_local(port),
        Connection::Remote(remote) => sc64::SC64::open_remote(remote),
    }?;

    if check_firmware {
        sc64.check_firmware_version()?;
    }

    Ok(sc64)
}

fn log_wait<F: FnOnce() -> Result<T, E>, T, E>(message: String, operation: F) -> Result<T, E> {
    print!("{}... ", message);
    stdout().flush().unwrap();
    let result = operation();
    if result.is_ok() {
        println!("{}", "done".bold().bright_green());
    } else {
        println!("{}", "error!".bold().bright_red());
    }
    result
}

fn prompt(message: String) -> String {
    print!("{message}");
    stdout().flush().unwrap();
    let mut answer = String::new();
    stdin().read_line(&mut answer).unwrap();
    answer.trim_end().to_string()
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
