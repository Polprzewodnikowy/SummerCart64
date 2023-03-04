mod cic;
mod error;
pub mod firmware;
mod link;
mod types;
mod utils;

pub use self::{
    error::Error,
    link::list_serial_devices,
    types::{
        BootMode, ButtonMode, ButtonState, CicSeed, DataPacket, DdDiskState, DdDriveType, DdMode,
        DebugPacket, DiskPacket, FpgaDebugData, McuStackUsage, SaveType, Switch, TvType,
    },
};

use self::{
    cic::{calculate_ipl3_checksum, guess_ipl3_seed, IPL3_LENGTH, IPL3_OFFSET},
    link::{Command, Link},
    types::{
        get_config, get_setting, Config, ConfigId, FirmwareStatus, Setting, SettingId, UpdateStatus,
    },
    utils::{args_from_vec, datetime_from_vec, u32_from_vec, vec_from_datetime},
};
use chrono::{DateTime, Local};
use std::{
    io::{Read, Seek},
    time::Instant,
    {cmp::min, time::Duration},
};

pub struct SC64 {
    link: Box<dyn Link>,
}

pub struct DeviceState {
    pub bootloader_switch: Switch,
    pub rom_write_enable: Switch,
    pub rom_shadow_enable: Switch,
    pub dd_mode: DdMode,
    pub isv_address: u32,
    pub boot_mode: BootMode,
    pub save_type: SaveType,
    pub cic_seed: CicSeed,
    pub tv_type: TvType,
    pub dd_sd_enable: Switch,
    pub dd_drive_type: DdDriveType,
    pub dd_disk_state: DdDiskState,
    pub button_state: ButtonState,
    pub button_mode: ButtonMode,
    pub rom_extended_enable: Switch,
    pub led_enable: Switch,
    pub datetime: DateTime<Local>,
    pub fpga_debug_data: FpgaDebugData,
    pub mcu_stack_usage: McuStackUsage,
}

const SC64_V2_IDENTIFIER: &[u8; 4] = b"SCv2";

const SUPPORTED_MAJOR_VERSION: u16 = 2;
const SUPPORTED_MINOR_VERSION: u16 = 12;

const SDRAM_ADDRESS: u32 = 0x0000_0000;
const SDRAM_LENGTH: usize = 64 * 1024 * 1024;

const ROM_SHADOW_ADDRESS: u32 = 0x04FE_0000;
const ROM_SHADOW_LENGTH: usize = 128 * 1024;

const ROM_EXTENDED_ADDRESS: u32 = 0x0400_0000;
const ROM_EXTENDED_LENGTH: usize = 14 * 1024 * 1024;

const MAX_ROM_LENGTH: usize = 78 * 1024 * 1024;

const DDIPL_ADDRESS: u32 = 0x03BC_0000;
const DDIPL_LENGTH: usize = 4 * 1024 * 1024;

const SAVE_ADDRESS: u32 = 0x03FE_0000;
const EEPROM_ADDRESS: u32 = 0x0500_2000;

const EEPROM_4K_LENGTH: usize = 512;
const EEPROM_16K_LENGTH: usize = 2 * 1024;
const SRAM_LENGTH: usize = 32 * 1024;
const SRAM_BANKED_LENGTH: usize = 3 * 32 * 1024;
const FLASHRAM_LENGTH: usize = 128 * 1024;

const BOOTLOADER_ADDRESS: u32 = 0x04E0_0000;
const BOOTLOADER_LENGTH: usize = 1920 * 1024;

const FIRMWARE_ADDRESS: u32 = 0x0010_0000; // Arbitrary offset in SDRAM memory
const FIRMWARE_COMMAND_TIMEOUT: Duration = Duration::from_secs(30);
const FIRMWARE_UPDATE_TIMEOUT: Duration = Duration::from_secs(90);

const COMMAND_USB_WRITE_TIMEOUT: Duration = Duration::from_secs(5);

const ISV_BUFFER_LENGTH: usize = 64 * 1024;

pub const MEMORY_LENGTH: usize = 0x0500_2980;

const MEMORY_WRITE_CHUNK_LENGTH: usize = 1 * 1024 * 1024;

impl SC64 {
    fn command_identifier_get(&mut self) -> Result<Vec<u8>, Error> {
        let identifier = self.link.execute_command(&mut Command {
            id: b'v',
            args: [0, 0],
            data: vec![],
        })?;
        Ok(identifier)
    }

    fn command_version_get(&mut self) -> Result<(u16, u16), Error> {
        let version = self.link.execute_command(&mut Command {
            id: b'V',
            args: [0, 0],
            data: vec![],
        })?;
        let major = utils::u16_from_vec(&version[0..2])?;
        let minor = utils::u16_from_vec(&version[2..4])?;
        Ok((major, minor))
    }

    fn command_state_reset(&mut self) -> Result<(), Error> {
        self.link.execute_command(&mut Command {
            id: b'R',
            args: [0, 0],
            data: vec![],
        })?;
        Ok(())
    }

    fn command_cic_params_set(
        &mut self,
        disable: bool,
        seed: u8,
        checksum: &[u8; 6],
    ) -> Result<(), Error> {
        let mut params: Vec<u8> = vec![];
        params.append(&mut [(disable as u8) << 0, seed].to_vec());
        params.append(&mut checksum.to_vec());
        self.link.execute_command(&mut Command {
            id: b'B',
            args: args_from_vec(&params[0..8])?,
            data: vec![],
        })?;
        Ok(())
    }

    fn command_config_get(&mut self, config_id: ConfigId) -> Result<Config, Error> {
        let data = self.link.execute_command(&mut Command {
            id: b'c',
            args: [config_id.into(), 0],
            data: vec![],
        })?;
        let value = u32_from_vec(&data[0..4])?;
        Ok((config_id, value).try_into()?)
    }

    fn command_config_set(&mut self, config: Config) -> Result<(), Error> {
        self.link.execute_command(&mut Command {
            id: b'C',
            args: config.into(),
            data: vec![],
        })?;
        Ok(())
    }

    fn command_setting_get(&mut self, setting_id: SettingId) -> Result<Setting, Error> {
        let data = self.link.execute_command(&mut Command {
            id: b'a',
            args: [setting_id.into(), 0],
            data: vec![],
        })?;
        let value = u32_from_vec(&data[0..4])?;
        Ok((setting_id, value).try_into()?)
    }

    fn command_setting_set(&mut self, setting: Setting) -> Result<(), Error> {
        self.link.execute_command(&mut Command {
            id: b'A',
            args: setting.into(),
            data: vec![],
        })?;
        Ok(())
    }

    fn command_time_get(&mut self) -> Result<DateTime<Local>, Error> {
        let data = self.link.execute_command(&mut Command {
            id: b't',
            args: [0, 0],
            data: vec![],
        })?;
        Ok(datetime_from_vec(&data[0..8])?)
    }

    fn command_time_set(&mut self, datetime: DateTime<Local>) -> Result<(), Error> {
        self.link.execute_command(&mut Command {
            id: b'T',
            args: args_from_vec(&vec_from_datetime(datetime)?[0..8])?,
            data: vec![],
        })?;
        Ok(())
    }

    fn command_memory_read(&mut self, address: u32, length: usize) -> Result<Vec<u8>, Error> {
        let data = self.link.execute_command(&mut Command {
            id: b'm',
            args: [address, length as u32],
            data: vec![],
        })?;
        Ok(data)
    }

    fn command_memory_write(&mut self, address: u32, data: &[u8]) -> Result<(), Error> {
        self.link.execute_command(&mut Command {
            id: b'M',
            args: [address, data.len() as u32],
            data: data.to_vec(),
        })?;
        Ok(())
    }

    fn command_usb_write(&mut self, datatype: u8, data: &[u8]) -> Result<(), Error> {
        self.link.execute_command_raw(
            &mut Command {
                id: b'U',
                args: [datatype as u32, data.len() as u32],
                data: data.to_vec(),
            },
            COMMAND_USB_WRITE_TIMEOUT,
            true,
            false,
        )?;
        Ok(())
    }

    fn command_dd_set_block_ready(&mut self, error: bool) -> Result<(), Error> {
        self.link.execute_command(&mut Command {
            id: b'D',
            args: [error as u32, 0],
            data: vec![],
        })?;
        Ok(())
    }

    fn command_flash_wait_busy(&mut self, wait: bool) -> Result<u32, Error> {
        let erase_block_size = self.link.execute_command(&mut Command {
            id: b'p',
            args: [wait as u32, 0],
            data: vec![],
        })?;
        Ok(utils::u32_from_vec(&erase_block_size[0..4])?)
    }

    fn command_flash_erase_block(&mut self, address: u32) -> Result<(), Error> {
        self.link.execute_command(&mut Command {
            id: b'P',
            args: [address, 0],
            data: vec![],
        })?;
        Ok(())
    }

    fn command_firmware_backup(&mut self, address: u32) -> Result<(FirmwareStatus, u32), Error> {
        let data = self.link.execute_command_raw(
            &mut Command {
                id: b'f',
                args: [address, 0],
                data: vec![],
            },
            FIRMWARE_COMMAND_TIMEOUT,
            false,
            true,
        )?;
        let status = FirmwareStatus::try_from(utils::u32_from_vec(&data[0..4])?)?;
        let length = utils::u32_from_vec(&data[4..8])?;
        Ok((status, length))
    }

    fn command_firmware_update(
        &mut self,
        address: u32,
        length: usize,
    ) -> Result<FirmwareStatus, Error> {
        let data = self.link.execute_command_raw(
            &mut Command {
                id: b'F',
                args: [address, length as u32],
                data: vec![],
            },
            FIRMWARE_COMMAND_TIMEOUT,
            false,
            true,
        )?;
        Ok(FirmwareStatus::try_from(utils::u32_from_vec(&data[0..4])?)?)
    }

    fn command_debug_get(&mut self) -> Result<FpgaDebugData, Error> {
        self.link
            .execute_command(&mut Command {
                id: b'?',
                args: [0, 0],
                data: vec![],
            })?
            .try_into()
    }

    fn command_stack_usage_get(&mut self) -> Result<McuStackUsage, Error> {
        self.link
            .execute_command(&mut Command {
                id: b'%',
                args: [0, 0],
                data: vec![],
            })?
            .try_into()
    }
}

impl SC64 {
    pub fn upload_rom<T: Read + Seek>(
        &mut self,
        reader: &mut T,
        length: usize,
        no_shadow: bool,
    ) -> Result<(), Error> {
        if length > MAX_ROM_LENGTH {
            return Err(Error::new("ROM length too big"));
        }

        let mut pi_config = vec![0u8; 4];
        reader.read(&mut pi_config)?;
        reader.rewind()?;

        let endian_swapper = match &pi_config[0..4] {
            [0x37, 0x80, 0x40, 0x12] => {
                |b: &mut [u8]| b.chunks_exact_mut(2).for_each(|c| c.swap(0, 1))
            }
            [0x40, 0x12, 0x37, 0x80] => |b: &mut [u8]| {
                b.chunks_exact_mut(4).for_each(|c| {
                    c.swap(0, 3);
                    c.swap(1, 2)
                })
            },
            _ => |_: &mut [u8]| {},
        };

        let rom_shadow_enabled = !no_shadow && length > (SDRAM_LENGTH - ROM_SHADOW_LENGTH);
        let rom_extended_enabled = length > SDRAM_LENGTH;

        let sdram_length = if rom_shadow_enabled {
            min(length, SDRAM_LENGTH - ROM_SHADOW_LENGTH)
        } else {
            min(length, SDRAM_LENGTH)
        };

        self.memory_write_chunked(reader, SDRAM_ADDRESS, sdram_length, Some(endian_swapper))?;

        self.command_config_set(Config::RomShadowEnable(rom_shadow_enabled.into()))?;
        if rom_shadow_enabled {
            let rom_shadow_length = min(length - sdram_length, ROM_SHADOW_LENGTH);
            self.flash_program(
                reader,
                ROM_SHADOW_ADDRESS,
                rom_shadow_length,
                Some(endian_swapper),
            )?;
        }

        self.command_config_set(Config::RomExtendedEnable(rom_extended_enabled.into()))?;
        if rom_extended_enabled {
            let rom_extended_length = min(length - SDRAM_LENGTH, ROM_EXTENDED_LENGTH);
            self.flash_program(
                reader,
                ROM_EXTENDED_ADDRESS,
                rom_extended_length,
                Some(endian_swapper),
            )?;
        }

        Ok(())
    }

    pub fn upload_ddipl<T: Read>(&mut self, reader: &mut T, length: usize) -> Result<(), Error> {
        if length > DDIPL_LENGTH {
            return Err(Error::new("DDIPL length too big"));
        }

        self.memory_write_chunked(reader, DDIPL_ADDRESS, length, None)
    }

    pub fn upload_save<T: Read>(&mut self, reader: &mut T, length: usize) -> Result<(), Error> {
        let save_type = get_config!(self, SaveType)?;

        let (address, save_length) = match save_type {
            SaveType::None => {
                return Err(Error::new("No save type is enabled"));
            }
            SaveType::Eeprom4k => (EEPROM_ADDRESS, EEPROM_4K_LENGTH),
            SaveType::Eeprom16k => (EEPROM_ADDRESS, EEPROM_16K_LENGTH),
            SaveType::Sram => (SAVE_ADDRESS, SRAM_LENGTH),
            SaveType::SramBanked => (SAVE_ADDRESS, SRAM_BANKED_LENGTH),
            SaveType::Flashram => (SAVE_ADDRESS, FLASHRAM_LENGTH),
        };

        if length != save_length {
            return Err(Error::new(
                "Save file size did not match currently enabled save type",
            ));
        }

        self.memory_write_chunked(reader, address, save_length, None)
    }

    pub fn dump_memory(&mut self, address: u32, length: usize) -> Result<Vec<u8>, Error> {
        if address + length as u32 > MEMORY_LENGTH as u32 {
            return Err(Error::new("Invalid dump address or length"));
        }
        self.command_memory_read(address, length)
    }

    pub fn calculate_cic_parameters(&mut self) -> Result<(), Error> {
        let boot_mode = get_config!(self, BootMode)?;
        let address = match boot_mode {
            BootMode::DirectRom => SDRAM_ADDRESS,
            BootMode::DirectDdIpl => DDIPL_ADDRESS,
            _ => BOOTLOADER_ADDRESS,
        };
        let ipl3 = self.command_memory_read(address + IPL3_OFFSET, IPL3_LENGTH)?;
        let seed = guess_ipl3_seed(&ipl3)?;
        let checksum = &calculate_ipl3_checksum(&ipl3, seed)?;
        self.command_cic_params_set(false, seed, checksum)
    }

    pub fn set_boot_mode(&mut self, boot_mode: BootMode) -> Result<(), Error> {
        self.command_config_set(Config::BootMode(boot_mode))
    }

    pub fn set_save_type(&mut self, save_type: SaveType) -> Result<(), Error> {
        self.command_config_set(Config::SaveType(save_type))
    }

    pub fn set_tv_type(&mut self, tv_type: TvType) -> Result<(), Error> {
        self.command_config_set(Config::TvType(tv_type))
    }

    pub fn get_datetime(&mut self) -> Result<DateTime<Local>, Error> {
        self.command_time_get()
    }

    pub fn set_datetime(&mut self, datetime: DateTime<Local>) -> Result<(), Error> {
        self.command_time_set(datetime)
    }

    pub fn set_led_blink(&mut self, enabled: bool) -> Result<(), Error> {
        self.command_setting_set(Setting::LedEnable(enabled.into()))
    }

    pub fn get_device_state(&mut self) -> Result<DeviceState, Error> {
        Ok(DeviceState {
            bootloader_switch: get_config!(self, BootloaderSwitch)?,
            rom_write_enable: get_config!(self, RomWriteEnable)?,
            rom_shadow_enable: get_config!(self, RomShadowEnable)?,
            dd_mode: get_config!(self, DdMode)?,
            isv_address: get_config!(self, IsvAddress)?,
            boot_mode: get_config!(self, BootMode)?,
            save_type: get_config!(self, SaveType)?,
            cic_seed: get_config!(self, CicSeed)?,
            tv_type: get_config!(self, TvType)?,
            dd_sd_enable: get_config!(self, DdSdEnable)?,
            dd_drive_type: get_config!(self, DdDriveType)?,
            dd_disk_state: get_config!(self, DdDiskState)?,
            button_state: get_config!(self, ButtonState)?,
            button_mode: get_config!(self, ButtonMode)?,
            rom_extended_enable: get_config!(self, RomExtendedEnable)?,
            led_enable: get_setting!(self, LedEnable)?,
            datetime: self.get_datetime()?,
            fpga_debug_data: self.command_debug_get()?,
            mcu_stack_usage: self.command_stack_usage_get()?,
        })
    }

    pub fn configure_64dd(
        &mut self,
        dd_mode: DdMode,
        drive_type: DdDriveType,
    ) -> Result<(), Error> {
        self.command_config_set(Config::DdMode(dd_mode))?;
        self.command_config_set(Config::DdSdEnable(Switch::Off))?;
        self.command_config_set(Config::DdDriveType(drive_type))?;
        self.command_config_set(Config::DdDiskState(DdDiskState::Ejected))?;
        Ok(())
    }

    pub fn set_64dd_disk_state(&mut self, disk_state: DdDiskState) -> Result<(), Error> {
        self.command_config_set(Config::DdDiskState(disk_state))
    }

    pub fn configure_is_viewer_64(&mut self, offset: Option<u32>) -> Result<(), Error> {
        if let Some(offset) = offset {
            if get_config!(self, RomShadowEnable)?.into() {
                if offset > (SAVE_ADDRESS - ISV_BUFFER_LENGTH as u32) {
                    return Err(Error::new(
                        format!(
                            "ROM shadow is enabled, IS-Viewer 64 at offset 0x{offset:08X} won't work"
                        )
                        .as_str(),
                    ));
                }
            }
            self.command_config_set(Config::RomWriteEnable(Switch::On))?;
            self.command_config_set(Config::IsvAddress(offset))?;
        } else {
            self.command_config_set(Config::RomWriteEnable(Switch::Off))?;
            self.command_config_set(Config::IsvAddress(0))?;
        }
        Ok(())
    }

    pub fn receive_data_packet(&mut self) -> Result<Option<DataPacket>, Error> {
        if let Some(packet) = self.link.receive_packet()? {
            return Ok(Some(packet.try_into()?));
        }
        Ok(None)
    }

    pub fn reply_disk_packet(&mut self, disk_packet: Option<DiskPacket>) -> Result<(), Error> {
        if let Some(packet) = disk_packet {
            match packet {
                DiskPacket::ReadBlock(disk_block) => {
                    self.command_memory_write(disk_block.address, &disk_block.data)?;
                }
                DiskPacket::WriteBlock(_) => {}
            }
            self.command_dd_set_block_ready(false)?;
        } else {
            self.command_dd_set_block_ready(true)?;
        }
        Ok(())
    }

    pub fn send_debug_packet(&mut self, debug_packet: DebugPacket) -> Result<(), Error> {
        self.command_usb_write(debug_packet.datatype, &debug_packet.data)
    }

    pub fn check_firmware_version(&mut self) -> Result<(u16, u16), Error> {
        let (major, minor) = self
            .command_version_get()
            .map_err(|_| Error::new("Outdated SC64 firmware version, please update firmware"))?;
        if major != SUPPORTED_MAJOR_VERSION || minor < SUPPORTED_MINOR_VERSION {
            return Err(Error::new(
                "Unsupported SC64 firmware version, please update firmware",
            ));
        }
        Ok((major, minor))
    }

    pub fn reset_state(&mut self) -> Result<(), Error> {
        self.command_state_reset()
    }

    pub fn backup_firmware(&mut self) -> Result<Vec<u8>, Error> {
        self.command_state_reset()?;
        let (status, length) = self.command_firmware_backup(FIRMWARE_ADDRESS)?;
        if !matches!(status, FirmwareStatus::Ok) {
            return Err(Error::new(
                format!("Firmware backup error: {}", status).as_str(),
            ));
        }
        self.command_memory_read(FIRMWARE_ADDRESS, length as usize)
    }

    pub fn update_firmware(&mut self, data: &[u8]) -> Result<(), Error> {
        self.command_state_reset()?;
        self.command_memory_write(FIRMWARE_ADDRESS, data)?;
        let status = self.command_firmware_update(FIRMWARE_ADDRESS, data.len())?;
        if !matches!(status, FirmwareStatus::Ok) {
            return Err(Error::new(
                format!("Firmware update verify error: {}", status).as_str(),
            ));
        }
        let timeout = Instant::now();
        let mut last_update_status = UpdateStatus::Err;
        loop {
            if let Some(packet) = self.receive_data_packet()? {
                if let DataPacket::UpdateStatus(status) = packet {
                    match status {
                        UpdateStatus::Done => {
                            std::thread::sleep(Duration::from_secs(2));
                            return Ok(());
                        }
                        UpdateStatus::Err => {
                            return Err(Error::new(
                                format!(
                                "Firmware update error on step {}, device is, most likely, bricked",
                                last_update_status
                            )
                                .as_str(),
                            ))
                        }
                        current_update_status => last_update_status = current_update_status,
                    }
                }
            }
            if timeout.elapsed() > FIRMWARE_UPDATE_TIMEOUT {
                return Err(Error::new(
                    format!(
                        "Firmware update timeout, SC64 did not finish update in {} seconds",
                        FIRMWARE_UPDATE_TIMEOUT.as_secs()
                    )
                    .as_str(),
                ));
            }
            std::thread::sleep(Duration::from_millis(1));
        }
    }

    pub fn update_bootloader(&mut self, reader: &mut dyn Read, length: usize) -> Result<(), Error> {
        if length > BOOTLOADER_LENGTH {
            return Err(Error::new("Bootloader length too big"));
        }
        self.flash_program(reader, BOOTLOADER_ADDRESS, length, None)
    }

    fn memory_write_chunked(
        &mut self,
        reader: &mut dyn Read,
        address: u32,
        length: usize,
        transform: Option<fn(&mut [u8])>,
    ) -> Result<(), Error> {
        let mut data: Vec<u8> = vec![0u8; MEMORY_WRITE_CHUNK_LENGTH];
        for offset in (0..length).step_by(MEMORY_WRITE_CHUNK_LENGTH) {
            let chunk = reader.read(&mut data)?;
            if let Some(transform) = transform {
                transform(&mut data);
            }
            self.command_memory_write(address + offset as u32, &data[0..chunk])?;
        }
        Ok(())
    }

    fn flash_erase(&mut self, address: u32, length: usize) -> Result<(), Error> {
        let erase_block_size = self.command_flash_wait_busy(false)?;
        for offset in (0..length as u32).step_by(erase_block_size as usize) {
            self.command_flash_erase_block(address + offset)?;
        }
        Ok(())
    }

    fn flash_program(
        &mut self,
        reader: &mut dyn Read,
        address: u32,
        length: usize,
        transform: Option<fn(&mut [u8])>,
    ) -> Result<(), Error> {
        self.flash_erase(address, length)?;
        self.memory_write_chunked(reader, address, length, transform)?;
        self.command_flash_wait_busy(true)?;
        Ok(())
    }
}

pub fn new(sn: Option<String>) -> Result<SC64, Error> {
    let port = match sn {
        Some(sn) => match list_serial_devices()?.iter().find(|d| d.sn == sn) {
            Some(device) => device.port.clone(),
            None => {
                return Err(Error::new(
                    "No SC64 device found matching provided serial number",
                ))
            }
        },
        None => list_serial_devices()?[0].port.clone(),
    };

    let mut sc64 = SC64 {
        link: link::new_serial(&port)?,
    };

    let identifier = sc64
        .command_identifier_get()
        .map_err(|_| Error::new("Couldn't get SC64 device identifier"))?;

    if identifier != SC64_V2_IDENTIFIER {
        return Err(Error::new("Unknown identifier received, not a SC64 device"));
    }

    Ok(sc64)
}
