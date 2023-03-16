use super::{
    error::Error,
    link::{new_serial, Command, DataType, Packet, Response, Serial},
};
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

    let serial = Arc::new(new_serial(port)?);
    let serial_writer = serial.clone();
    let serial_reader = serial.clone();

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

    let mut header = [0u8; 4];
    let header_length = header.len();

    loop {
        let mut header_position = 0;

        let timeout = stream.read_timeout()?;
        stream.set_read_timeout(Some(Duration::from_millis(10)))?;
        while header_position < header_length {
            if exit_flag.load(Ordering::Relaxed) {
                return Ok(());
            }
            match stream.read(&mut header[header_position..header_length]) {
                Ok(0) => return Ok(()),
                Ok(bytes) => header_position += bytes,
                Err(error) => match error.kind() {
                    ErrorKind::Interrupted | ErrorKind::TimedOut | ErrorKind::WouldBlock => {}
                    _ => return Err(error.into()),
                },
            }
        }
        stream.set_read_timeout(timeout)?;

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
            keepalive = Instant::now();
            let event = Event::KeepAlive;
            if event_sender.send(event).is_err() {
                break;
            }
        } else {
            thread::sleep(Duration::from_millis(10));
        }
    }
}
