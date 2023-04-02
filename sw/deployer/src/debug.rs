use crate::sc64;
use chrono::Local;
use colored::Colorize;
use encoding_rs::EUC_JP;
use panic_message::panic_message;
use std::{
    fs::File,
    io::{stdin, ErrorKind, Read, Write},
    net::{TcpListener, TcpStream},
    panic,
    path::PathBuf,
    sync::mpsc::{channel, Receiver, Sender},
    thread::{sleep, spawn},
    time::Duration,
};

pub struct Handler {
    header: Option<Vec<u8>>,
    line_rx: Receiver<String>,
    gdb_tx: Sender<Vec<u8>>,
    gdb_rx: Receiver<Vec<u8>>,
}

enum DataType {
    Text,
    RawBinary,
    Header,
    Screenshot,
    GDB,
    Unknown,
}

impl From<u8> for DataType {
    fn from(value: u8) -> Self {
        match value {
            0x01 => Self::Text,
            0x02 => Self::RawBinary,
            0x03 => Self::Header,
            0x04 => Self::Screenshot,
            0xDB => Self::GDB,
            _ => Self::Unknown,
        }
    }
}

impl From<DataType> for u8 {
    fn from(value: DataType) -> Self {
        match value {
            DataType::Text => 0x01,
            DataType::RawBinary => 0x02,
            DataType::Header => 0x03,
            DataType::Screenshot => 0x04,
            DataType::GDB => 0xDB,
            DataType::Unknown => 0xFF,
        }
    }
}

impl From<DataType> for u32 {
    fn from(value: DataType) -> Self {
        u8::from(value) as u32
    }
}

#[derive(Clone, Copy)]
enum ScreenshotPixelFormat {
    Rgba16,
    Rgba32,
}

impl TryFrom<u32> for ScreenshotPixelFormat {
    type Error = String;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            2 => Self::Rgba16,
            4 => Self::Rgba32,
            _ => return Err("Invalid pixel format for screenshot metadata".into()),
        })
    }
}

impl From<ScreenshotPixelFormat> for u32 {
    fn from(value: ScreenshotPixelFormat) -> Self {
        match value {
            ScreenshotPixelFormat::Rgba16 => 2,
            ScreenshotPixelFormat::Rgba32 => 4,
        }
    }
}

struct ScreenshotMetadata {
    format: ScreenshotPixelFormat,
    width: u32,
    height: u32,
}

impl TryFrom<Vec<u8>> for ScreenshotMetadata {
    type Error = String;
    fn try_from(value: Vec<u8>) -> Result<Self, Self::Error> {
        if value.len() != 16 {
            return Err("Invalid header length for screenshot metadata".into());
        }
        if u32::from_be_bytes(value[0..4].try_into().unwrap()) != DataType::Screenshot.into() {
            return Err("Invalid header datatype for screenshot metadata".into());
        }
        let format = u32::from_be_bytes(value[4..8].try_into().unwrap());
        let width = u32::from_be_bytes(value[8..12].try_into().unwrap());
        let height = u32::from_be_bytes(value[12..16].try_into().unwrap());
        if width > 4096 || height > 4096 {
            return Err("Invalid width or height for screenshot metadata".into());
        }
        Ok(ScreenshotMetadata {
            format: format.try_into()?,
            width,
            height,
        })
    }
}

macro_rules! success {
    ($($a: tt)*) => {
        println!("{}", format!($($a)*).bright_blue());
    };
}

macro_rules! error {
    ($($a: tt)*) => {{
        println!("{}", format!("Error: {}", format!($($a)*)).bright_red());
    }};
}

macro_rules! stop {
    ($r: expr, $($a: tt)*) => {{
        error!($($a)*);
        $r
    }};
}

const MAX_PACKET_LENGTH: usize = 8 * 1024 * 1024;

impl Handler {
    pub fn process_user_input(&self) -> Option<sc64::DebugPacket> {
        let line = match self.line_rx.try_recv() {
            Ok(line) => {
                if line.len() == 0 {
                    return None;
                } else {
                    line
                }
            }
            Err(_) => return None,
        };

        let token_count = line.matches("@").count();

        if (token_count % 2) != 0 {
            return stop!(None, "Missing closing '@' token");
        }

        let packet = if token_count == 2 && line.starts_with("@") && line.ends_with("@") {
            sc64::DebugPacket {
                datatype: DataType::RawBinary.into(),
                data: match load_file(line.trim_matches('@')) {
                    Ok(data) => data,
                    Err(error) => return stop!(None, "{error}"),
                },
            }
        } else {
            let mut is_text = true;
            let mut path = String::new();
            let mut character_buffer = vec![0u8; 4];
            let mut data = vec![0u8; 0];
            for character in line.chars() {
                if character == '@' {
                    if is_text {
                        is_text = false;
                    } else {
                        let mut file = match load_file(&path) {
                            Ok(data) => data,
                            Err(error) => return stop!(None, "{error}"),
                        };
                        let length = file.len();
                        data.append(&mut format!("@{length}@").into_bytes());
                        data.append(&mut file);
                        is_text = true;
                        path = String::new();
                    }
                } else {
                    if is_text {
                        let encoded = character.encode_utf8(&mut character_buffer);
                        data.append(&mut encoded.as_bytes().to_vec());
                    } else {
                        path.push(character);
                    }
                }
            }
            sc64::DebugPacket {
                datatype: DataType::Text.into(),
                data,
            }
        };

        if packet.data.len() > MAX_PACKET_LENGTH {
            return stop!(
                None,
                "Debug packet size too big ({}), exceeding maximum size of {}",
                packet.data.len(),
                MAX_PACKET_LENGTH
            );
        }

        Some(packet)
    }

    pub fn handle_debug_packet(&mut self, debug_packet: sc64::DebugPacket) {
        let sc64::DebugPacket { datatype, data } = debug_packet;
        match datatype.into() {
            DataType::Text => self.handle_datatype_text(&data),
            DataType::RawBinary => self.handle_datatype_raw_binary(&data),
            DataType::Header => self.handle_datatype_header(&data),
            DataType::Screenshot => self.handle_datatype_screenshot(&data),
            DataType::GDB => self.handle_datatype_gdb(&data),
            _ => error!("Received unknown debug packet datatype: 0x{datatype:02X}"),
        }
    }

    pub fn handle_is_viewer_64(&self, message: Vec<u8>) {
        print!("{}", EUC_JP.decode(&message).0)
    }

    pub fn handle_save_writeback(
        &self,
        save_writeback: sc64::SaveWriteback,
        path: &Option<PathBuf>,
    ) {
        let filename = &if let Some(path) = path {
            path.to_string_lossy().to_string()
        } else {
            generate_filename(
                "save",
                match save_writeback.save {
                    sc64::SaveType::Eeprom4k | sc64::SaveType::Eeprom16k => "eep",
                    sc64::SaveType::Sram | sc64::SaveType::SramBanked | sc64::SaveType::Sram1m => {
                        "srm"
                    }
                    sc64::SaveType::Flashram => "fla",
                    _ => "sav",
                },
            )
        };
        match File::create(filename) {
            Ok(mut file) => {
                if let Err(error) = file.write_all(&save_writeback.data) {
                    error!("Couldn't write save [{filename}]: {error}");
                }
                success!("Wrote [{}] save to [{filename}]", save_writeback.save);
            }
            Err(error) => error!("Couldn't create save writeback file [{filename}]: {error}"),
        }
    }

    pub fn receive_gdb_packet(&self) -> Option<sc64::DebugPacket> {
        if let Some(data) = self.gdb_rx.try_recv().ok() {
            Some(sc64::DebugPacket {
                datatype: DataType::GDB.into(),
                data,
            })
        } else {
            None
        }
    }

    fn handle_datatype_text(&self, data: &[u8]) {
        print!("{}", String::from_utf8_lossy(data));
    }

    fn handle_datatype_raw_binary(&self, data: &[u8]) {
        let filename = &generate_filename("binaryout", "bin");
        match File::create(filename) {
            Ok(mut file) => {
                if let Err(error) = file.write_all(data) {
                    error!("Couldn't write raw binary [{filename}]: {error}");
                }
                success!("Wrote [{}] bytes to [{filename}]", data.len());
            }
            Err(error) => error!("Couldn't create raw binary file [{filename}]: {error}"),
        }
    }

    fn handle_datatype_header(&mut self, data: &[u8]) {
        self.header = Some(data.to_vec());
    }

    fn handle_datatype_screenshot(&mut self, data: &[u8]) {
        let header = match self.header.take() {
            Some(header) => header,
            None => return error!("Got screenshot packet without header data"),
        };
        let ScreenshotMetadata {
            format,
            height,
            width,
        } = match header.try_into() {
            Ok(data) => data,
            Err(error) => return error!("{error}"),
        };
        let format_size: u32 = format.into();
        if data.len() as u32 != format_size * width * height {
            return error!("Data length did not match header data for screenshot datatype");
        }
        let mut image = image::RgbaImage::new(width, height);
        for (x, y, pixel) in image.enumerate_pixels_mut() {
            let location = ((x + (y * width)) * format_size) as usize;
            let p = &data[location..location + format_size as usize];
            pixel.0 = match format {
                ScreenshotPixelFormat::Rgba16 => {
                    let r = ((p[0] >> 3) & 0x1F) << 3;
                    let g = (((p[0] & 0x07) << 2) | ((p[1] >> 6) & 0x03)) << 3;
                    let b = ((p[1] >> 1) & 0x1F) << 3;
                    let a = ((p[1]) & 0x01) * 255;
                    [r, g, b, a]
                }
                ScreenshotPixelFormat::Rgba32 => [p[0], p[1], p[2], p[3]],
            }
        }
        let filename = &generate_filename("screenshot", "png");
        if let Some(error) = image.save(filename).err() {
            return error!("Couldn't save screenshot [{filename}]: {error}");
        }
        success!("Wrote {width}x{height} pixels to [{filename}]");
    }

    fn handle_datatype_gdb(&self, data: &[u8]) {
        self.gdb_tx.send(data.to_vec()).ok();
    }
}

fn load_file(path: &str) -> Result<Vec<u8>, String> {
    if path.len() == 0 {
        return Err(format!("Couldn't open file: Specified path is empty"));
    }
    let mut file = match File::open(path) {
        Ok(file) => file,
        Err(error) => return Err(format!("Couldn't open file [{path}]: {error}")),
    };
    let length = match file.metadata() {
        Ok(metadata) => metadata.len(),
        Err(error) => return Err(format!("Couldn't get file [{path}] length: {error}")),
    };
    if length > MAX_PACKET_LENGTH as u64 {
        return Err(format!("File [{path}] size too big"));
    }
    let mut data = vec![0u8; length as usize];
    match file.read_exact(&mut data) {
        Ok(()) => Ok(data),
        Err(error) => Err(format!("Couldn't read file [{path}] contents: {error}")),
    }
}

fn generate_filename(prefix: &str, extension: &str) -> String {
    format!(
        "{prefix}-{}.{extension}",
        Local::now().format("%y%m%d%H%M%S.%f")
    )
}

pub fn new(gdb_port: Option<u16>) -> Result<Handler, sc64::Error> {
    let (line_tx, line_rx) = channel::<String>();
    let (gdb_tx, gdb_loop_rx) = channel::<Vec<u8>>();
    let (gdb_loop_tx, gdb_rx) = channel::<Vec<u8>>();

    spawn(move || stdin_thread(line_tx));

    if let Some(port) = gdb_port {
        let listener = TcpListener::bind(format!("0.0.0.0:{port}"))
            .map_err(|_| sc64::Error::new("Couldn't open GDB TCP socket port"))?;
        listener.set_nonblocking(true).map_err(|_| {
            sc64::Error::new("Couldn't set GDB TCP socket listener as non-blocking")
        })?;
        spawn(move || gdb_thread(listener, gdb_loop_tx, gdb_loop_rx));
    }

    Ok(Handler {
        header: None,
        line_rx,
        gdb_tx,
        gdb_rx,
    })
}

fn stdin_thread(line_tx: Sender<String>) {
    loop {
        let mut line = String::new();
        if stdin().read_line(&mut line).is_ok() {
            if line_tx.send(line.trim_end().to_string()).is_err() {
                return;
            }
        }
    }
}

fn gdb_thread(listener: TcpListener, gdb_tx: Sender<Vec<u8>>, gdb_rx: Receiver<Vec<u8>>) {
    match panic::catch_unwind(|| gdb_loop(listener, gdb_tx, gdb_rx)) {
        Ok(_) => {}
        Err(payload) => {
            eprintln!("{}", panic_message(&payload).red());
        }
    };
}

fn gdb_loop(listener: TcpListener, gdb_tx: Sender<Vec<u8>>, gdb_rx: Receiver<Vec<u8>>) {
    for tcp_stream in listener.incoming() {
        match tcp_stream {
            Ok(mut stream) => {
                handle_gdb_connection(&mut stream, &gdb_tx, &gdb_rx);
            }
            Err(error) => {
                if error.kind() == ErrorKind::WouldBlock {
                    sleep(Duration::from_millis(1));
                } else {
                    panic!("{error}");
                }
            }
        }
    }
}

fn handle_gdb_connection(
    stream: &mut TcpStream,
    gdb_tx: &Sender<Vec<u8>>,
    gdb_rx: &Receiver<Vec<u8>>,
) {
    const GDB_DATA_BUFFER: usize = 64 * 1024;

    let mut buffer = vec![0u8; GDB_DATA_BUFFER];

    let peer = stream.peer_addr().unwrap();

    println!("[GDB]: New connection ({peer})");

    loop {
        match stream.read(&mut buffer) {
            Ok(length) => {
                if length > 0 {
                    gdb_tx.send(buffer[0..length].to_vec()).ok();
                    continue;
                } else {
                    println!("[GDB]: Connection closed ({peer})");
                    break;
                }
            }
            Err(e) => {
                if e.kind() != ErrorKind::WouldBlock {
                    println!("[GDB]: Connection closed ({peer}), read IO error: {e}");
                    break;
                }
            }
        }

        if let Ok(data) = gdb_rx.try_recv() {
            match stream.write_all(&data) {
                Ok(()) => {
                    continue;
                }
                Err(e) => {
                    println!("[GDB]: Connection closed ({peer}), write IO error: {e}");
                    break;
                }
            }
        }

        sleep(Duration::from_millis(1));
    }
}
