use super::error::Error;
use serial2::SerialPort;
use std::{
    collections::VecDeque,
    io::{BufReader, BufWriter, ErrorKind, Read, Write},
    net::{TcpListener, TcpStream},
    sync::{
        atomic::{AtomicBool, Ordering},
        mpsc::{channel, Receiver, Sender},
        Arc,
    },
    thread,
    time::{Duration, Instant},
};

enum DataType {
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

struct Serial {
    serial: SerialPort,
}

impl Serial {
    fn reset(&self) -> Result<(), Error> {
        const WAIT_DURATION: Duration = Duration::from_millis(10);
        const RETRY_COUNT: i32 = 100;

        self.serial.set_dtr(true)?;
        for n in 0..=RETRY_COUNT {
            self.serial.discard_buffers()?;
            thread::sleep(WAIT_DURATION);
            if self.serial.read_dsr()? {
                break;
            }
            if n == RETRY_COUNT {
                return Err(Error::new("Couldn't reset SC64 device (on)"));
            }
        }

        self.serial.set_dtr(false)?;
        for n in 0..=RETRY_COUNT {
            thread::sleep(WAIT_DURATION);
            if !self.serial.read_dsr()? {
                break;
            }
            if n == RETRY_COUNT {
                return Err(Error::new("Couldn't reset SC64 device (off)"));
            }
        }

        Ok(())
    }

    fn read_data(&self, buffer: &mut [u8], block: bool) -> Result<Option<()>, Error> {
        let timeout = Instant::now();
        let mut position = 0;
        let length = buffer.len();
        while position < length {
            if timeout.elapsed() > Duration::from_secs(5) {
                return Err(Error::new("Serial read timeout"));
            }
            match self.serial.read(&mut buffer[position..length]) {
                Ok(0) => return Err(Error::new("Unexpected end of serial data")),
                Ok(bytes) => position += bytes,
                Err(error) => match error.kind() {
                    ErrorKind::Interrupted | ErrorKind::TimedOut => {
                        if !block && position == 0 {
                            return Ok(None);
                        }
                    }
                    _ => return Err(error.into()),
                },
            }
        }
        Ok(Some(()))
    }

    fn read_exact(&self, buffer: &mut [u8]) -> Result<(), Error> {
        match self.read_data(buffer, true)? {
            Some(()) => Ok(()),
            None => Err(Error::new("Unexpected end of serial data")),
        }
    }

    fn read_header(&self, block: bool) -> Result<Option<[u8; 4]>, Error> {
        let mut header = [0u8; 4];
        Ok(self.read_data(&mut header, block)?.map(|_| header))
    }

    fn send_command(&self, command: &Command) -> Result<(), Error> {
        self.serial.write_all(b"CMD")?;
        self.serial.write_all(&command.id.to_be_bytes())?;
        self.serial.write_all(&command.args[0].to_be_bytes())?;
        self.serial.write_all(&command.args[1].to_be_bytes())?;

        self.serial.write_all(&command.data)?;

        self.serial.flush()?;

        Ok(())
    }

    fn process_incoming_data(
        &self,
        data_type: DataType,
        packets: &mut VecDeque<Packet>,
    ) -> Result<Option<Response>, Error> {
        let block = matches!(data_type, DataType::Response);
        while let Some(header) = self.read_header(block)? {
            let (packet_token, error) = (match &header[0..3] {
                b"CMP" => Ok((false, false)),
                b"PKT" => Ok((true, false)),
                b"ERR" => Ok((false, true)),
                _ => Err(Error::new("Unknown response token")),
            })?;
            let id = header[3];

            let mut buffer = [0u8; 4];

            self.read_exact(&mut buffer)?;
            let length = u32::from_be_bytes(buffer) as usize;

            let mut data = vec![0u8; length];
            self.read_exact(&mut data)?;

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

fn new_serial(port: &str) -> Result<Serial, Error> {
    let mut serial = SerialPort::open(port, 115_200)?;
    serial.set_read_timeout(Duration::from_millis(10))?;
    serial.set_write_timeout(Duration::from_secs(5))?;
    let backend = Serial { serial };
    backend.reset()?;
    Ok(backend)
}

struct SerialBackend {
    inner: Serial,
}

impl Backend for SerialBackend {
    fn send_command(&mut self, command: &Command) -> Result<(), Error> {
        self.inner.send_command(command)
    }

    fn process_incoming_data(
        &mut self,
        data_type: DataType,
        packets: &mut VecDeque<Packet>,
    ) -> Result<Option<Response>, Error> {
        self.inner.process_incoming_data(data_type, packets)
    }
}

fn new_serial_backend(port: &str) -> Result<SerialBackend, Error> {
    let backend = SerialBackend {
        inner: new_serial(port)?,
    };
    Ok(backend)
}

struct TcpBackend {
    reader: BufReader<TcpStream>,
    writer: BufWriter<TcpStream>,
}

impl TcpBackend {
    fn read_data(&mut self, buffer: &mut [u8], block: bool) -> Result<Option<()>, Error> {
        let timeout = Instant::now();
        let mut position = 0;
        let length = buffer.len();
        while position < length {
            if timeout.elapsed() > Duration::from_secs(10) {
                return Err(Error::new("Stream read timeout"));
            }
            match self.reader.read(&mut buffer[position..length]) {
                Ok(0) => return Err(Error::new("Unexpected end of stream data")),
                Ok(bytes) => position += bytes,
                Err(error) => match error.kind() {
                    ErrorKind::Interrupted | ErrorKind::TimedOut => {
                        if !block && position == 0 {
                            return Ok(None);
                        }
                    }
                    _ => return Err(error.into()),
                },
            }
        }
        Ok(Some(()))
    }

    fn read_exact(&mut self, buffer: &mut [u8]) -> Result<(), Error> {
        match self.read_data(buffer, true)? {
            Some(()) => Ok(()),
            None => Err(Error::new("Unexpected end of stream data")),
        }
    }

    fn read_header(&mut self, block: bool) -> Result<Option<[u8; 4]>, Error> {
        let mut header = [0u8; 4];
        Ok(self.read_data(&mut header, block)?.map(|_| header))
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
        let block = matches!(data_type, DataType::Response);
        while let Some(header) = self.read_header(block)? {
            let payload_data_type: DataType = u32::from_be_bytes(header).try_into()?;
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

                    packets.push_back(Packet {
                        id: packet_info[0],
                        data,
                    });
                    if matches!(data_type, DataType::Packet) {
                        break;
                    }
                }
                DataType::KeepAlive => {}
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
            stream.set_read_timeout(Some(Duration::from_millis(10)))?;
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
    Ok(TcpBackend { reader, writer })
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
    Listening(String),
    Connected(String),
    Disconnected(String),
    Err(String),
}

pub fn run_server(
    port: &str,
    address: String,
    event_callback: fn(ServerEvent),
) -> Result<(), Error> {
    let listener = TcpListener::bind(address)?;
    let listening_address = listener.local_addr()?;
    event_callback(ServerEvent::Listening(listening_address.to_string()));

    for stream in listener.incoming() {
        match stream {
            Ok(mut stream) => {
                let peer = stream.peer_addr()?.to_string();
                event_callback(ServerEvent::Connected(peer.clone()));
                match server_accept_connection(port, &mut stream) {
                    Ok(()) => event_callback(ServerEvent::Disconnected(peer.clone())),
                    Err(error) => event_callback(ServerEvent::Err(error.to_string())),
                }
            }
            Err(error) => match error.kind() {
                _ => return Err(error.into()),
            },
        }
    }

    Ok(())
}

enum Event {
    Command((u8, [u32; 2], Vec<u8>)),
    Response(Response),
    Packet(Packet),
    KeepAlive,
    Closed(Option<Error>),
}

fn server_accept_connection(port: &str, stream: &mut TcpStream) -> Result<(), Error> {
    let (event_sender, event_receiver) = channel::<Event>();
    let exit_flag = Arc::new(AtomicBool::new(false));

    let mut stream_writer = BufWriter::new(stream.try_clone()?);
    let mut stream_reader = stream.try_clone()?;

    let stream_event_sender = event_sender.clone();
    let stream_exit_flag = exit_flag.clone();
    let stream_thread = thread::spawn(move || {
        let closed_sender = stream_event_sender.clone();
        match server_stream_thread(&mut stream_reader, stream_event_sender, stream_exit_flag) {
            Ok(()) => closed_sender.send(Event::Closed(None)),
            Err(error) => closed_sender.send(Event::Closed(Some(error))),
        }
        .ok();
    });

    let serial = Arc::new(new_serial(port)?);
    let serial_writer = serial.clone();
    let serial_reader = serial.clone();

    let serial_event_sender = event_sender.clone();
    let serial_exit_flag = exit_flag.clone();
    let serial_thread = thread::spawn(move || {
        let closed_sender = serial_event_sender.clone();
        match server_serial_thread(serial_reader, serial_event_sender, serial_exit_flag) {
            Ok(()) => closed_sender.send(Event::Closed(None)),
            Err(error) => closed_sender.send(Event::Closed(Some(error))),
        }
        .ok();
    });

    let keepalive_event_sender = event_sender.clone();
    let keepalive_exit_flag = exit_flag.clone();
    let keepalive_thread = thread::spawn(move || {
        server_keepalive_thread(keepalive_event_sender, keepalive_exit_flag);
    });

    let result = server_process_events(&mut stream_writer, serial_writer, event_receiver);

    exit_flag.store(true, Ordering::Relaxed);
    stream_thread.join().ok();
    serial_thread.join().ok();
    keepalive_thread.join().ok();

    result
}

fn server_process_events(
    stream_writer: &mut BufWriter<TcpStream>,
    serial_writer: Arc<Serial>,
    event_receiver: Receiver<Event>,
) -> Result<(), Error> {
    for event in event_receiver.into_iter() {
        match event {
            Event::Command((id, args, data)) => {
                serial_writer.send_command(&Command {
                    id,
                    args,
                    data: &data,
                })?;
            }
            Event::Response(response) => {
                stream_writer.write_all(&u32::to_be_bytes(DataType::Response.into()))?;
                stream_writer.write_all(&[response.id])?;
                stream_writer.write_all(&[response.error as u8])?;
                stream_writer.write_all(&(response.data.len() as u32).to_be_bytes())?;
                stream_writer.write_all(&response.data)?;
                stream_writer.flush()?;
            }
            Event::Packet(packet) => {
                stream_writer.write_all(&u32::to_be_bytes(DataType::Packet.into()))?;
                stream_writer.write_all(&[packet.id])?;
                stream_writer.write_all(&(packet.data.len() as u32).to_be_bytes())?;
                stream_writer.write_all(&packet.data)?;
                stream_writer.flush()?;
            }
            Event::KeepAlive => {
                stream_writer.write_all(&u32::to_be_bytes(DataType::KeepAlive.into()))?;
                stream_writer.flush()?;
            }
            Event::Closed(result) => match result {
                Some(error) => return Err(error),
                None => {
                    break;
                }
            },
        }
    }

    Ok(())
}

fn server_stream_thread(
    stream: &mut TcpStream,
    event_sender: Sender<Event>,
    exit_flag: Arc<AtomicBool>,
) -> Result<(), Error> {
    let mut stream_reader = BufReader::new(stream.try_clone()?);

    loop {
        let mut header = [0u8; 4];
        let header_length = header.len();
        let mut header_position = 0;

        stream.set_read_timeout(Some(Duration::from_millis(10)))?;
        while header_position < header_length {
            if exit_flag.load(Ordering::Relaxed) {
                return Ok(());
            }
            match stream_reader.read(&mut header[header_position..header_length]) {
                Ok(0) => return Ok(()),
                Ok(bytes) => header_position += bytes,
                Err(error) => match error.kind() {
                    ErrorKind::Interrupted | ErrorKind::TimedOut => {}
                    _ => return Err(error.into()),
                },
            }
        }
        stream.set_read_timeout(None)?;

        let data_type: DataType = u32::from_be_bytes(header).try_into()?;
        if !matches!(data_type, DataType::Command) {
            return Err(Error::new("Received data type was not a command data type"));
        }

        let mut buffer = [0u8; 4];
        let mut id = [0u8; 1];
        let mut args = [0u32; 2];

        stream_reader.read_exact(&mut id)?;
        stream_reader.read_exact(&mut buffer)?;
        args[0] = u32::from_be_bytes(buffer);
        stream_reader.read_exact(&mut buffer)?;
        args[1] = u32::from_be_bytes(buffer);

        stream_reader.read_exact(&mut buffer)?;
        let command_data_length = u32::from_be_bytes(buffer) as usize;
        let mut data = vec![0u8; command_data_length];
        stream_reader.read_exact(&mut data)?;

        let event = Event::Command((id[0], args, data));
        if event_sender.send(event).is_err() {
            break;
        }
    }

    Ok(())
}

fn server_serial_thread(
    serial_reader: Arc<Serial>,
    event_sender: Sender<Event>,
    exit_flag: Arc<AtomicBool>,
) -> Result<(), Error> {
    let mut packets: VecDeque<Packet> = VecDeque::new();

    while !exit_flag.load(Ordering::Relaxed) {
        let response = serial_reader.process_incoming_data(DataType::Packet, &mut packets)?;
        if let Some(response) = response {
            let event = Event::Response(response);
            if event_sender.send(event).is_err() {
                break;
            }
        }
        if let Some(packet) = packets.pop_front() {
            let event = Event::Packet(packet);
            if event_sender.send(event).is_err() {
                break;
            }
        }
    }

    Ok(())
}

fn server_keepalive_thread(event_sender: Sender<Event>, exit_flag: Arc<AtomicBool>) {
    let mut keepalive = Instant::now();

    while !exit_flag.load(Ordering::Relaxed) {
        if keepalive.elapsed() >= Duration::from_secs(5) {
            let event = Event::KeepAlive;
            if event_sender.send(event).is_err() {
                break;
            }
            keepalive = Instant::now();
        } else {
            thread::sleep(Duration::from_millis(10));
        }
    }
}
