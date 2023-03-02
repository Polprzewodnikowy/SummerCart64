use crate::sc64;
use chrono::Local;
use std::{
    io::{ErrorKind, Read, Write},
    net::{TcpListener, TcpStream},
    sync::mpsc::{channel, Receiver, Sender},
    thread::{sleep, spawn},
    time::Duration,
};

pub struct Handler {
    header: Option<Vec<u8>>,
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

impl Handler {
    pub fn handle_debug_packet(
        &mut self,
        debug_packet: sc64::DebugPacket,
    ) -> Result<(), sc64::Error> {
        let sc64::DebugPacket { datatype, data } = debug_packet;
        match datatype.into() {
            DataType::Text => self.handle_datatype_text(&data),
            DataType::RawBinary => self.handle_datatype_raw_binary(&data),
            DataType::Header => self.handle_datatype_header(&data),
            DataType::Screenshot => self.handle_datatype_screenshot(&data),
            DataType::GDB => self.handle_datatype_gdb(&data),
            _ => {
                println!("Unknown debug packet datatype: 0x{datatype:02X}");
                Ok(())
            }
        }
    }

    fn handle_datatype_text(&self, data: &[u8]) -> Result<(), sc64::Error> {
        print!("{}", String::from_utf8_lossy(data));
        Ok(())
    }

    fn handle_datatype_raw_binary(&self, data: &[u8]) -> Result<(), sc64::Error> {
        let filename = &self.generate_filename("binaryout", "bin");
        let mut file = std::fs::File::create(filename)?;
        file.write_all(data)?;
        println!("Wrote {} bytes to {}", data.len(), filename);
        Ok(())
    }

    fn handle_datatype_header(&mut self, data: &[u8]) -> Result<(), sc64::Error> {
        self.header = Some(data.to_vec());
        Ok(())
    }

    fn handle_datatype_screenshot(&mut self, _data: &[u8]) -> Result<(), sc64::Error> {
        if let Some(header) = self.header.take() {
            // TODO: support screenshot datatype
            println!("Screenshot datatype not supported yet {:?}", header);
        } else {
            println!("Got screenshot packet without header data");
        }
        Ok(())
    }

    fn handle_datatype_gdb(&self, data: &[u8]) -> Result<(), sc64::Error> {
        self.gdb_tx.send(data.to_vec()).ok();
        Ok(())
    }

    fn generate_filename(&self, prefix: &str, extension: &str) -> String {
        format!(
            "{prefix}-{}.{extension}",
            Local::now().format("%y%m%d%H%M%S.%f")
        )
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
}

pub fn new(gdb_port: Option<u16>) -> Result<Handler, sc64::Error> {
    let (gdb_tx, gdb_loop_rx) = channel::<Vec<u8>>();
    let (gdb_loop_tx, gdb_rx) = channel::<Vec<u8>>();

    if let Some(port) = gdb_port {
        let listener = TcpListener::bind(format!("0.0.0.0:{port}"))
            .map_err(|_| sc64::Error::new("Couldn't open GDB TCP socket port"))?;
        listener.set_nonblocking(true).map_err(|_| {
            sc64::Error::new("Couldn't set GDB TCP socket listener as non-blocking")
        })?;
        spawn(move || gdb_loop(listener, gdb_loop_tx, gdb_loop_rx));
    }

    Ok(Handler {
        header: None,
        gdb_tx,
        gdb_rx,
    })
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
    const GDB_DATA_BUFFER: usize = 8 * 1024;

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
