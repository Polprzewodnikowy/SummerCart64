pub struct DeviceInfo {
    pub description: String,
    pub serial: String,
    pub port: String,
}

pub struct SerialDevice {
    serial: serial2::SerialPort,
    writer: std::io::BufWriter<serial2::SerialPort>,
    unclog_buffer: std::collections::VecDeque<u8>,
    poll_timeout: std::time::Duration,
    io_timeout: std::time::Duration,
}

impl SerialDevice {
    const DEFAULT_POLL_TIMEOUT: std::time::Duration = std::time::Duration::from_millis(16);
    const DEFAULT_RW_TIMEOUT: std::time::Duration = std::time::Duration::from_secs(5);
    const WRITE_CHUNK_TIMEOUT: std::time::Duration = std::time::Duration::from_millis(100);
    const BUFFER_SIZE: usize = 16 * 1024;

    pub fn new(
        port: &str,
        poll_timeout: Option<std::time::Duration>,
        io_timeout: Option<std::time::Duration>,
    ) -> std::io::Result<Self> {
        let serial = serial2::SerialPort::open(port, 115_200)?;
        let writer = std::io::BufWriter::with_capacity(Self::BUFFER_SIZE, serial.try_clone()?);
        let mut device = Self {
            serial,
            writer,
            unclog_buffer: std::collections::VecDeque::new(),
            poll_timeout: poll_timeout.unwrap_or(Self::DEFAULT_POLL_TIMEOUT),
            io_timeout: io_timeout.unwrap_or(Self::DEFAULT_RW_TIMEOUT),
        };
        device.serial.set_read_timeout(device.poll_timeout)?;
        device.serial.set_write_timeout(Self::WRITE_CHUNK_TIMEOUT)?;
        Ok(device)
    }

    pub fn list(vendor: u16, product: u16) -> std::io::Result<Vec<DeviceInfo>> {
        let mut devices = vec![];

        for port in serialport::available_ports()? {
            if let serialport::SerialPortType::UsbPort(info) = port.port_type {
                if info.vid == vendor && info.pid == product {
                    devices.push(DeviceInfo {
                        description: info.product.unwrap_or_default(),
                        serial: info.serial_number.unwrap_or_default(),
                        port: port.port_name,
                    })
                }
            }
        }

        devices.sort_by(|a, b| a.serial.cmp(&b.serial));

        Ok(devices)
    }

    pub fn set_dtr(&mut self, value: bool) -> std::io::Result<()> {
        self.serial.set_dtr(value)
    }

    pub fn read_dsr(&mut self) -> std::io::Result<bool> {
        self.serial.read_dsr()
    }

    pub fn discard_input(&mut self) -> std::io::Result<()> {
        let timeout = std::time::Instant::now();
        self.serial.discard_input_buffer()?;
        loop {
            match self.serial.read(&mut vec![0u8; Self::BUFFER_SIZE]) {
                Ok(_) => {}
                Err(error) => match error.kind() {
                    std::io::ErrorKind::Interrupted
                    | std::io::ErrorKind::TimedOut
                    | std::io::ErrorKind::WouldBlock => {}
                    _ => return Err(error),
                },
            };
            if timeout.elapsed() > std::time::Duration::from_millis(1) {
                return Ok(());
            }
        }
    }

    pub fn discard_output(&mut self) -> std::io::Result<()> {
        self.serial.discard_output_buffer()
    }

    fn unclog_pipe(&mut self) -> std::io::Result<()> {
        let mut buffer = vec![0u8; Self::BUFFER_SIZE];
        let read = match self.serial.read(&mut buffer) {
            Ok(read) => read,
            Err(error) => match error.kind() {
                std::io::ErrorKind::Interrupted
                | std::io::ErrorKind::TimedOut
                | std::io::ErrorKind::WouldBlock => 0,
                _ => return Err(error),
            },
        };
        self.unclog_buffer.extend(buffer[0..read].iter());
        Ok(())
    }
}

impl std::io::Read for SerialDevice {
    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        if buffer.is_empty() {
            Err(std::io::ErrorKind::InvalidInput.into())
        } else if self.unclog_buffer.is_empty() {
            self.serial.read(buffer)
        } else {
            for (index, item) in buffer.iter_mut().enumerate() {
                if let Some(byte) = self.unclog_buffer.pop_front() {
                    *item = byte;
                } else {
                    return Ok(index);
                }
            }
            Ok(buffer.len())
        }
    }
}

impl std::io::Write for SerialDevice {
    fn write(&mut self, buffer: &[u8]) -> std::io::Result<usize> {
        let timeout = std::time::Instant::now();
        loop {
            match self.writer.write(buffer) {
                Ok(bytes) => return Ok(bytes),
                Err(error) => match error.kind() {
                    std::io::ErrorKind::TimedOut => self.unclog_pipe()?,
                    _ => return Err(error),
                },
            };
            if timeout.elapsed() > self.io_timeout {
                return Err(std::io::ErrorKind::TimedOut.into());
            }
        }
    }

    fn flush(&mut self) -> std::io::Result<()> {
        let timeout = std::time::Instant::now();
        loop {
            match self.writer.flush() {
                Ok(()) => return Ok(()),
                Err(error) => match error.kind() {
                    std::io::ErrorKind::TimedOut => self.unclog_pipe()?,
                    _ => return Err(error),
                },
            };
            if timeout.elapsed() > self.io_timeout {
                return Err(std::io::ErrorKind::TimedOut.into());
            }
        }
    }
}
