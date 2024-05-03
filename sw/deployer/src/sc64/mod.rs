mod cic;
mod error;
pub mod firmware;
mod link;
pub mod server;
mod time;
mod types;

pub use self::{
    error::Error,
    link::list_local_devices,
    server::ServerEvent,
    types::{
        BootMode, ButtonMode, ButtonState, CicSeed, DataPacket, DdDiskState, DdDriveType, DdMode,
        DebugPacket, DiagnosticData, DiskPacket, DiskPacketKind, FpgaDebugData, SaveType,
        SaveWriteback, Switch, TvType,
    },
};

use self::{
    cic::{sign_ipl3, IPL3_LENGTH, IPL3_OFFSET},
    link::{Command, Link},
    time::{convert_from_datetime, convert_to_datetime},
    types::{
        get_config, get_setting, Config, ConfigId, FirmwareStatus, Setting, SettingId, UpdateStatus,
    },
};
use chrono::{DateTime, Local};
use std::{
    io::{Read, Seek, Write},
    time::Instant,
    {cmp::min, time::Duration},
};

pub struct SC64 {
    link: Link,
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
    pub diagnostic_data: DiagnosticData,
}

const SC64_V2_IDENTIFIER: &[u8; 4] = b"SCv2";

const SUPPORTED_MAJOR_VERSION: u16 = 2;
const SUPPORTED_MINOR_VERSION: u16 = 18;

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
const FLASHRAM_LENGTH: usize = 128 * 1024;
const SRAM_BANKED_LENGTH: usize = 3 * 32 * 1024;
const SRAM_1M_LENGTH: usize = 128 * 1024;

const BOOTLOADER_ADDRESS: u32 = 0x04E0_0000;

const FIRMWARE_ADDRESS_SDRAM: u32 = 0x0010_0000; // Arbitrary offset in SDRAM memory
const FIRMWARE_ADDRESS_FLASH: u32 = 0x0410_0000; // Arbitrary offset in Flash memory
const FIRMWARE_UPDATE_TIMEOUT: Duration = Duration::from_secs(90);

const ISV_BUFFER_LENGTH: usize = 64 * 1024;

pub const MEMORY_LENGTH: usize = 0x0500_2980;

const MEMORY_CHUNK_LENGTH: usize = 1 * 1024 * 1024;

impl SC64 {
    fn command_identifier_get(&mut self) -> Result<[u8; 4], Error> {
        let data = self.link.execute_command(&Command {
            id: b'v',
            args: [0, 0],
            data: vec![],
        })?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for identifier get command",
            ));
        }
        Ok(data[0..4].try_into().unwrap())
    }

    fn command_version_get(&mut self) -> Result<(u16, u16, u32), Error> {
        let data = self.link.execute_command(&Command {
            id: b'V',
            args: [0, 0],
            data: vec![],
        })?;
        if data.len() != 8 {
            return Err(Error::new(
                "Invalid data length received for version get command",
            ));
        }
        let major = u16::from_be_bytes(data[0..2].try_into().unwrap());
        let minor = u16::from_be_bytes(data[2..4].try_into().unwrap());
        let revision = u32::from_be_bytes(data[4..8].try_into().unwrap());
        Ok((major, minor, revision))
    }

    fn command_state_reset(&mut self) -> Result<(), Error> {
        self.link.execute_command(&Command {
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
        checksum: u64,
    ) -> Result<(), Error> {
        let checksum_high = ((checksum >> 32) & 0xFFFF) as u32;
        let checksum_low = (checksum & 0xFFFFFFFF) as u32;
        let args = [
            ((if disable { 1 } else { 0 }) << 24) | ((seed as u32) << 16) | checksum_high,
            checksum_low,
        ];
        self.link.execute_command(&Command {
            id: b'B',
            args,
            data: vec![],
        })?;
        Ok(())
    }

    fn command_config_get(&mut self, config_id: ConfigId) -> Result<Config, Error> {
        let data = self.link.execute_command(&Command {
            id: b'c',
            args: [config_id.into(), 0],
            data: vec![],
        })?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for config get command",
            ));
        }
        let value = u32::from_be_bytes(data[0..4].try_into().unwrap());
        Ok((config_id, value).try_into()?)
    }

    fn command_config_set(&mut self, config: Config) -> Result<(), Error> {
        self.link.execute_command(&Command {
            id: b'C',
            args: config.into(),
            data: vec![],
        })?;
        Ok(())
    }

    fn command_setting_get(&mut self, setting_id: SettingId) -> Result<Setting, Error> {
        let data = self.link.execute_command(&Command {
            id: b'a',
            args: [setting_id.into(), 0],
            data: vec![],
        })?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for setting get command",
            ));
        }
        let value = u32::from_be_bytes(data[0..4].try_into().unwrap());
        Ok((setting_id, value).try_into()?)
    }

    fn command_setting_set(&mut self, setting: Setting) -> Result<(), Error> {
        self.link.execute_command(&Command {
            id: b'A',
            args: setting.into(),
            data: vec![],
        })?;
        Ok(())
    }

    fn command_time_get(&mut self) -> Result<DateTime<Local>, Error> {
        let data = self.link.execute_command(&Command {
            id: b't',
            args: [0, 0],
            data: vec![],
        })?;
        if data.len() != 8 {
            return Err(Error::new(
                "Invalid data length received for time get command",
            ));
        }
        Ok(convert_to_datetime(&data[0..8].try_into().unwrap())?)
    }

    fn command_time_set(&mut self, datetime: DateTime<Local>) -> Result<(), Error> {
        self.link.execute_command(&Command {
            id: b'T',
            args: convert_from_datetime(datetime),
            data: vec![],
        })?;
        Ok(())
    }

    fn command_memory_read(&mut self, address: u32, length: usize) -> Result<Vec<u8>, Error> {
        let data = self.link.execute_command(&Command {
            id: b'm',
            args: [address, length as u32],
            data: vec![],
        })?;
        if data.len() != length {
            return Err(Error::new(
                "Invalid data length received for memory read command",
            ));
        }
        Ok(data)
    }

    fn command_memory_write(&mut self, address: u32, data: &[u8]) -> Result<(), Error> {
        self.link.execute_command(&Command {
            id: b'M',
            args: [address, data.len() as u32],
            data: data.to_vec(),
        })?;
        Ok(())
    }

    fn command_usb_write(&mut self, datatype: u8, data: &[u8]) -> Result<(), Error> {
        self.link.execute_command_raw(
            &Command {
                id: b'U',
                args: [datatype as u32, data.len() as u32],
                data: data.to_vec(),
            },
            true,
            false,
        )?;
        Ok(())
    }

    fn command_dd_set_block_ready(&mut self, error: bool) -> Result<(), Error> {
        self.link.execute_command(&Command {
            id: b'D',
            args: [error as u32, 0],
            data: vec![],
        })?;
        Ok(())
    }

    fn command_writeback_enable(&mut self) -> Result<(), Error> {
        self.link.execute_command(&Command {
            id: b'W',
            args: [0, 0],
            data: vec![],
        })?;
        Ok(())
    }

    fn command_flash_wait_busy(&mut self, wait: bool) -> Result<u32, Error> {
        let data = self.link.execute_command(&Command {
            id: b'p',
            args: [wait as u32, 0],
            data: vec![],
        })?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for flash wait busy command",
            ));
        }
        let erase_block_size = u32::from_be_bytes(data[0..4].try_into().unwrap());
        Ok(erase_block_size)
    }

    fn command_flash_erase_block(&mut self, address: u32) -> Result<(), Error> {
        self.link.execute_command(&Command {
            id: b'P',
            args: [address, 0],
            data: vec![],
        })?;
        Ok(())
    }

    fn command_firmware_backup(&mut self, address: u32) -> Result<(FirmwareStatus, u32), Error> {
        let data = self.link.execute_command_raw(
            &Command {
                id: b'f',
                args: [address, 0],
                data: vec![],
            },
            false,
            true,
        )?;
        if data.len() != 8 {
            return Err(Error::new(
                "Invalid data length received for firmware backup command",
            ));
        }
        let status = u32::from_be_bytes(data[0..4].try_into().unwrap());
        let length = u32::from_be_bytes(data[4..8].try_into().unwrap());
        Ok((status.try_into()?, length))
    }

    fn command_firmware_update(
        &mut self,
        address: u32,
        length: usize,
    ) -> Result<FirmwareStatus, Error> {
        let data = self.link.execute_command_raw(
            &Command {
                id: b'F',
                args: [address, length as u32],
                data: vec![],
            },
            false,
            true,
        )?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for firmware update command",
            ));
        }
        Ok(u32::from_be_bytes(data[0..4].try_into().unwrap()).try_into()?)
    }

    fn command_fpga_debug_data_get(&mut self) -> Result<FpgaDebugData, Error> {
        let data = self.link.execute_command(&Command {
            id: b'?',
            args: [0, 0],
            data: vec![],
        })?;
        Ok(data.try_into()?)
    }

    fn command_diagnostic_data_get(&mut self) -> Result<DiagnosticData, Error> {
        let data = self.link.execute_command(&Command {
            id: b'%',
            args: [0, 0],
            data: vec![],
        })?;
        Ok(data.try_into()?)
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

        reader.rewind()?;
        reader.read_exact(&mut pi_config)?;
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
            SaveType::Flashram => (SAVE_ADDRESS, FLASHRAM_LENGTH),
            SaveType::SramBanked => (SAVE_ADDRESS, SRAM_BANKED_LENGTH),
            SaveType::Sram1m => (SAVE_ADDRESS, SRAM_1M_LENGTH),
        };

        if length != save_length {
            return Err(Error::new(
                "Save file size did not match currently enabled save type",
            ));
        }

        self.memory_write_chunked(reader, address, save_length, None)
    }

    pub fn download_save<T: Write>(&mut self, writer: &mut T) -> Result<(), Error> {
        let save_type = get_config!(self, SaveType)?;

        let (address, save_length) = match save_type {
            SaveType::None => {
                return Err(Error::new("No save type is enabled"));
            }
            SaveType::Eeprom4k => (EEPROM_ADDRESS, EEPROM_4K_LENGTH),
            SaveType::Eeprom16k => (EEPROM_ADDRESS, EEPROM_16K_LENGTH),
            SaveType::Sram => (SAVE_ADDRESS, SRAM_LENGTH),
            SaveType::Flashram => (SAVE_ADDRESS, FLASHRAM_LENGTH),
            SaveType::SramBanked => (SAVE_ADDRESS, SRAM_BANKED_LENGTH),
            SaveType::Sram1m => (SAVE_ADDRESS, SRAM_1M_LENGTH),
        };

        self.memory_read_chunked(writer, address, save_length)
    }

    pub fn dump_memory<T: Write>(
        &mut self,
        writer: &mut T,
        address: u32,
        length: usize,
    ) -> Result<(), Error> {
        if address + length as u32 > MEMORY_LENGTH as u32 {
            return Err(Error::new("Invalid dump address or length"));
        }
        self.memory_read_chunked(writer, address, length)
    }

    pub fn calculate_cic_parameters(&mut self, custom_seed: Option<u8>) -> Result<(), Error> {
        let (address, cic_seed, boot_seed) = match get_config!(self, BootMode)? {
            BootMode::Menu => (BOOTLOADER_ADDRESS, None, None),
            BootMode::Rom => (BOOTLOADER_ADDRESS, None, custom_seed),
            BootMode::DdIpl => (BOOTLOADER_ADDRESS, None, custom_seed),
            BootMode::DirectRom => (SDRAM_ADDRESS, custom_seed, None),
            BootMode::DirectDdIpl => (DDIPL_ADDRESS, custom_seed, None),
        };
        let ipl3 = self.command_memory_read(address + IPL3_OFFSET, IPL3_LENGTH)?;
        let (seed, checksum) = sign_ipl3(&ipl3, cic_seed)?;
        self.command_cic_params_set(false, seed, checksum)?;
        if let Some(seed) = boot_seed {
            self.command_config_set(Config::CicSeed(CicSeed::Seed(seed)))?;
        }
        Ok(())
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
            fpga_debug_data: self.command_fpga_debug_data_get()?,
            diagnostic_data: self.command_diagnostic_data_get()?,
        })
    }

    pub fn configure_64dd(
        &mut self,
        dd_mode: DdMode,
        drive_type: Option<DdDriveType>,
    ) -> Result<(), Error> {
        self.command_config_set(Config::DdMode(dd_mode))?;
        if let Some(drive_type) = drive_type {
            self.command_config_set(Config::DdSdEnable(Switch::Off))?;
            self.command_config_set(Config::DdDriveType(drive_type))?;
            self.command_config_set(Config::DdDiskState(DdDiskState::Ejected))?;
            self.command_config_set(Config::ButtonMode(ButtonMode::UsbPacket))?;
        }
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

    pub fn set_save_writeback(&mut self, enabled: bool) -> Result<(), Error> {
        if enabled {
            self.command_writeback_enable()?;
        } else {
            let save_type = get_config!(self, SaveType)?;
            self.set_save_type(save_type)?;
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
            match packet.kind {
                DiskPacketKind::Read => {
                    self.command_memory_write(packet.info.address, &packet.info.data)?;
                }
                DiskPacketKind::Write => {}
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

    pub fn check_device(&mut self) -> Result<(), Error> {
        let identifier = self.command_identifier_get().map_err(|e| {
            Error::new(format!("Couldn't get SC64 device identifier: {e}").as_str())
        })?;
        if &identifier != SC64_V2_IDENTIFIER {
            return Err(Error::new("Unknown identifier received, not a SC64 device"));
        }
        Ok(())
    }

    pub fn check_firmware_version(&mut self) -> Result<(u16, u16, u32), Error> {
        let unsupported_version_message = format!(
            "Unsupported SC64 firmware version, minimum supported version: {}.{}.x",
            SUPPORTED_MAJOR_VERSION, SUPPORTED_MINOR_VERSION
        );
        let (major, minor, revision) = self
            .command_version_get()
            .map_err(|_| Error::new(unsupported_version_message.as_str()))?;
        if major != SUPPORTED_MAJOR_VERSION || minor < SUPPORTED_MINOR_VERSION {
            return Err(Error::new(unsupported_version_message.as_str()));
        }
        Ok((major, minor, revision))
    }

    pub fn reset_state(&mut self) -> Result<(), Error> {
        self.command_state_reset()
    }

    pub fn backup_firmware(&mut self) -> Result<Vec<u8>, Error> {
        self.command_state_reset()?;
        let (status, length) = self.command_firmware_backup(FIRMWARE_ADDRESS_SDRAM)?;
        if !matches!(status, FirmwareStatus::Ok) {
            return Err(Error::new(
                format!("Firmware backup error: {}", status).as_str(),
            ));
        }
        self.command_memory_read(FIRMWARE_ADDRESS_SDRAM, length as usize)
    }

    pub fn update_firmware(&mut self, data: &[u8], use_flash_memory: bool) -> Result<(), Error> {
        const FLASH_UPDATE_SUPPORTED_MINOR_VERSION: u16 = 19;
        let status = if use_flash_memory {
            let unsupported_version_error = Error::new(format!(
                "Your firmware doesn't support updating from Flash memory, minimum required version: {}.{}.x",
                SUPPORTED_MAJOR_VERSION, FLASH_UPDATE_SUPPORTED_MINOR_VERSION
            ).as_str());
            self.command_version_get()
                .and_then(|(major, minor, revision)| {
                    if major != SUPPORTED_MAJOR_VERSION
                        || minor < FLASH_UPDATE_SUPPORTED_MINOR_VERSION
                    {
                        return Err(unsupported_version_error.clone());
                    } else {
                        Ok((major, minor, revision))
                    }
                })
                .map_err(|_| unsupported_version_error.clone())?;
            self.command_state_reset()?;
            self.flash_erase(FIRMWARE_ADDRESS_FLASH, data.len())?;
            self.command_memory_write(FIRMWARE_ADDRESS_FLASH, data)?;
            self.command_flash_wait_busy(true)?;
            self.command_firmware_update(FIRMWARE_ADDRESS_FLASH, data.len())?
        } else {
            self.command_state_reset()?;
            self.command_memory_write(FIRMWARE_ADDRESS_SDRAM, data)?;
            self.command_firmware_update(FIRMWARE_ADDRESS_SDRAM, data.len())?
        };
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
                        "Firmware update timeout, SC64 did not finish update in {} seconds, last step: {}",
                        FIRMWARE_UPDATE_TIMEOUT.as_secs(),
                        last_update_status
                    )
                    .as_str(),
                ));
            }
            std::thread::sleep(Duration::from_millis(1));
        }
    }

    fn memory_read_chunked(
        &mut self,
        writer: &mut dyn Write,
        address: u32,
        length: usize,
    ) -> Result<(), Error> {
        let mut memory_address = address;
        let mut bytes_left = length;
        while bytes_left > 0 {
            let bytes = min(MEMORY_CHUNK_LENGTH, bytes_left);
            let data = self.command_memory_read(memory_address, bytes)?;
            writer.write_all(&data)?;
            memory_address += bytes as u32;
            bytes_left -= bytes;
        }
        Ok(())
    }

    fn memory_write_chunked(
        &mut self,
        reader: &mut dyn Read,
        address: u32,
        length: usize,
        transform: Option<fn(&mut [u8])>,
    ) -> Result<(), Error> {
        let mut limited_reader = reader.take(length as u64);
        let mut memory_address = address;
        let mut data: Vec<u8> = vec![0u8; MEMORY_CHUNK_LENGTH];
        loop {
            let bytes = limited_reader.read(&mut data)?;
            if bytes == 0 {
                break;
            }
            if let Some(transform) = transform {
                transform(&mut data[0..bytes]);
            }
            self.command_memory_write(memory_address, &data[0..bytes])?;
            memory_address += bytes as u32;
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

impl SC64 {
    pub fn open_local(port: Option<String>) -> Result<Self, Error> {
        let port = if let Some(port) = port {
            port
        } else {
            list_local_devices()?[0].port.clone()
        };
        let mut sc64 = SC64 {
            link: link::new_local(&port)?,
        };
        sc64.check_device()?;
        Ok(sc64)
    }

    pub fn open_remote(address: String) -> Result<Self, Error> {
        let mut sc64 = SC64 {
            link: link::new_remote(&address)?,
        };
        sc64.check_device()?;
        Ok(sc64)
    }
}
