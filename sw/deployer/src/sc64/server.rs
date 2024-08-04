use super::{
    error::Error,
    link::{list_local_devices, new_local, AsynchronousPacket, DataType, Response, UsbPacket},
};
use std::io::{Read, Write};

pub enum ServerEvent {
    Listening(String),
    Connected(String),
    Disconnected(String),
    Err(String),
}

struct StreamHandler {
    stream: std::net::TcpStream,
    reader: std::io::BufReader<std::net::TcpStream>,
    writer: std::io::BufWriter<std::net::TcpStream>,
}

const READ_TIMEOUT: std::time::Duration = std::time::Duration::from_secs(5);
const WRITE_TIMEOUT: std::time::Duration = std::time::Duration::from_secs(5);
const KEEPALIVE_PERIOD: std::time::Duration = std::time::Duration::from_secs(5);

impl StreamHandler {
    fn new(stream: std::net::TcpStream) -> std::io::Result<StreamHandler> {
        let reader = std::io::BufReader::new(stream.try_clone()?);
        let writer = std::io::BufWriter::new(stream.try_clone()?);
        stream.set_read_timeout(Some(READ_TIMEOUT))?;
        stream.set_write_timeout(Some(WRITE_TIMEOUT))?;
        Ok(StreamHandler {
            stream,
            reader,
            writer,
        })
    }

    fn try_read_header(&mut self) -> std::io::Result<Option<[u8; 4]>> {
        self.stream.set_nonblocking(true)?;
        let mut header = [0u8; 4];
        let result = match self.reader.read_exact(&mut header) {
            Ok(()) => Ok(Some(header)),
            Err(error) => match error.kind() {
                std::io::ErrorKind::WouldBlock => Ok(None),
                _ => Err(error),
            },
        };
        self.stream.set_nonblocking(false)?;
        result
    }

    fn receive_command(&mut self) -> std::io::Result<Option<(u8, [u32; 2], Vec<u8>)>> {
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

            self.reader.read_exact(&mut id_buffer)?;
            let id = id_buffer[0];

            self.reader.read_exact(&mut buffer)?;
            args[0] = u32::from_be_bytes(buffer);
            self.reader.read_exact(&mut buffer)?;
            args[1] = u32::from_be_bytes(buffer);

            self.reader.read_exact(&mut buffer)?;
            let command_data_length = u32::from_be_bytes(buffer) as usize;
            let mut data = vec![0u8; command_data_length];
            self.reader.read_exact(&mut data)?;

            Ok(Some((id, args, data)))
        } else {
            Ok(None)
        }
    }

    fn send_response(&mut self, response: Response) -> std::io::Result<()> {
        self.writer
            .write_all(&u32::to_be_bytes(DataType::Response.into()))?;
        self.writer.write_all(&[response.id])?;
        self.writer.write_all(&[response.error as u8])?;
        self.writer
            .write_all(&(response.data.len() as u32).to_be_bytes())?;
        self.writer.write_all(&response.data)?;
        self.writer.flush()?;
        Ok(())
    }

    fn send_packet(&mut self, packet: AsynchronousPacket) -> std::io::Result<()> {
        self.writer
            .write_all(&u32::to_be_bytes(DataType::Packet.into()))?;
        self.writer.write_all(&[packet.id])?;
        self.writer
            .write_all(&(packet.data.len() as u32).to_be_bytes())?;
        self.writer.write_all(&packet.data)?;
        self.writer.flush()?;
        Ok(())
    }

    fn send_keepalive(&mut self) -> std::io::Result<()> {
        self.writer
            .write_all(&u32::to_be_bytes(DataType::KeepAlive.into()))?;
        self.writer.flush()?;
        Ok(())
    }
}

fn server_accept_connection(port: String, connection: &mut StreamHandler) -> Result<(), Error> {
    let mut link = new_local(&port)?;

    let mut keepalive = std::time::Instant::now();

    loop {
        match connection.receive_command() {
            Ok(Some((id, args, data))) => {
                link.execute_command_raw(id, args, &data, true, true)?;
            }
            Ok(None) => {}
            Err(error) => match error.kind() {
                std::io::ErrorKind::UnexpectedEof => return Ok(()),
                _ => return Err(error.into()),
            },
        };

        if let Some(usb_packet) = link.receive_response_or_packet()? {
            match usb_packet {
                UsbPacket::Response(response) => connection.send_response(response)?,
                UsbPacket::AsynchronousPacket(packet) => connection.send_packet(packet)?,
            }
        }

        if keepalive.elapsed() > KEEPALIVE_PERIOD {
            keepalive = std::time::Instant::now();
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
    let listener = std::net::TcpListener::bind(address)?;
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
