use super::error::Error;
use std::{
    collections::VecDeque,
    io::{BufRead, BufReader, BufWriter, ErrorKind, Read, Write},
    net::{TcpListener, TcpStream},
    time::Duration,
};

enum DataType {
    Command,
    Response,
    Packet,
}

impl From<DataType> for u32 {
    fn from(value: DataType) -> Self {
        match value {
            DataType::Command => 1,
            DataType::Response => 2,
            DataType::Packet => 3,
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
            _ => return Err(Error::new("Unknown data type")),
        })
    }
}

pub struct Command<'a> {
    pub id: u8,
    pub args: [u32; 2],
    pub data: &'a [u8],
}

pub struct Response {
    pub id: u8,
    pub data: Vec<u8>,
    pub error: bool,
}

pub struct Packet {
    pub id: u8,
    pub data: Vec<u8>,
}

trait Backend {
    fn send_command(&mut self, command: &Command) -> Result<(), Error>;
    fn process_incoming_data(
        &mut self,
        data_type: DataType,
        packets: &mut VecDeque<Packet>,
    ) -> Result<Option<Response>, Error>;
}

struct SerialBackend {
    serial: Box<dyn serialport::SerialPort>,
}

impl SerialBackend {
    fn reset(&mut self) -> Result<(), Error> {
        const WAIT_DURATION: Duration = Duration::from_millis(10);
        const RETRY_COUNT: i32 = 100;

        self.serial.write_data_terminal_ready(true)?;
        for n in 0..=RETRY_COUNT {
            self.serial.clear(serialport::ClearBuffer::All)?;
            std::thread::sleep(WAIT_DURATION);
            if self.serial.read_data_set_ready()? {
                break;
            }
            if n == RETRY_COUNT {
                return Err(Error::new("Couldn't reset SC64 device (on)"));
            }
        }

        self.serial.write_data_terminal_ready(false)?;
        for n in 0..=RETRY_COUNT {
            std::thread::sleep(WAIT_DURATION);
            if !self.serial.read_data_set_ready()? {
                break;
            }
            if n == RETRY_COUNT {
                return Err(Error::new("Couldn't reset SC64 device (off)"));
            }
        }

        Ok(())
    }
}

impl Backend for SerialBackend {
    fn send_command(&mut self, command: &Command) -> Result<(), Error> {
        self.serial.write_all(b"CMD")?;
        self.serial.write_all(&command.id.to_be_bytes())?;
        self.serial.write_all(&command.args[0].to_be_bytes())?;
        self.serial.write_all(&command.args[1].to_be_bytes())?;

        self.serial.write_all(&command.data)?;

        self.serial.flush()?;

        Ok(())
    }

    fn process_incoming_data(
        &mut self,
        data_type: DataType,
        packets: &mut VecDeque<Packet>,
    ) -> Result<Option<Response>, Error> {
        let mut buffer = [0u8; 4];

        while matches!(data_type, DataType::Response)
            || self.serial.bytes_to_read()? as usize >= buffer.len()
        {
            self.serial.read_exact(&mut buffer)?;
            let (packet_token, error) = (match &buffer[0..3] {
                b"CMP" => Ok((false, false)),
                b"PKT" => Ok((true, false)),
                b"ERR" => Ok((false, true)),
                _ => Err(Error::new("Unknown response token")),
            })?;
            let id = buffer[3];

            self.serial.read_exact(&mut buffer)?;
            let length = u32::from_be_bytes(buffer) as usize;

            let mut data = vec![0u8; length];
            self.serial.read_exact(&mut data)?;

            if packet_token {
                packets.push_back(Packet { id, data });
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

fn new_serial_backend(port: &str) -> Result<SerialBackend, Error> {
    let mut backend = SerialBackend {
        serial: serialport::new(port, 115_200)
            .timeout(Duration::from_secs(10))
            .open()?,
    };
    backend.reset()?;
    Ok(backend)
}

struct TcpBackend {
    stream: TcpStream,
    reader: BufReader<TcpStream>,
    writer: BufWriter<TcpStream>,
}

impl TcpBackend {
    fn bytes_to_read(&mut self) -> Result<usize, Error> {
        self.stream.set_nonblocking(true)?;
        let result = self.reader.fill_buf();
        let length = match result {
            Ok(buffer) => buffer.len(),
            Err(error) => {
                if error.kind() == ErrorKind::WouldBlock {
                    0
                } else {
                    return Err(error.into());
                }
            }
        };
        self.stream.set_nonblocking(false)?;
        return Ok(length);
    }
}

impl Backend for TcpBackend {
    fn send_command(&mut self, command: &Command) -> Result<(), Error> {
        let payload_data_type: u32 = DataType::Command.into();
        self.writer.write_all(&payload_data_type.to_be_bytes())?;

        self.writer.write_all(&command.id.to_be_bytes())?;
        self.writer.write_all(&command.args[0].to_be_bytes())?;
        self.writer.write_all(&command.args[1].to_be_bytes())?;

        let command_data_length = command.data.len() as u32;
        self.writer.write_all(&command_data_length.to_be_bytes())?;
        self.writer.write_all(&command.data)?;

        self.writer.flush()?;

        Ok(())
    }

    fn process_incoming_data(
        &mut self,
        data_type: DataType,
        packets: &mut VecDeque<Packet>,
    ) -> Result<Option<Response>, Error> {
        let mut buffer = [0u8; 4];

        while matches!(data_type, DataType::Response) || self.bytes_to_read()? >= 4 {
            self.reader.read_exact(&mut buffer)?;
            let payload_data_type: DataType = u32::from_be_bytes(buffer).try_into()?;

            match payload_data_type {
                DataType::Response => {
                    let mut response_info = vec![0u8; 2];
                    self.reader.read_exact(&mut response_info)?;

                    self.reader.read_exact(&mut buffer)?;
                    let response_data_length = u32::from_be_bytes(buffer) as usize;

                    let mut data = vec![0u8; response_data_length];
                    self.reader.read_exact(&mut data)?;

                    return Ok(Some(Response {
                        id: response_info[0],
                        error: response_info[1] != 0,
                        data,
                    }));
                }
                DataType::Packet => {
                    let mut packet_info = vec![0u8; 1];
                    self.reader.read_exact(&mut packet_info)?;

                    self.reader.read_exact(&mut buffer)?;
                    let packet_data_length = u32::from_be_bytes(buffer) as usize;

                    let mut data = vec![0u8; packet_data_length];
                    self.reader.read_exact(&mut data)?;

                    packets.push_back(Packet {
                        id: packet_info[0],
                        data,
                    });
                    if matches!(data_type, DataType::Packet) {
                        break;
                    }
                }
                _ => return Err(Error::new("Unexpected payload data type received")),
            };
        }

        Ok(None)
    }
}

fn new_tcp_backend(address: &str) -> Result<TcpBackend, Error> {
    let stream = match TcpStream::connect(address) {
        Ok(stream) => {
            stream.set_write_timeout(Some(Duration::from_secs(10)))?;
            stream.set_read_timeout(Some(Duration::from_secs(10)))?;
            stream
        }
        Err(error) => {
            return Err(Error::new(
                format!("Couldn't connect to [{address}]: {error}").as_str(),
            ))
        }
    };
    let reader = BufReader::new(stream.try_clone()?);
    let writer = BufWriter::new(stream.try_clone()?);
    Ok(TcpBackend {
        stream,
        reader,
        writer,
    })
}

pub struct Link {
    backend: Box<dyn Backend>,
    packets: VecDeque<Packet>,
}

impl Link {
    pub fn execute_command(&mut self, command: &Command) -> Result<Vec<u8>, Error> {
        self.execute_command_raw(command, false, false)
    }

    pub fn execute_command_raw(
        &mut self,
        command: &Command,
        no_response: bool,
        ignore_error: bool,
    ) -> Result<Vec<u8>, Error> {
        self.backend.send_command(command)?;
        if no_response {
            return Ok(vec![]);
        }
        let response = self.receive_response()?;
        if command.id != response.id {
            return Err(Error::new("Command response ID didn't match"));
        }
        if !ignore_error && response.error {
            return Err(Error::new("Command response error"));
        }
        Ok(response.data)
    }

    fn receive_response(&mut self) -> Result<Response, Error> {
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

    pub fn receive_packet(&mut self) -> Result<Option<Packet>, Error> {
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
}

pub fn new_local(port: &str) -> Result<Link, Error> {
    Ok(Link {
        backend: Box::new(new_serial_backend(port)?),
        packets: VecDeque::new(),
    })
}

pub fn new_remote(address: &str) -> Result<Link, Error> {
    Ok(Link {
        backend: Box::new(new_tcp_backend(address)?),
        packets: VecDeque::new(),
    })
}

pub struct LocalDevice {
    pub port: String,
    pub serial_number: String,
}

pub fn list_local_devices() -> Result<Vec<LocalDevice>, Error> {
    const SC64_VID: u16 = 0x0403;
    const SC64_PID: u16 = 0x6014;
    const SC64_SID: &str = "SC64";

    let mut serial_devices: Vec<LocalDevice> = Vec::new();

    for device in serialport::available_ports()?.into_iter() {
        if let serialport::SerialPortType::UsbPort(info) = device.port_type {
            let serial_number = info.serial_number.unwrap_or("".to_string());
            if info.vid == SC64_VID && info.pid == SC64_PID && serial_number.starts_with(SC64_SID) {
                serial_devices.push(LocalDevice {
                    port: device.port_name,
                    serial_number,
                });
            }
        }
    }

    if serial_devices.len() == 0 {
        return Err(Error::new("No SC64 devices found"));
    }

    return Ok(serial_devices);
}

pub enum ServerEvent {
    StartedListening(String),
    NewConnection(String),
    Disconnected(String),
    Err(String),
}

pub fn run_server(
    port: &str,
    address: String,
    event_callback: fn(ServerEvent),
) -> Result<(), Error> {
    let listener = TcpListener::bind(address)?;

    event_callback(ServerEvent::StartedListening(
        listener.local_addr()?.to_string(),
    ));

    for stream in listener.incoming() {
        match stream {
            Ok(mut stream) => match server_accept_connection(port, event_callback, &mut stream) {
                Ok(()) => {}
                Err(error) => event_callback(ServerEvent::Err(error.to_string())),
            },
            Err(error) => return Err(error.into()),
        }
    }

    Ok(())
}

fn server_accept_connection(
    port: &str,
    event_callback: fn(ServerEvent),
    stream: &mut TcpStream,
) -> Result<(), Error> {
    stream.set_nonblocking(true)?;
    let peer = stream.peer_addr()?.to_string();

    let mut serial_backend = new_serial_backend(port)?;
    serial_backend.reset()?;

    let mut packets: VecDeque<Packet> = VecDeque::new();

    let mut buffer = [0u8; 4];

    event_callback(ServerEvent::NewConnection(peer.clone()));

    loop {
        match stream.read_exact(&mut buffer) {
            Ok(()) => {
                let data_type: DataType = u32::from_be_bytes(buffer).try_into()?;

                if !matches!(data_type, DataType::Command) {
                    return Err(Error::new("Received data type wasn't a command data type"));
                }

                let mut id_buffer = [0u8; 1];
                let mut args = [0u32; 2];

                stream.read_exact(&mut id_buffer)?;
                stream.read_exact(&mut buffer)?;
                args[0] = u32::from_be_bytes(buffer);
                stream.read_exact(&mut buffer)?;
                args[1] = u32::from_be_bytes(buffer);

                stream.read_exact(&mut buffer)?;
                let command_data_length = u32::from_be_bytes(buffer) as usize;
                let mut data = vec![0u8; command_data_length];
                stream.read_exact(&mut data)?;

                serial_backend.send_command(&Command {
                    id: id_buffer[0],
                    args,
                    data: &data,
                })?;

                continue;
            }
            Err(error) => {
                if error.kind() != ErrorKind::WouldBlock {
                    event_callback(ServerEvent::Disconnected(peer.clone()));
                    return Ok(());
                }
            }
        }
        if let Some(response) =
            serial_backend.process_incoming_data(DataType::Packet, &mut packets)?
        {
            stream.write_all(&u32::to_be_bytes(DataType::Response.into()))?;
            stream.write_all(&[response.id])?;
            stream.write_all(&[response.error as u8])?;
            stream.write_all(&(response.data.len() as u32).to_be_bytes())?;
            stream.write_all(&response.data)?;
            stream.flush()?;
        } else if let Some(packet) = packets.pop_front() {
            stream.write_all(&u32::to_be_bytes(DataType::Packet.into()))?;
            stream.write_all(&[packet.id])?;
            stream.write_all(&(packet.data.len() as u32).to_be_bytes())?;
            stream.write_all(&packet.data)?;
            stream.flush()?;
        } else {
            std::thread::sleep(Duration::from_millis(1));
        }
    }
}
