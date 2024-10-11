mod cic;
mod error;
pub mod ff;
pub mod firmware;
mod ftdi;
mod link;
mod serial;
pub mod server;
mod time;
mod types;

pub use self::{
    error::Error,
    link::list_local_devices,
    server::ServerEvent,
    types::{
        AuxMessage, BootMode, ButtonMode, ButtonState, CicSeed, CicStep, DataPacket, DdDiskState,
        DdDriveType, DdMode, DebugPacket, DiagnosticData, DiskPacket, DiskPacketKind,
        FpgaDebugData, ISViewer, MemoryTestPattern, MemoryTestPatternResult, SaveType,
        SaveWriteback, SdCardInfo, SdCardOpPacket, SdCardResult, SdCardStatus, SpeedTestDirection,
        Switch, TvType,
    },
};

use self::{
    cic::{sign_ipl3, IPL3_LENGTH, IPL3_OFFSET},
    link::Link,
    time::{convert_from_datetime, convert_to_datetime},
    types::{
        get_config, get_setting, Config, ConfigId, FirmwareStatus, SdCardOp, Setting, SettingId,
        UpdateStatus,
    },
};
use chrono::NaiveDateTime;
use rand::Rng;
use std::{
    cmp::min,
    io::{Read, Seek, Write},
    thread::sleep,
    time::{Duration, Instant},
};

pub struct SC64 {
    link: Link,
}

pub struct DeviceState {
    pub bootloader_switch: Switch,
    pub rom_write_enable: Switch,
    pub rom_shadow_enable: Switch,
    pub dd_mode: DdMode,
    pub isviewer: ISViewer,
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
    pub sd_card_status: SdCardStatus,
    pub datetime: NaiveDateTime,
    pub fpga_debug_data: FpgaDebugData,
    pub diagnostic_data: DiagnosticData,
}

const SC64_V2_IDENTIFIER: &[u8; 4] = b"SCv2";

const SUPPORTED_MAJOR_VERSION: u16 = 2;
const SUPPORTED_MINOR_VERSION: u16 = 20;

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

const SD_CARD_BUFFER_ADDRESS: u32 = 0x03FE_0000; // Arbitrary offset in SDRAM memory
const SD_CARD_BUFFER_LENGTH: usize = 128 * 1024; // Arbitrary length in SDRAM memory

pub const SD_CARD_SECTOR_SIZE: usize = 512;

const FIRMWARE_ADDRESS_SDRAM: u32 = 0x0010_0000; // Arbitrary offset in SDRAM memory
const FIRMWARE_ADDRESS_FLASH: u32 = 0x0410_0000; // Arbitrary offset in Flash memory
const FIRMWARE_UPDATE_TIMEOUT: Duration = Duration::from_secs(90);

const ISV_BUFFER_LENGTH: usize = 64 * 1024;

pub const MEMORY_LENGTH: usize = 0x0500_2C80;

const MEMORY_CHUNK_LENGTH: usize = 1 * 1024 * 1024;

impl SC64 {
    fn command_identifier_get(&mut self) -> Result<[u8; 4], Error> {
        let data = self.link.execute_command(b'v', [0, 0], &[])?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for identifier get command",
            ));
        }
        Ok(data[0..4].try_into().unwrap())
    }

    fn command_version_get(&mut self) -> Result<(u16, u16, u32), Error> {
        let data = self.link.execute_command(b'V', [0, 0], &[])?;
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
        self.link.execute_command(b'R', [0, 0], &[])?;
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
        self.link.execute_command(b'B', args, &[])?;
        Ok(())
    }

    fn command_config_get(&mut self, config_id: ConfigId) -> Result<Config, Error> {
        let data = self
            .link
            .execute_command(b'c', [config_id.into(), 0], &[])?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for config get command",
            ));
        }
        let value = u32::from_be_bytes(data[0..4].try_into().unwrap());
        Ok((config_id, value).try_into()?)
    }

    fn command_config_set(&mut self, config: Config) -> Result<(), Error> {
        self.link.execute_command(b'C', config.into(), &[])?;
        Ok(())
    }

    fn command_setting_get(&mut self, setting_id: SettingId) -> Result<Setting, Error> {
        let data = self
            .link
            .execute_command(b'a', [setting_id.into(), 0], &[])?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for setting get command",
            ));
        }
        let value = u32::from_be_bytes(data[0..4].try_into().unwrap());
        Ok((setting_id, value).try_into()?)
    }

    fn command_setting_set(&mut self, setting: Setting) -> Result<(), Error> {
        self.link.execute_command(b'A', setting.into(), &[])?;
        Ok(())
    }

    fn command_time_get(&mut self) -> Result<NaiveDateTime, Error> {
        let data = self.link.execute_command(b't', [0, 0], &[])?;
        if data.len() != 8 {
            return Err(Error::new(
                "Invalid data length received for time get command",
            ));
        }
        Ok(convert_to_datetime(&data[0..8].try_into().unwrap())?)
    }

    fn command_time_set(&mut self, datetime: NaiveDateTime) -> Result<(), Error> {
        self.link
            .execute_command(b'T', convert_from_datetime(datetime), &[])?;
        Ok(())
    }

    fn command_memory_read(&mut self, address: u32, length: usize) -> Result<Vec<u8>, Error> {
        let data = self
            .link
            .execute_command(b'm', [address, length as u32], &[])?;
        if data.len() != length {
            return Err(Error::new(
                "Invalid data length received for memory read command",
            ));
        }
        Ok(data)
    }

    fn command_memory_write(&mut self, address: u32, data: &[u8]) -> Result<(), Error> {
        self.link
            .execute_command(b'M', [address, data.len() as u32], data)?;
        Ok(())
    }

    fn command_usb_write(&mut self, datatype: u8, data: &[u8]) -> Result<(), Error> {
        self.link.execute_command_raw(
            b'U',
            [datatype as u32, data.len() as u32],
            data,
            true,
            false,
        )?;
        Ok(())
    }

    fn command_aux_write(&mut self, data: u32) -> Result<(), Error> {
        self.link.execute_command(b'X', [data, 0], &[])?;
        Ok(())
    }

    fn command_sd_card_operation(&mut self, op: SdCardOp) -> Result<SdCardOpPacket, Error> {
        let data = self
            .link
            .execute_command_raw(b'i', op.into(), &[], false, true)?;
        if data.len() != 8 {
            return Err(Error::new(
                "Invalid data length received for SD card operation command",
            ));
        }
        Ok(SdCardOpPacket {
            result: data.clone().try_into()?,
            status: data.clone().try_into()?,
        })
    }

    fn command_sd_card_read(
        &mut self,
        address: u32,
        sector: u32,
        count: u32,
    ) -> Result<SdCardResult, Error> {
        let data = self.link.execute_command_raw(
            b's',
            [address, count],
            &sector.to_be_bytes(),
            false,
            true,
        )?;
        Ok(data.try_into()?)
    }

    fn command_sd_card_write(
        &mut self,
        address: u32,
        sector: u32,
        count: u32,
    ) -> Result<SdCardResult, Error> {
        let data = self.link.execute_command_raw(
            b'S',
            [address, count],
            &sector.to_be_bytes(),
            false,
            true,
        )?;
        Ok(data.try_into()?)
    }

    fn command_dd_set_block_ready(&mut self, error: bool) -> Result<(), Error> {
        self.link.execute_command(b'D', [error as u32, 0], &[])?;
        Ok(())
    }

    fn command_writeback_enable(&mut self) -> Result<(), Error> {
        self.link.execute_command(b'W', [0, 0], &[])?;
        Ok(())
    }

    fn command_flash_wait_busy(&mut self, wait: bool) -> Result<u32, Error> {
        let data = self.link.execute_command(b'p', [wait as u32, 0], &[])?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for flash wait busy command",
            ));
        }
        let erase_block_size = u32::from_be_bytes(data[0..4].try_into().unwrap());
        Ok(erase_block_size)
    }

    fn command_flash_erase_block(&mut self, address: u32) -> Result<(), Error> {
        self.link.execute_command(b'P', [address, 0], &[])?;
        Ok(())
    }

    fn command_firmware_backup(&mut self, address: u32) -> Result<(FirmwareStatus, u32), Error> {
        let data = self
            .link
            .execute_command_raw(b'f', [address, 0], &[], false, true)?;
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
        let data =
            self.link
                .execute_command_raw(b'F', [address, length as u32], &[], false, true)?;
        if data.len() != 4 {
            return Err(Error::new(
                "Invalid data length received for firmware update command",
            ));
        }
        Ok(u32::from_be_bytes(data[0..4].try_into().unwrap()).try_into()?)
    }

    fn command_fpga_debug_data_get(&mut self) -> Result<FpgaDebugData, Error> {
        let data = self.link.execute_command(b'?', [0, 0], &[])?;
        Ok(data.try_into()?)
    }

    fn command_diagnostic_data_get(&mut self) -> Result<DiagnosticData, Error> {
        let data = self.link.execute_command(b'%', [0, 0], &[])?;
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

    pub fn get_datetime(&mut self) -> Result<NaiveDateTime, Error> {
        self.command_time_get()
    }

    pub fn set_datetime(&mut self, datetime: NaiveDateTime) -> Result<(), Error> {
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
            isviewer: get_config!(self, ISViewer)?,
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
            sd_card_status: self.get_sd_card_status()?,
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
            self.command_config_set(Config::ISViewer(ISViewer::Enabled(offset)))?;
        } else {
            self.command_config_set(Config::RomWriteEnable(Switch::Off))?;
            self.command_config_set(Config::ISViewer(ISViewer::Disabled))?;
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

    pub fn send_aux_packet(&mut self, data: AuxMessage) -> Result<(), Error> {
        self.command_aux_write(data.into())
    }

    pub fn send_and_receive_aux_packet(
        &mut self,
        data: AuxMessage,
        timeout: std::time::Duration,
    ) -> Result<Option<AuxMessage>, Error> {
        self.send_aux_packet(data)?;
        let reply_timeout = std::time::Instant::now();
        loop {
            match self.receive_data_packet()? {
                Some(packet) => match packet {
                    DataPacket::AuxData(data) => {
                        return Ok(Some(data));
                    }
                    _ => {}
                },
                None => {}
            }
            if reply_timeout.elapsed() > timeout {
                return Ok(None);
            }
        }
    }

    pub fn try_notify_via_aux(&mut self, message: AuxMessage) -> Result<bool, Error> {
        let timeout = std::time::Duration::from_millis(500);
        if let Some(response) = self.send_and_receive_aux_packet(message, timeout)? {
            return Ok(message == response);
        }
        Ok(false)
    }

    pub fn init_sd_card(&mut self) -> Result<SdCardResult, Error> {
        self.command_sd_card_operation(SdCardOp::Init)
            .map(|packet| packet.result)
    }

    pub fn deinit_sd_card(&mut self) -> Result<SdCardResult, Error> {
        self.command_sd_card_operation(SdCardOp::Deinit)
            .map(|packet| packet.result)
    }

    pub fn get_sd_card_status(&mut self) -> Result<SdCardStatus, Error> {
        self.command_sd_card_operation(SdCardOp::GetStatus)
            .map(|reply| reply.status)
    }

    pub fn get_sd_card_info(&mut self) -> Result<SdCardInfo, Error> {
        const SD_CARD_INFO_BUFFER_ADDRESS: u32 = 0x0500_2BE0;
        let info =
            match self.command_sd_card_operation(SdCardOp::GetInfo(SD_CARD_INFO_BUFFER_ADDRESS))? {
                SdCardOpPacket {
                    result: SdCardResult::OK,
                    status: _,
                } => self.command_memory_read(SD_CARD_INFO_BUFFER_ADDRESS, 32)?,
                packet => {
                    return Err(Error::new(
                        format!("Couldn't get SD card info registers: {}", packet.result).as_str(),
                    ))
                }
            };
        Ok(info.try_into()?)
    }

    pub fn read_sd_card(&mut self, data: &mut [u8], sector: u32) -> Result<SdCardResult, Error> {
        if data.len() % SD_CARD_SECTOR_SIZE != 0 {
            return Err(Error::new(
                "SD card read length not aligned to the sector size",
            ));
        }

        let mut current_sector = sector;

        for mut chunk in data.chunks_mut(SD_CARD_BUFFER_LENGTH) {
            let sectors = (chunk.len() / SD_CARD_SECTOR_SIZE) as u32;
            match self.command_sd_card_read(SD_CARD_BUFFER_ADDRESS, current_sector, sectors)? {
                SdCardResult::OK => {}
                result => return Ok(result),
            }
            let data = self.command_memory_read(SD_CARD_BUFFER_ADDRESS, chunk.len())?;
            chunk.write_all(&data)?;
            current_sector += sectors;
        }

        Ok(SdCardResult::OK)
    }

    pub fn write_sd_card(&mut self, data: &[u8], sector: u32) -> Result<SdCardResult, Error> {
        if data.len() % SD_CARD_SECTOR_SIZE != 0 {
            return Err(Error::new(
                "SD card write length not aligned to the sector size",
            ));
        }

        let mut current_sector = sector;

        for chunk in data.chunks(SD_CARD_BUFFER_LENGTH) {
            let sectors = (chunk.len() / SD_CARD_SECTOR_SIZE) as u32;
            self.command_memory_write(SD_CARD_BUFFER_ADDRESS, chunk)?;
            match self.command_sd_card_write(SD_CARD_BUFFER_ADDRESS, current_sector, sectors)? {
                SdCardResult::OK => {}
                result => return Ok(result),
            }
            current_sector += sectors;
        }

        Ok(SdCardResult::OK)
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

    pub fn is_console_powered_on(&mut self) -> Result<bool, Error> {
        let debug_data = self.command_fpga_debug_data_get()?;
        Ok(match debug_data.cic_step {
            CicStep::Unavailable | CicStep::PowerOff => false,
            _ => true,
        })
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

    pub fn test_usb_speed(&mut self, direction: SpeedTestDirection) -> Result<f64, Error> {
        const TEST_ADDRESS: u32 = SDRAM_ADDRESS;
        const TEST_LENGTH: usize = 8 * 1024 * 1024;
        const MIB_DIVIDER: f64 = 1024.0 * 1024.0;

        let data = vec![0x00; TEST_LENGTH];

        let time = std::time::Instant::now();

        match direction {
            SpeedTestDirection::Read => {
                self.command_memory_read(TEST_ADDRESS, TEST_LENGTH)?;
            }
            SpeedTestDirection::Write => {
                self.command_memory_write(TEST_ADDRESS, &data)?;
            }
        }

        let elapsed = time.elapsed();

        Ok((TEST_LENGTH as f64 / MIB_DIVIDER) / elapsed.as_secs_f64())
    }

    pub fn test_sd_card(&mut self) -> Result<f64, Error> {
        const TEST_LENGTH: usize = 4 * 1024 * 1024;
        const MIB_DIVIDER: f64 = 1024.0 * 1024.0;

        let mut data = vec![0x00; TEST_LENGTH];

        match self.init_sd_card()? {
            SdCardResult::OK => {}
            result => {
                return Err(Error::new(
                    format!("Init SD card failed: {result}").as_str(),
                ))
            }
        }

        let time = std::time::Instant::now();

        match self.read_sd_card(&mut data, 0)? {
            SdCardResult::OK => {}
            result => {
                return Err(Error::new(
                    format!("Read SD card failed: {result}").as_str(),
                ))
            }
        }

        let elapsed = time.elapsed();

        match self.deinit_sd_card()? {
            SdCardResult::OK => {}
            result => {
                return Err(Error::new(
                    format!("Deinit SD card failed: {result}").as_str(),
                ))
            }
        }

        Ok((TEST_LENGTH as f64 / MIB_DIVIDER) / elapsed.as_secs_f64())
    }

    pub fn test_sdram_pattern(
        &mut self,
        pattern: MemoryTestPattern,
        fade: Option<u64>,
    ) -> Result<MemoryTestPatternResult, Error> {
        let item_size = std::mem::size_of::<u32>();
        let mut test_data = vec![0u32; SDRAM_LENGTH / item_size];

        match pattern {
            MemoryTestPattern::OwnAddress(inverted) => {
                for (index, item) in test_data.iter_mut().enumerate() {
                    let mut value = (index * item_size) as u32;
                    if inverted {
                        value ^= 0xFFFFFFFFu32;
                    }
                    *item = value;
                }
            }
            MemoryTestPattern::AllZeros => test_data.fill(0x00000000u32),
            MemoryTestPattern::AllOnes => test_data.fill(0xFFFFFFFFu32),
            MemoryTestPattern::Custom(pattern) => test_data.fill(pattern),
            MemoryTestPattern::Random => rand::thread_rng().fill(&mut test_data[..]),
        };

        let raw_test_data: Vec<u8> = test_data.iter().flat_map(|v| v.to_be_bytes()).collect();
        let mut writer: &[u8] = &raw_test_data;
        self.memory_write_chunked(&mut writer, SDRAM_ADDRESS, SDRAM_LENGTH, None)?;

        if let Some(fade) = fade {
            sleep(Duration::from_secs(fade));
        }

        let mut raw_check_data = vec![0u8; SDRAM_LENGTH];
        let mut reader: &mut [u8] = &mut raw_check_data;
        self.memory_read_chunked(&mut reader, SDRAM_ADDRESS, SDRAM_LENGTH)?;
        let check_data = raw_check_data
            .chunks(4)
            .map(|a| u32::from_be_bytes(a[0..4].try_into().unwrap()));

        let all_errors: Vec<(usize, (u32, u32))> = test_data
            .into_iter()
            .zip(check_data)
            .enumerate()
            .filter(|(_, (a, b))| a != b)
            .map(|(i, (a, b))| (i * item_size, (a, b)))
            .collect();

        let first_error = if all_errors.len() > 0 {
            Some(all_errors.get(0).copied().unwrap())
        } else {
            None
        };

        return Ok(MemoryTestPatternResult {
            first_error,
            all_errors,
        });
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
        let mut sc64 = SC64 {
            link: link::new_local(&port.unwrap_or(list_local_devices()?[0].port.clone()))?,
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
