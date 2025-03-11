pub struct DeviceInfo {
    pub description: String,
    pub serial: String,
    pub port: String,
}

#[allow(dead_code)]
enum InterfaceIndex {
    Any,
    A,
    B,
    C,
    D,
}

#[allow(dead_code)]
enum ModuleDetachMode {
    AutoDetach,
    DontDetach,
    AutoDetachReattach,
}

struct ModemStatus {
    dsr: bool,
}

struct Wrapper {
    context: *mut libftdi1_sys::ftdi_context,
    unclog_buffer: std::collections::VecDeque<u8>,
    write_buffer: Vec<u8>,
    io_timeout: std::time::Duration,
    read_chunksize: usize,
    write_chunksize: usize,
}

impl Wrapper {
    const DEFAULT_POLL_TIMEOUT: std::time::Duration = std::time::Duration::from_millis(16);
    const DEFAULT_IO_TIMEOUT: std::time::Duration = std::time::Duration::from_secs(5);
    const WRITE_CHUNK_TIMEOUT: std::time::Duration = std::time::Duration::from_millis(100);

    fn new(io_timeout: Option<std::time::Duration>) -> std::io::Result<Self> {
        let context = unsafe { libftdi1_sys::ftdi_new() };
        if context.is_null() {
            return Err(std::io::ErrorKind::OutOfMemory.into());
        }
        let mut wrapper = Self {
            context,
            unclog_buffer: std::collections::VecDeque::new(),
            write_buffer: vec![],
            io_timeout: Self::DEFAULT_IO_TIMEOUT,
            read_chunksize: 4096,
            write_chunksize: 4096,
        };
        wrapper.set_io_timeout(io_timeout)?;
        wrapper.read_data_set_chunksize(wrapper.read_chunksize)?;
        wrapper.write_data_set_chunksize(wrapper.write_chunksize)?;
        Ok(wrapper)
    }

    fn list_devices(vendor: u16, product: u16) -> std::io::Result<Vec<DeviceInfo>> {
        let wrapper = Self::new(None)?;

        let mut device_list: *mut libftdi1_sys::ftdi_device_list = std::ptr::null_mut();
        let devices = unsafe {
            libftdi1_sys::ftdi_usb_find_all(
                wrapper.context,
                &mut device_list,
                vendor as i32,
                product as i32,
            )
        };

        let result = if devices > 0 {
            let mut list: Vec<DeviceInfo> = vec![];

            let mut description = [std::ffi::c_char::from(0); 128];
            let mut serial = [std::ffi::c_char::from(0); 128];

            let mut device = device_list;
            let mut index = 0;
            while !device.is_null() {
                let result = unsafe {
                    libftdi1_sys::ftdi_usb_get_strings(
                        wrapper.context,
                        (*device).dev,
                        std::ptr::null_mut(),
                        0,
                        description.as_mut_ptr(),
                        description.len() as i32,
                        serial.as_mut_ptr(),
                        serial.len() as i32,
                    )
                };

                let description = unsafe { std::ffi::CStr::from_ptr(description.as_ptr()) }
                    .to_string_lossy()
                    .into_owned();
                let serial = unsafe { std::ffi::CStr::from_ptr(serial.as_ptr()) }
                    .to_string_lossy()
                    .into_owned();
                let port = if list.binary_search_by(|d| d.serial.cmp(&serial)).is_ok() {
                    format!("i:0x{vendor:04X}:0x{product:04X}:{index}")
                } else {
                    format!("s:0x{vendor:04X}:0x{product:04X}:{serial}")
                };

                if result == 0 {
                    list.push(DeviceInfo {
                        description,
                        serial,
                        port,
                    });
                }

                device = unsafe { (*device).next };
                index += 1;
            }

            list.sort_by(|a, b| a.serial.cmp(&b.serial));

            Ok(list)
        } else {
            match devices {
                0 => Ok(vec![]),
                -3 => Err(std::io::ErrorKind::OutOfMemory.into()),
                -5 => Err(std::io::ErrorKind::BrokenPipe.into()),
                -6 => Err(std::io::ErrorKind::BrokenPipe.into()),
                result => Err(std::io::Error::other(format!(
                    "Unexpected response from ftdi_usb_find_all: {result}"
                ))),
            }
        };

        unsafe { libftdi1_sys::ftdi_list_free(&mut device_list) }

        result
    }

    fn libusb_convert_result(&self, result: i32) -> std::io::Error {
        if result == libusb1_sys::constants::LIBUSB_ERROR_OVERFLOW {
            return std::io::Error::other("libusb overflow");
        }
        match result {
            libusb1_sys::constants::LIBUSB_ERROR_IO => std::io::ErrorKind::UnexpectedEof,
            libusb1_sys::constants::LIBUSB_ERROR_INVALID_PARAM => std::io::ErrorKind::InvalidInput,
            libusb1_sys::constants::LIBUSB_ERROR_ACCESS => std::io::ErrorKind::PermissionDenied,
            libusb1_sys::constants::LIBUSB_ERROR_NO_DEVICE => std::io::ErrorKind::NotConnected,
            libusb1_sys::constants::LIBUSB_ERROR_NOT_FOUND => std::io::ErrorKind::NotFound,
            libusb1_sys::constants::LIBUSB_ERROR_BUSY => std::io::ErrorKind::WouldBlock,
            libusb1_sys::constants::LIBUSB_ERROR_TIMEOUT => std::io::ErrorKind::TimedOut,
            libusb1_sys::constants::LIBUSB_ERROR_PIPE => std::io::ErrorKind::BrokenPipe,
            libusb1_sys::constants::LIBUSB_ERROR_INTERRUPTED => std::io::ErrorKind::Interrupted,
            libusb1_sys::constants::LIBUSB_ERROR_NO_MEM => std::io::ErrorKind::OutOfMemory,
            libusb1_sys::constants::LIBUSB_ERROR_NOT_SUPPORTED => std::io::ErrorKind::Unsupported,
            _ => std::io::ErrorKind::Other,
        }
        .into()
    }

    fn set_io_timeout(&mut self, io_timeout: Option<std::time::Duration>) -> std::io::Result<()> {
        let io_timeout = io_timeout.unwrap_or(Self::DEFAULT_IO_TIMEOUT);
        unsafe {
            (*self.context).usb_read_timeout = i32::try_from(io_timeout.as_millis())
                .map_err(|_| std::io::ErrorKind::InvalidInput)?;
            (*self.context).usb_write_timeout = i32::try_from(io_timeout.as_millis())
                .map_err(|_| std::io::ErrorKind::InvalidInput)?;
        }
        self.io_timeout = io_timeout;
        Ok(())
    }

    fn set_module_detach_mode(&mut self, mode: ModuleDetachMode) {
        let mode = match mode {
            ModuleDetachMode::AutoDetach => {
                libftdi1_sys::ftdi_module_detach_mode::AUTO_DETACH_SIO_MODULE
            }
            ModuleDetachMode::DontDetach => {
                libftdi1_sys::ftdi_module_detach_mode::DONT_DETACH_SIO_MODULE
            }
            ModuleDetachMode::AutoDetachReattach => {
                libftdi1_sys::ftdi_module_detach_mode::AUTO_DETACH_REATACH_SIO_MODULE
            }
        };
        unsafe {
            (*self.context).module_detach_mode = mode;
        };
    }

    fn set_interface(&mut self, interface: InterfaceIndex) -> std::io::Result<()> {
        let interface = match interface {
            InterfaceIndex::Any => libftdi1_sys::ftdi_interface::INTERFACE_ANY,
            InterfaceIndex::A => libftdi1_sys::ftdi_interface::INTERFACE_A,
            InterfaceIndex::B => libftdi1_sys::ftdi_interface::INTERFACE_B,
            InterfaceIndex::C => libftdi1_sys::ftdi_interface::INTERFACE_C,
            InterfaceIndex::D => libftdi1_sys::ftdi_interface::INTERFACE_D,
        };
        match unsafe { libftdi1_sys::ftdi_set_interface(self.context, interface) } {
            0 => Ok(()),
            -1 => Err(std::io::ErrorKind::InvalidInput.into()),
            -2 => Err(std::io::ErrorKind::NotConnected.into()),
            -3 => Err(std::io::ErrorKind::InvalidData.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_set_interface: {result}"
            ))),
        }
    }

    fn usb_open_string(&mut self, description: &str) -> std::io::Result<()> {
        let description = std::ffi::CString::new(description)
            .unwrap_or_default()
            .into_raw();
        match unsafe { libftdi1_sys::ftdi_usb_open_string(self.context, description) } {
            0 => Ok(()),
            -2 => Err(std::io::ErrorKind::ConnectionRefused.into()),
            -3 => Err(std::io::ErrorKind::NotFound.into()),
            -4 => Err(std::io::ErrorKind::PermissionDenied.into()),
            -5 => Err(std::io::ErrorKind::PermissionDenied.into()),
            -6 => Err(std::io::ErrorKind::ConnectionRefused.into()),
            -7 => Err(std::io::ErrorKind::ConnectionRefused.into()),
            -8 => Err(std::io::ErrorKind::ConnectionRefused.into()),
            -9 => Err(std::io::ErrorKind::ConnectionRefused.into()),
            -10 => Err(std::io::ErrorKind::BrokenPipe.into()),
            -11 => Err(std::io::ErrorKind::InvalidInput.into()),
            -12 => Err(std::io::ErrorKind::InvalidData.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_usb_open_string: {result}"
            ))),
        }
    }

    fn usb_reset(&mut self) -> std::io::Result<()> {
        match unsafe { libftdi1_sys::ftdi_usb_reset(self.context) } {
            0 => Ok(()),
            -1 => Err(std::io::ErrorKind::BrokenPipe.into()),
            -2 => Err(std::io::ErrorKind::NotConnected.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_usb_reset: {result}"
            ))),
        }
    }

    fn set_latency_timer(&mut self, latency: Option<std::time::Duration>) -> std::io::Result<()> {
        let latency = u8::try_from(latency.unwrap_or(Self::DEFAULT_POLL_TIMEOUT).as_millis())
            .map_err(|_| std::io::ErrorKind::InvalidInput)?;
        match unsafe { libftdi1_sys::ftdi_set_latency_timer(self.context, latency) } {
            0 => Ok(()),
            -1 => Err(std::io::ErrorKind::InvalidInput.into()),
            -2 => Err(std::io::ErrorKind::BrokenPipe.into()),
            -3 => Err(std::io::ErrorKind::NotConnected.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_set_latency_timer: {result}"
            ))),
        }
    }

    fn read_data_set_chunksize(&mut self, chunksize: usize) -> std::io::Result<()> {
        match unsafe {
            libftdi1_sys::ftdi_read_data_set_chunksize(
                self.context,
                u32::try_from(chunksize).map_err(|_| std::io::ErrorKind::InvalidInput)?,
            )
        } {
            0 => {
                self.read_chunksize = chunksize;
                Ok(())
            }
            -1 => Err(std::io::ErrorKind::NotConnected.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_read_data_set_chunksize: {result}"
            ))),
        }
    }

    fn write_data_set_chunksize(&mut self, chunksize: usize) -> std::io::Result<()> {
        match unsafe {
            libftdi1_sys::ftdi_write_data_set_chunksize(
                self.context,
                u32::try_from(chunksize).map_err(|_| std::io::ErrorKind::InvalidInput)?,
            )
        } {
            0 => {
                self.write_chunksize = chunksize;
                self.commit_write()
            }
            -1 => Err(std::io::ErrorKind::NotConnected.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_write_data_set_chunksize: {result}"
            ))),
        }
    }

    pub fn set_dtr(&mut self, value: bool) -> std::io::Result<()> {
        let state = if value { 1 } else { 0 };
        match unsafe { libftdi1_sys::ftdi_setdtr(self.context, state) } {
            0 => Ok(()),
            -1 => Err(std::io::ErrorKind::BrokenPipe.into()),
            -2 => Err(std::io::ErrorKind::NotConnected.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_setdtr: {result}"
            ))),
        }
    }

    fn poll_modem_status(&mut self) -> std::io::Result<ModemStatus> {
        const DSR_BIT: u16 = 1 << 5;

        let mut status = 0;

        match unsafe { libftdi1_sys::ftdi_poll_modem_status(self.context, &mut status) } {
            0 => Ok(ModemStatus {
                dsr: (status & DSR_BIT) != 0,
            }),
            -1 => Err(std::io::ErrorKind::BrokenPipe.into()),
            -2 => Err(std::io::ErrorKind::NotConnected.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_poll_modem_status: {result}"
            ))),
        }
    }

    fn tciflush(&mut self) -> std::io::Result<()> {
        match unsafe { libftdi1_sys::ftdi_tciflush(self.context) } {
            0 => Ok(()),
            -1 => Err(std::io::ErrorKind::BrokenPipe.into()),
            -2 => Err(std::io::ErrorKind::NotConnected.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_tciflush: {result}"
            ))),
        }?;
        let timeout = std::time::Instant::now();
        loop {
            match self.read(&mut vec![0u8; self.read_chunksize]) {
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

    fn tcoflush(&mut self) -> std::io::Result<()> {
        self.write_buffer.clear();
        match unsafe { libftdi1_sys::ftdi_tcoflush(self.context) } {
            0 => Ok(()),
            -1 => Err(std::io::ErrorKind::BrokenPipe.into()),
            -2 => Err(std::io::ErrorKind::NotConnected.into()),
            result => Err(std::io::Error::other(format!(
                "Unexpected response from ftdi_tcoflush: {result}"
            ))),
        }
    }

    pub fn read_data(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        let length = i32::try_from(buffer.len()).map_err(|_| std::io::ErrorKind::InvalidInput)?;
        let result =
            unsafe { libftdi1_sys::ftdi_read_data(self.context, buffer.as_mut_ptr(), length) };
        match result {
            1.. => Ok(result as usize),
            0 => Err(std::io::ErrorKind::WouldBlock.into()),
            -666 => Err(std::io::ErrorKind::NotConnected.into()),
            result => Err(self.libusb_convert_result(result)),
        }
    }

    fn write_data(&mut self, buffer: &[u8], written: &mut usize) -> std::io::Result<()> {
        let mut transferred = 0;
        let result = unsafe {
            // NOTE: Nasty hack to overcome libftdi1 API limitation.
            //       Write can partially succeed, but the default ftdi_write_data
            //       function doesn't report number of transferred bytes in that case.
            libusb1_sys::libusb_bulk_transfer(
                (*self.context).usb_dev,
                (*self.context).in_ep as u8,
                Vec::from(buffer).as_mut_ptr(),
                buffer.len() as i32,
                &mut transferred,
                Self::WRITE_CHUNK_TIMEOUT.as_millis() as u32,
            )
        };
        *written = transferred as usize;
        if result < 0 {
            return Err(self.libusb_convert_result(result));
        }
        Ok(())
    }

    fn unclog_pipe(&mut self) -> std::io::Result<()> {
        let mut buffer = vec![0u8; self.read_chunksize];
        let read = match self.read_data(&mut buffer) {
            Ok(read) => read,
            Err(error) => match error.kind() {
                std::io::ErrorKind::Interrupted | std::io::ErrorKind::WouldBlock => 0,
                _ => return Err(error),
            },
        };
        self.unclog_buffer.extend(buffer[0..read].iter());
        Ok(())
    }

    fn commit_write(&mut self) -> std::io::Result<()> {
        let timeout = std::time::Instant::now();
        while !self.write_buffer.is_empty() {
            let mut written = 0;
            let result = self.write_data(&self.write_buffer.clone(), &mut written);
            self.write_buffer.drain(..written);
            if let Err(error) = result {
                match error.kind() {
                    std::io::ErrorKind::TimedOut => self.unclog_pipe()?,
                    _ => return Err(error),
                }
            }
            if timeout.elapsed() > self.io_timeout {
                return Err(std::io::ErrorKind::TimedOut.into());
            }
        }
        Ok(())
    }

    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        if buffer.is_empty() {
            Err(std::io::ErrorKind::InvalidInput.into())
        } else if self.unclog_buffer.is_empty() {
            self.read_data(buffer)
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

    fn write(&mut self, buffer: &[u8]) -> std::io::Result<usize> {
        let remaining_space = self.write_chunksize - self.write_buffer.len();
        let length = buffer.len().min(remaining_space);
        self.write_buffer.extend(&buffer[..length]);
        if self.write_buffer.len() >= self.write_chunksize {
            self.commit_write()?
        }
        Ok(length)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.commit_write()
    }
}

impl Drop for Wrapper {
    fn drop(&mut self) {
        unsafe { libftdi1_sys::ftdi_free(self.context) }
    }
}

pub struct FtdiDevice {
    wrapper: Wrapper,
}

impl FtdiDevice {
    pub fn list(vendor: u16, product: u16) -> std::io::Result<Vec<DeviceInfo>> {
        Wrapper::list_devices(vendor, product)
    }

    pub fn open(
        description: &str,
        poll_timeout: Option<std::time::Duration>,
        io_timeout: Option<std::time::Duration>,
    ) -> std::io::Result<FtdiDevice> {
        let mut wrapper = Wrapper::new(io_timeout)?;

        wrapper.set_module_detach_mode(ModuleDetachMode::AutoDetachReattach);
        wrapper.set_interface(InterfaceIndex::A)?;

        const CHUNK_SIZE: usize = 1 * 1024 * 1024;

        wrapper.read_data_set_chunksize(CHUNK_SIZE)?;
        wrapper.write_data_set_chunksize(CHUNK_SIZE)?;

        wrapper.usb_open_string(description)?;

        wrapper.usb_reset()?;

        wrapper.set_latency_timer(poll_timeout)?;

        Ok(FtdiDevice { wrapper })
    }

    pub fn set_dtr(&mut self, value: bool) -> std::io::Result<()> {
        self.wrapper.set_dtr(value)
    }

    pub fn read_dsr(&mut self) -> std::io::Result<bool> {
        Ok(self.wrapper.poll_modem_status()?.dsr)
    }

    pub fn discard_input(&mut self) -> std::io::Result<()> {
        self.wrapper.tciflush()
    }

    pub fn discard_output(&mut self) -> std::io::Result<()> {
        self.wrapper.tcoflush()
    }
}

impl std::io::Read for FtdiDevice {
    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        self.wrapper.read(buffer)
    }
}

impl std::io::Write for FtdiDevice {
    fn write(&mut self, buffer: &[u8]) -> std::io::Result<usize> {
        self.wrapper.write(buffer)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.wrapper.flush()
    }
}

impl Drop for FtdiDevice {
    fn drop(&mut self) {
        unsafe { libftdi1_sys::ftdi_usb_close(self.wrapper.context) };
    }
}
