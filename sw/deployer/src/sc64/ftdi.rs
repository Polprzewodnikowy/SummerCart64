pub struct FtdiError {
    pub description: String,
}

impl FtdiError {
    fn malloc() -> FtdiError {
        FtdiError {
            description: format!("Couldn't allocate memory for the context"),
        }
    }

    fn libftdi(context: *mut libftdi1_sys::ftdi_context) -> FtdiError {
        let raw = unsafe { std::ffi::CStr::from_ptr(libftdi1_sys::ftdi_get_error_string(context)) };
        FtdiError {
            description: format!("{}", raw.to_str().unwrap_or("Unknown error")),
        }
    }
}

impl From<FtdiError> for std::io::Error {
    fn from(value: FtdiError) -> Self {
        return Self::new(std::io::ErrorKind::Other, value.description);
    }
}

struct Context {
    context: *mut libftdi1_sys::ftdi_context,
}

impl Context {
    fn new() -> Result<Context, FtdiError> {
        let ctx = unsafe { libftdi1_sys::ftdi_new() };
        if ctx.is_null() {
            return Err(FtdiError::malloc());
        }
        let context = Context { context: ctx };
        let result = unsafe {
            libftdi1_sys::ftdi_set_interface(
                context.get(),
                libftdi1_sys::ftdi_interface::INTERFACE_A,
            )
        };
        context.check_result(result)?;
        Ok(context)
    }

    fn get(&self) -> *mut libftdi1_sys::ftdi_context {
        return self.context;
    }

    fn check_result(&self, result: i32) -> Result<(), FtdiError> {
        if result < 0 {
            return Err(FtdiError::libftdi(self.get()));
        }
        Ok(())
    }
}

impl Drop for Context {
    fn drop(&mut self) {
        unsafe { libftdi1_sys::ftdi_free(self.get()) }
    }
}

pub struct FtdiDevice {
    context: Context,
    read_timeout: std::time::Duration,
    write_timeout: std::time::Duration,
}

impl FtdiDevice {
    pub fn open(
        port: &str,
        read_timeout: std::time::Duration,
        write_timeout: std::time::Duration,
    ) -> Result<FtdiDevice, FtdiError> {
        let context = Context::new()?;
        unsafe {
            let mode = libftdi1_sys::ftdi_module_detach_mode::AUTO_DETACH_REATACH_SIO_MODULE;
            (*context.get()).module_detach_mode = mode;
        }
        let description = std::ffi::CString::new(port).unwrap_or_default().into_raw();
        let result = unsafe { libftdi1_sys::ftdi_usb_open_string(context.get(), description) };
        context.check_result(result)?;
        let result = unsafe { libftdi1_sys::ftdi_set_latency_timer(context.get(), 1) };
        context.check_result(result)?;
        Ok(FtdiDevice {
            context,
            read_timeout,
            write_timeout,
        })
    }

    pub fn set_dtr(&self, value: bool) -> Result<(), FtdiError> {
        let state = if value { 1 } else { 0 };
        let result = unsafe { libftdi1_sys::ftdi_setdtr(self.context.get(), state) };
        self.context.check_result(result)?;
        Ok(())
    }

    pub fn read_dsr(&self) -> Result<bool, FtdiError> {
        const DSR_BIT: u16 = 1 << 5;
        let mut status: u16 = 0;
        let result = unsafe {
            libftdi1_sys::ftdi_poll_modem_status(
                self.context.get(),
                std::slice::from_mut(&mut status).as_mut_ptr(),
            )
        };
        self.context.check_result(result)?;
        Ok((status & DSR_BIT) != 0)
    }

    pub fn read(&self, data: &mut [u8]) -> std::io::Result<usize> {
        let timeout = std::time::Instant::now();
        loop {
            let result = unsafe {
                libftdi1_sys::ftdi_read_data(
                    self.context.get(),
                    data.as_mut_ptr(),
                    data.len() as i32,
                )
            };
            self.context.check_result(result)?;
            if result > 0 {
                return Ok(result as usize);
            }
            if timeout.elapsed() > self.read_timeout {
                return Err(std::io::ErrorKind::TimedOut.into());
            }
        }
    }

    pub fn write(&self, data: &[u8]) -> std::io::Result<usize> {
        let timeout = std::time::Instant::now();
        loop {
            let result = unsafe {
                libftdi1_sys::ftdi_write_data(self.context.get(), data.as_ptr(), data.len() as i32)
            };
            self.context.check_result(result)?;
            if result > 0 {
                return Ok(result as usize);
            }
            if timeout.elapsed() > self.write_timeout {
                return Err(std::io::ErrorKind::TimedOut.into());
            }
        }
    }

    pub fn write_all(&self, data: &[u8]) -> std::io::Result<()> {
        let mut data = data;
        while !data.is_empty() {
            let written = self.write(data)?;
            data = &data[written..];
        }
        Ok(())
    }

    pub fn discard_buffers(&self) -> std::io::Result<()> {
        let result = unsafe { libftdi1_sys::ftdi_tcioflush(self.context.get()) };
        self.context.check_result(result)?;
        Ok(())
    }
}

impl Drop for FtdiDevice {
    fn drop(&mut self) {
        unsafe { libftdi1_sys::ftdi_usb_close(self.context.get()) };
    }
}

pub struct FtdiDeviceInfo {
    pub port: String,
    pub serial: String,
}

pub fn list_ftdi_devices(vendor: u16, product: u16) -> Result<Vec<FtdiDeviceInfo>, FtdiError> {
    let context = Context::new()?;

    let mut device_list: *mut libftdi1_sys::ftdi_device_list = std::ptr::null_mut();
    let result = unsafe {
        libftdi1_sys::ftdi_usb_find_all(
            context.get(),
            &mut device_list,
            vendor as i32,
            product as i32,
        )
    };
    context.check_result(result)?;

    let mut list: Vec<FtdiDeviceInfo> = vec![];

    let mut serial = [0i8; 128];

    let mut device = device_list;
    let mut index = 0;
    while !device.is_null() {
        let result = unsafe {
            libftdi1_sys::ftdi_usb_get_strings(
                context.get(),
                (*device).dev,
                std::ptr::null_mut(),
                0,
                std::ptr::null_mut(),
                0,
                serial.as_mut_ptr(),
                serial.len() as i32,
            )
        };

        if let Ok(()) = context.check_result(result) {
            list.push(FtdiDeviceInfo {
                port: format!("i:0x{vendor:04X}:0x{product:04X}:{index}"),
                serial: unsafe { std::ffi::CStr::from_ptr(serial.as_ptr()) }
                    .to_string_lossy()
                    .into_owned(),
            });
        }

        device = unsafe { (*device).next };
        index += 1;
    }

    unsafe { libftdi1_sys::ftdi_list_free(&mut device_list) }

    Ok(list)
}
