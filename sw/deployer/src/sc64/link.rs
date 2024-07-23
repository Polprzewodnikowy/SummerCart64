use super::{error::Error, ftdi::FtdiDevice, serial::SerialDevice};
use std::{
    collections::VecDeque,
    fmt::Display,
    io::{BufReader, BufWriter, Read, Write},
    net::TcpStream,
    time::{Duration, Instant},
};

pub enum DataType {
    Command,
    Response,
    Packet,
    KeepAlive,
}

impl From<DataType> for u32 {
    fn from(value: DataType) -> Self {
        match value {
            DataType::Command => 1,
            DataType::Response => 2,
            DataType::Packet => 3,
            DataType::KeepAlive => 0xCAFEBEEF,
        }
    }
}

impl TryFrom<u32> for DataType {
    type Error = Error;
    fn try_from(value: u32) -> Result<Self, Self::Error> {
        Ok(match value {
            1 => Self::Command,
            2 => Self::Response,
            3 => Self::Packet,
            0xCAFEBEEF => Self::KeepAlive,
            _ => return Err(Error::new("Unknown data type")),
        })
    }
}

pub struct Response {
    pub id: u8,
    pub data: Vec<u8>,
    pub error: bool,
}

pub struct AsynchronousPacket {
    pub id: u8,
    pub data: Vec<u8>,
}

pub enum UsbPacket {
    Response(Response),
    AsynchronousPacket(AsynchronousPacket),
}

const SERIAL_PREFIX: &str = "serial://";
const FTDI_PREFIX: &str = "ftdi://";

const RESET_TIMEOUT: Duration = Duration::from_secs(1);
const POLL_TIMEOUT: Duration = Duration::from_millis(5);
const READ_TIMEOUT: Duration = Duration::from_secs(5);
const WRITE_TIMEOUT: Duration = Duration::from_secs(5);

pub trait Backend {
    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize>;

    fn write_all(&mut self, buffer: &[u8]) -> std::io::Result<()>;

    fn flush(&mut self) -> std::io::Result<()>;

    fn discard_input(&mut self) -> std::io::Result<()> {
        Ok(())
    }

    fn discard_output(&mut self) -> std::io::Result<()> {
        Ok(())
    }

    fn set_dtr(&mut self, _value: bool) -> std::io::Result<()> {
        Ok(())
    }

    fn read_dsr(&mut self) -> std::io::Result<bool> {
        Ok(false)
    }

    fn close(&mut self) {}

    fn reset(&mut self) -> std::io::Result<()> {
        self.discard_output()?;

        let timeout = Instant::now();
        self.set_dtr(true)?;
        loop {
            if self.read_dsr()? {
                break;
            }
            if timeout.elapsed() > RESET_TIMEOUT {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::TimedOut,
                    "Couldn't reset SC64 device (on)",
                ));
            }
        }

        self.discard_input()?;

        let timeout = Instant::now();
        self.set_dtr(false)?;
        loop {
            if !self.read_dsr()? {
                break;
            }
            if timeout.elapsed() > RESET_TIMEOUT {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::TimedOut,
                    "Couldn't reset SC64 device (off)",
                ));
            }
        }

        Ok(())
    }

    fn try_read_exact(&mut self, buffer: &mut [u8], block: bool) -> std::io::Result<Option<()>> {
        let mut position = 0;
        let length = buffer.len();
        let timeout = Instant::now();
        while position < length {
            match self.read(&mut buffer[position..length]) {
                Ok(0) => return Err(std::io::ErrorKind::UnexpectedEof.into()),
                Ok(bytes) => position += bytes,
                Err(error) => match error.kind() {
                    std::io::ErrorKind::Interrupted
                    | std::io::ErrorKind::TimedOut
                    | std::io::ErrorKind::WouldBlock => {
                        if !block && position == 0 {
                            return Ok(None);
                        }
                    }
                    _ => return Err(error.into()),
                },
            }
            if timeout.elapsed() > READ_TIMEOUT {
                return Err(std::io::ErrorKind::TimedOut.into());
            }
        }
        Ok(Some(()))
    }

    fn try_read_header(&mut self, block: bool) -> std::io::Result<Option<[u8; 4]>> {
        let mut header = [0u8; 4];
        Ok(self.try_read_exact(&mut header, block)?.map(|_| header))
    }

    fn read_exact(&mut self, buffer: &mut [u8]) -> std::io::Result<()> {
        match self.try_read_exact(buffer, true)? {
            Some(()) => Ok(()),
            None => Err(std::io::ErrorKind::UnexpectedEof.into()),
        }
    }

    fn send_command(&mut self, id: u8, args: [u32; 2], data: &[u8]) -> std::io::Result<()> {
        self.write_all(b"CMD")?;
        self.write_all(&id.to_be_bytes())?;

        self.write_all(&args[0].to_be_bytes())?;
        self.write_all(&args[1].to_be_bytes())?;

        self.write_all(data)?;

        self.flush()?;

        Ok(())
    }

    fn process_incoming_data(
        &mut self,
        data_type: DataType,
        packets: &mut VecDeque<AsynchronousPacket>,
    ) -> std::io::Result<Option<Response>> {
        let block = matches!(data_type, DataType::Response);

        while let Some(header) = self.try_read_header(block)? {
            let (packet_token, error) = match &header[0..3] {
                b"CMP" => (false, false),
                b"PKT" => (true, false),
                b"ERR" => (false, true),
                _ => return Err(std::io::ErrorKind::InvalidData.into()),
            };
            let id = header[3];

            let mut buffer = [0u8; 4];

            self.read_exact(&mut buffer)?;
            let length = u32::from_be_bytes(buffer) as usize;

            let mut data = vec![0u8; length];
            self.read_exact(&mut data)?;

            if packet_token {
                packets.push_back(AsynchronousPacket { id, data });
                if matches!(data_type, DataType::Packet) {
                    break;
                }
            } else {
                return Ok(Some(Response { id, error, data }));
            }
        }

        Ok(None)
    }
}

pub struct SerialBackend {
    device: SerialDevice,
}

impl Backend for SerialBackend {
    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        self.device.read(buffer)
    }

    fn write_all(&mut self, buffer: &[u8]) -> std::io::Result<()> {
        self.device.write_all(buffer)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.device.flush()
    }

    fn discard_input(&mut self) -> std::io::Result<()> {
        self.device.discard_input()
    }

    fn discard_output(&mut self) -> std::io::Result<()> {
        self.device.discard_output()
    }

    fn set_dtr(&mut self, value: bool) -> std::io::Result<()> {
        self.device.set_dtr(value)
    }

    fn read_dsr(&mut self) -> std::io::Result<bool> {
        self.device.read_dsr()
    }
}

fn new_serial_backend(port: &str) -> std::io::Result<SerialBackend> {
    Ok(SerialBackend {
        device: SerialDevice::new(
            port,
            Some(POLL_TIMEOUT),
            Some(READ_TIMEOUT),
            Some(WRITE_TIMEOUT),
        )?,
    })
}

struct FtdiBackend {
    device: FtdiDevice,
}

impl Backend for FtdiBackend {
    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        self.device.read(buffer)
    }

    fn write_all(&mut self, buffer: &[u8]) -> std::io::Result<()> {
        self.device.write_all(buffer)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.device.flush()
    }

    fn discard_input(&mut self) -> std::io::Result<()> {
        self.device.discard_input()
    }

    fn discard_output(&mut self) -> std::io::Result<()> {
        self.device.discard_output()
    }

    fn set_dtr(&mut self, value: bool) -> std::io::Result<()> {
        self.device.set_dtr(value)
    }

    fn read_dsr(&mut self) -> std::io::Result<bool> {
        self.device.read_dsr()
    }
}

fn new_ftdi_backend(port: &str) -> std::io::Result<FtdiBackend> {
    Ok(FtdiBackend {
        device: FtdiDevice::open(
            port,
            Some(POLL_TIMEOUT),
            Some(READ_TIMEOUT),
            Some(WRITE_TIMEOUT),
        )?,
    })
}

struct TcpBackend {
    stream: TcpStream,
    reader: BufReader<TcpStream>,
    writer: BufWriter<TcpStream>,
}

impl Backend for TcpBackend {
    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        self.reader.read(buffer)
    }

    fn write_all(&mut self, buffer: &[u8]) -> std::io::Result<()> {
        self.writer.write_all(buffer)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.writer.flush()
    }

    fn close(&mut self) {
        self.stream.shutdown(std::net::Shutdown::Both).ok();
    }

    fn send_command(&mut self, id: u8, args: [u32; 2], data: &[u8]) -> std::io::Result<()> {
        let payload_data_type: u32 = DataType::Command.into();
        self.write_all(&payload_data_type.to_be_bytes())?;

        self.write_all(&id.to_be_bytes())?;
        self.write_all(&args[0].to_be_bytes())?;
        self.write_all(&args[1].to_be_bytes())?;

        let command_data_length = data.len() as u32;
        self.write_all(&command_data_length.to_be_bytes())?;
        self.write_all(data)?;

        self.flush()?;

        Ok(())
    }

    fn process_incoming_data(
        &mut self,
        data_type: DataType,
        packets: &mut VecDeque<AsynchronousPacket>,
    ) -> std::io::Result<Option<Response>> {
        let block = matches!(data_type, DataType::Response);
        while let Some(header) = self.try_read_header(block)? {
            let payload_data_type: DataType = u32::from_be_bytes(header)
                .try_into()
                .map_err(|_| std::io::ErrorKind::InvalidData)?;
            let mut buffer = [0u8; 4];
            match payload_data_type {
                DataType::Response => {
                    let mut response_info = vec![0u8; 2];
                    self.read_exact(&mut response_info)?;

                    self.read_exact(&mut buffer)?;
                    let response_data_length = u32::from_be_bytes(buffer) as usize;

                    let mut data = vec![0u8; response_data_length];
                    self.read_exact(&mut data)?;

                    return Ok(Some(Response {
                        id: response_info[0],
                        error: response_info[1] != 0,
                        data,
                    }));
                }
                DataType::Packet => {
                    let mut packet_info = vec![0u8; 1];
                    self.read_exact(&mut packet_info)?;

                    self.read_exact(&mut buffer)?;
                    let packet_data_length = u32::from_be_bytes(buffer) as usize;

                    let mut data = vec![0u8; packet_data_length];
                    self.read_exact(&mut data)?;

                    packets.push_back(AsynchronousPacket {
                        id: packet_info[0],
                        data,
                    });
                    if matches!(data_type, DataType::Packet) {
                        break;
                    }
                }
                DataType::KeepAlive => {}
                _ => return Err(std::io::ErrorKind::InvalidData.into()),
            };
        }

        Ok(None)
    }
}

fn new_tcp_backend(address: &str) -> Result<TcpBackend, Error> {
    let stream = TcpStream::connect(address).map_err(|error| {
        Error::new(format!("Couldn't connect to [{address}]: {error}").as_str())
    })?;
    stream.set_read_timeout(Some(POLL_TIMEOUT))?;
    stream.set_write_timeout(Some(WRITE_TIMEOUT))?;
    let reader = BufReader::new(stream.try_clone()?);
    let writer = BufWriter::new(stream.try_clone()?);
    Ok(TcpBackend {
        stream,
        reader,
        writer,
    })
}

fn new_local_backend(port: &str) -> Result<Box<dyn Backend>, Error> {
    let mut backend: Box<dyn Backend> = if port.starts_with(SERIAL_PREFIX) {
        Box::new(new_serial_backend(
            port.strip_prefix(SERIAL_PREFIX).unwrap_or_default(),
        )?)
    } else if port.starts_with(FTDI_PREFIX) {
        Box::new(new_ftdi_backend(
            port.strip_prefix(FTDI_PREFIX).unwrap_or_default(),
        )?)
    } else {
        return Err(Error::new("Invalid port prefix provided"));
    };
    backend.reset()?;
    Ok(backend)
}

fn new_remote_backend(address: &str) -> Result<Box<dyn Backend>, Error> {
    Ok(Box::new(new_tcp_backend(address)?))
}

pub struct Link {
    backend: Box<dyn Backend>,
    packets: VecDeque<AsynchronousPacket>,
}

impl Link {
    pub fn execute_command(
        &mut self,
        id: u8,
        args: [u32; 2],
        data: &[u8],
    ) -> Result<Vec<u8>, Error> {
        self.execute_command_raw(id, args, data, false, false)
    }

    pub fn execute_command_raw(
        &mut self,
        id: u8,
        args: [u32; 2],
        data: &[u8],
        no_response: bool,
        ignore_error: bool,
    ) -> Result<Vec<u8>, Error> {
        self.backend.send_command(id, args, data)?;
        if no_response {
            return Ok(vec![]);
        }
        let response = self.receive_response()?;
        if id != response.id {
            return Err(Error::new("Command response ID didn't match"));
        }
        if !ignore_error && response.error {
            return Err(Error::new("Command response error"));
        }
        Ok(response.data)
    }

    pub fn receive_response(&mut self) -> Result<Response, Error> {
        match self
            .backend
            .process_incoming_data(DataType::Response, &mut self.packets)
        {
            Ok(response) => match response {
                Some(response) => Ok(response),
                None => Err(Error::new("No response was received")),
            },
            Err(error) => Err(Error::new(
                format!("Command response error: {error}").as_str(),
            )),
        }
    }

    pub fn receive_packet(&mut self) -> Result<Option<AsynchronousPacket>, Error> {
        if self.packets.len() == 0 {
            let response = self
                .backend
                .process_incoming_data(DataType::Packet, &mut self.packets)?;
            if response.is_some() {
                return Err(Error::new("Unexpected command response in data stream"));
            }
        }
        Ok(self.packets.pop_front())
    }

    pub fn receive_response_or_packet(&mut self) -> Result<Option<UsbPacket>, Error> {
        let response = self
            .backend
            .process_incoming_data(DataType::Packet, &mut self.packets)?;
        if let Some(response) = response {
            return Ok(Some(UsbPacket::Response(response)));
        }
        if let Some(packet) = self.packets.pop_front() {
            return Ok(Some(UsbPacket::AsynchronousPacket(packet)));
        }
        Ok(None)
    }
}

impl Drop for Link {
    fn drop(&mut self) {
        self.backend.close();
    }
}

pub fn new_local(port: &str) -> Result<Link, Error> {
    Ok(Link {
        backend: new_local_backend(port)?,
        packets: VecDeque::new(),
    })
}

pub fn new_remote(address: &str) -> Result<Link, Error> {
    Ok(Link {
        backend: new_remote_backend(address)?,
        packets: VecDeque::new(),
    })
}

pub enum BackendType {
    Serial,
    Ftdi,
}

impl Display for BackendType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(match self {
            Self::Serial => "serial",
            Self::Ftdi => "libftdi",
        })
    }
}

pub struct DeviceInfo {
    pub backend: BackendType,
    pub port: String,
    pub serial: String,
}

pub fn list_local_devices() -> Result<Vec<DeviceInfo>, Error> {
    const SC64_VID: u16 = 0x0403;
    const SC64_PID: u16 = 0x6014;
    const SC64_SERIAL_PREFIX: &str = "SC64";
    const SC64_DESCRIPTION: &str = "SC64";

    let mut devices: Vec<DeviceInfo> = Vec::new();

    if let Ok(list) = FtdiDevice::list(SC64_VID, SC64_PID) {
        for device in list.into_iter() {
            if device.description == SC64_DESCRIPTION {
                devices.push(DeviceInfo {
                    backend: BackendType::Ftdi,
                    port: format!("{FTDI_PREFIX}{}", device.port),
                    serial: device.serial,
                })
            }
        }
    }

    if let Ok(list) = SerialDevice::list(SC64_VID, SC64_PID) {
        for device in list.into_iter() {
            let is_sc64_device = device.description == SC64_DESCRIPTION
                || device.serial.starts_with(SC64_SERIAL_PREFIX);
            if is_sc64_device {
                devices.push(DeviceInfo {
                    backend: BackendType::Serial,
                    port: format!("{SERIAL_PREFIX}{}", device.port),
                    serial: device.serial,
                });
            }
        }
    }

    if devices.len() == 0 {
        return Err(Error::new("No SC64 devices found"));
    }

    return Ok(devices);
}
