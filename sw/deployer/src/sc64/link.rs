use super::{error::Error, utils};
use std::{collections::VecDeque, time::Duration};

pub struct LocalDevice {
    pub port: String,
    pub serial_number: String,
}

pub struct Command {
    pub id: u8,
    pub args: [u32; 2],
    pub data: Vec<u8>,
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

enum DataType {
    Response,
    Packet,
}

pub trait Link {
    fn execute_command(&mut self, command: &Command) -> Result<Vec<u8>, Error>;
    fn execute_command_raw(
        &mut self,
        command: &Command,
        timeout: Duration,
        no_response: bool,
        ignore_error: bool,
    ) -> Result<Vec<u8>, Error>;
    fn receive_packet(&mut self) -> Result<Option<Packet>, Error>;
}

pub struct SerialLink {
    serial: Box<dyn serialport::SerialPort>,
    packets: VecDeque<Packet>,
}

const COMMAND_TIMEOUT: Duration = Duration::from_secs(5);
const PACKET_TIMEOUT: Duration = Duration::from_secs(5);

impl SerialLink {
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

    fn serial_send_command(&mut self, command: &Command, timeout: Duration) -> Result<(), Error> {
        let mut header: Vec<u8> = Vec::new();
        header.append(&mut b"CMD".to_vec());
        header.append(&mut [command.id].to_vec());
        header.append(&mut command.args[0].to_be_bytes().to_vec());
        header.append(&mut command.args[1].to_be_bytes().to_vec());

        self.serial.set_timeout(timeout)?;
        self.serial.write_all(&header)?;
        self.serial.write_all(&command.data)?;
        self.serial.flush()?;

        Ok(())
    }

    fn serial_process_incoming_data(
        &mut self,
        data_type: DataType,
        timeout: Duration,
    ) -> Result<Option<Response>, Error> {
        const HEADER_SIZE: u32 = 8;

        let mut buffer = [0u8; HEADER_SIZE as usize];

        self.serial.set_timeout(timeout)?;

        while matches!(data_type, DataType::Response) || self.serial.bytes_to_read()? >= HEADER_SIZE
        {
            self.serial.read_exact(&mut buffer)?;

            let (packet_token, error) = (match &buffer[0..3] {
                b"CMP" => Ok((false, false)),
                b"PKT" => Ok((true, false)),
                b"ERR" => Ok((false, true)),
                _ => Err(Error::new("Unknown response token")),
            })?;
            let id = buffer[3];
            let length = utils::u32_from_vec(&buffer[4..8])? as usize;

            let mut data = vec![0u8; length];
            self.serial.read_exact(&mut data)?;

            if packet_token {
                self.packets.push_back(Packet { id, data });
                if matches!(data_type, DataType::Packet) {
                    return Ok(None);
                }
            } else {
                return Ok(Some(Response { id, error, data }));
            }
        }

        Ok(None)
    }

    fn send_command(&mut self, command: &Command) -> Result<(), Error> {
        self.serial_send_command(command, COMMAND_TIMEOUT)
    }

    fn receive_response(&mut self, timeout: Duration) -> Result<Response, Error> {
        if let Some(response) = self.serial_process_incoming_data(DataType::Response, timeout)? {
            return Ok(response);
        }
        Err(Error::new("Command response timeout"))
    }
}

impl Link for SerialLink {
    fn execute_command(&mut self, command: &Command) -> Result<Vec<u8>, Error> {
        self.execute_command_raw(command, COMMAND_TIMEOUT, false, false)
    }

    fn execute_command_raw(
        &mut self,
        command: &Command,
        timeout: Duration,
        no_response: bool,
        ignore_error: bool,
    ) -> Result<Vec<u8>, Error> {
        self.send_command(command)?;
        if no_response {
            return Ok(vec![]);
        }
        let response = self.receive_response(timeout)?;
        if command.id != response.id {
            return Err(Error::new("Command response ID didn't match"));
        }
        if !ignore_error && response.error {
            return Err(Error::new("Command response error"));
        }
        Ok(response.data)
    }

    fn receive_packet(&mut self) -> Result<Option<Packet>, Error> {
        if self.packets.len() == 0 {
            self.serial_process_incoming_data(DataType::Packet, PACKET_TIMEOUT)?;
        }
        Ok(self.packets.pop_front())
    }
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

pub fn new_local(port: &str) -> Result<Box<dyn Link>, Error> {
    let mut link = SerialLink {
        serial: serialport::new(port, 115_200).open()?,
        packets: VecDeque::new(),
    };

    link.reset()?;

    Ok(Box::new(link))
}
