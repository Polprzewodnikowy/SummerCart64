use super::{
    error::Error,
    link::{
        list_local_devices, new_local, AsynchronousPacket, Command, DataType, Response, UsbPacket,
    },
};
use std::{
    io::{Read, Write},
    net::{TcpListener, TcpStream},
    time::{Duration, Instant},
};

pub enum ServerEvent {
    Listening(String),
    Connected(String),
    Disconnected(String),
    Err(String),
}

struct StreamHandler {
    stream: TcpStream,
}

const POLL_TIMEOUT: Duration = Duration::from_millis(1);
const READ_TIMEOUT: Duration = Duration::from_secs(5);
const WRITE_TIMEOUT: Duration = Duration::from_secs(5);
const KEEPALIVE_PERIOD: Duration = Duration::from_secs(5);

impl StreamHandler {
    fn new(stream: TcpStream) -> std::io::Result<StreamHandler> {
        stream.set_read_timeout(Some(READ_TIMEOUT))?;
        stream.set_write_timeout(Some(WRITE_TIMEOUT))?;
        Ok(StreamHandler { stream })
    }

    fn try_read_exact(&mut self, buffer: &mut [u8]) -> std::io::Result<Option<()>> {
        let mut position = 0;
        let length = buffer.len();
        let timeout = Instant::now();

        self.stream.set_read_timeout(Some(POLL_TIMEOUT))?;

        while position < length {
            match self.stream.read(&mut buffer[position..length]) {
                Ok(0) => return Err(std::io::ErrorKind::UnexpectedEof.into()),
                Ok(bytes) => position += bytes,
                Err(error) => match error.kind() {
                    std::io::ErrorKind::Interrupted
                    | std::io::ErrorKind::TimedOut
                    | std::io::ErrorKind::WouldBlock => {
                        if position == 0 {
                            break;
                        }
                    }
                    _ => return Err(error),
                },
            }
            if timeout.elapsed() > READ_TIMEOUT {
                return Err(std::io::ErrorKind::TimedOut.into());
            }
        }

        self.stream.set_read_timeout(Some(READ_TIMEOUT))?;

        if position > 0 {
            Ok(Some(()))
        } else {
            Ok(None)
        }
    }

    fn try_read_header(&mut self) -> std::io::Result<Option<[u8; 4]>> {
        let mut header = [0u8; 4];
        Ok(self.try_read_exact(&mut header)?.map(|_| header))
    }

    fn receive_command(&mut self) -> std::io::Result<Option<Command>> {
        if let Some(header) = self.try_read_header()? {
            if let Ok(data_type) = TryInto::<DataType>::try_into(u32::from_be_bytes(header)) {
                if !matches!(data_type, DataType::Command) {
                    return Err(std::io::Error::other(
                        "Received data type was not a command data type",
                    ));
                }
            }

            let mut buffer = [0u8; 4];
            let mut id_buffer = [0u8; 1];
            let mut args = [0u32; 2];

            self.stream.read_exact(&mut id_buffer)?;
            let id = id_buffer[0];

            self.stream.read_exact(&mut buffer)?;
            args[0] = u32::from_be_bytes(buffer);
            self.stream.read_exact(&mut buffer)?;
            args[1] = u32::from_be_bytes(buffer);

            self.stream.read_exact(&mut buffer)?;
            let command_data_length = u32::from_be_bytes(buffer) as usize;
            let mut data = vec![0u8; command_data_length];
            self.stream.read_exact(&mut data)?;

            Ok(Some(Command { id, args, data }))
        } else {
            Ok(None)
        }
    }

    fn send_response(&mut self, response: Response) -> std::io::Result<()> {
        self.stream
            .write_all(&u32::to_be_bytes(DataType::Response.into()))?;
        self.stream.write_all(&[response.id])?;
        self.stream.write_all(&[response.error as u8])?;
        self.stream
            .write_all(&(response.data.len() as u32).to_be_bytes())?;
        self.stream.write_all(&response.data)?;
        self.stream.flush()?;
        Ok(())
    }

    fn send_packet(&mut self, packet: AsynchronousPacket) -> std::io::Result<()> {
        self.stream
            .write_all(&u32::to_be_bytes(DataType::Packet.into()))?;
        self.stream.write_all(&[packet.id])?;
        self.stream
            .write_all(&(packet.data.len() as u32).to_be_bytes())?;
        self.stream.write_all(&packet.data)?;
        self.stream.flush()?;
        Ok(())
    }

    fn send_keepalive(&mut self) -> std::io::Result<()> {
        self.stream
            .write_all(&u32::to_be_bytes(DataType::KeepAlive.into()))?;
        self.stream.flush()?;
        Ok(())
    }
}

fn server_accept_connection(port: String, connection: &mut StreamHandler) -> Result<(), Error> {
    let mut link = new_local(&port)?;

    let mut keepalive = Instant::now();

    loop {
        while let Some(usb_packet) = link.receive_response_or_packet()? {
            match usb_packet {
                UsbPacket::Response(response) => connection.send_response(response)?,
                UsbPacket::AsynchronousPacket(packet) => connection.send_packet(packet)?,
            }
        }

        match connection.receive_command() {
            Ok(Some(command)) => {
                link.execute_command_raw(&command, true, true)?;
            }
            Ok(None) => {}
            Err(error) => match error.kind() {
                std::io::ErrorKind::UnexpectedEof => return Ok(()),
                _ => return Err(error.into()),
            },
        };

        if keepalive.elapsed() > KEEPALIVE_PERIOD {
            keepalive = Instant::now();
            connection.send_keepalive().ok();
        }
    }
}

pub fn run(
    port: Option<String>,
    address: String,
    event_callback: fn(ServerEvent),
) -> Result<(), Error> {
    let port = port.unwrap_or(list_local_devices()?[0].port.clone());
    let listener = TcpListener::bind(address)?;
    let listening_address = listener.local_addr()?;

    event_callback(ServerEvent::Listening(listening_address.to_string()));

    for incoming in listener.incoming() {
        let stream = incoming?;
        let peer = stream.peer_addr()?.to_string();

        event_callback(ServerEvent::Connected(peer.clone()));

        match server_accept_connection(port.clone(), &mut StreamHandler::new(stream)?) {
            Ok(()) => event_callback(ServerEvent::Disconnected(peer.clone())),
            Err(error) => event_callback(ServerEvent::Err(error.to_string())),
        }
    }

    Ok(())
}
